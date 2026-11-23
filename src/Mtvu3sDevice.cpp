// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#ifdef _WIN32

#include "Mtvu3sDevice.hpp"

#include <chrono>
#include <thread>

namespace friio {

namespace {

inline void SleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool MatchTSID(uint16_t want, const std::vector<uint16_t>& ids, uint16_t& matched) {
    if (want == 0) { matched = 0; return true; }
    for (uint16_t id : ids) {
        if (id == want) { matched = id; return true; }
    }
    if ((want & 0xf000) == 0x4000) {
        for (uint16_t id : ids) {
            if ((id & 0xf000) == 0x4000 && (id & ~0x0e00u) == (want & ~0x0e00u)) {
                matched = id;
                return true;
            }
        }
    }
    if ((want & 0xe000) == 0x6000) {
        for (uint16_t id : ids) {
            if ((id & 0xe000) == 0x6000 && (id & ~0x1000u) == (want & ~0x1000u)) {
                matched = id;
                return true;
            }
        }
    }
    return false;
}

} // namespace

void Mtvu3sDevice::Em28178Bus::write(uint8_t addr, const uint8_t* data, uint16_t len, bool stop) {
    std::lock_guard<std::mutex> lk(dev_->io_mu_);
    dev_->usb_.ControlTransfer(0x40, stop ? 0x02 : 0x03, 0, addr,
                               const_cast<uint8_t*>(data), len, 1000);
}

void Mtvu3sDevice::Em28178Bus::read(uint8_t addr, uint8_t* data, uint16_t len, bool stop) {
    std::lock_guard<std::mutex> lk(dev_->io_mu_);
    dev_->usb_.ControlTransfer(0xc0, stop ? 0x02 : 0x03, 0, addr, data, len, 1000);
}

bool Mtvu3sDevice::WriteReg(uint8_t reg, uint8_t val) {
    std::lock_guard<std::mutex> lk(io_mu_);
    return usb_.ControlTransfer(0x40, 0x00, 0, reg, &val, 1, 1000);
}

bool Mtvu3sDevice::ReadReg(uint8_t reg, uint8_t& val) {
    std::lock_guard<std::mutex> lk(io_mu_);
    size_t got = 0;
    return usb_.ControlTransfer(0xc0, 0x00, 0, reg, &val, 1, 1000, &got) && got == 1;
}

bool Mtvu3sDevice::Open(int device_index) {
    auto paths = WinUsbDevice::Enumerate(kMtvu3sVid, kMtvu3sPid);
    int matched = 0;
    bool opened = false;
    for (const auto& p : paths) {
        if (!usb_.Open(p)) continue;
        if (matched == device_index) {
            opened = true;
            break;
        }
        usb_.Close();
        ++matched;
    }
    if (!opened) return false;
    usb_.SetAlternateSetting(0);

    bus_ = std::make_unique<Em28178Bus>(this);
    ter_ = std::make_unique<Tc90522>(*bus_, 0x20);
    sat_ = std::make_unique<Tc90522>(*bus_, 0x22);
    ter_tuner_ = std::make_unique<Fc0013>(ter_->tunerBus(), 0xc6);
    sat_tuner_ = std::make_unique<Qm1d1c0042>(sat_->tunerBus(), 0xc0);
    const uint8_t zero_key[16]{};
    aes_.setKey(zero_key);

    Mtvu3sCardIO card_io;
    card_io.read_reg = [this](uint8_t reg, uint8_t& val) { return ReadReg(reg, val); };
    card_io.write_reg = [this](uint8_t reg, uint8_t val) { return WriteReg(reg, val); };
    card_io.control = [this](uint8_t req_type, uint8_t req, uint16_t val, uint16_t idx,
                             uint8_t* data, uint16_t len, uint32_t timeout_ms, size_t* transferred) {
        std::lock_guard<std::mutex> lk(io_mu_);
        return usb_.ControlTransfer(req_type, req, val, idx, data, len, timeout_ms, transferred);
    };
    card_io.sleep_ms = [](int ms) { SleepMs(ms); };
    card_ = std::make_unique<Mtvu3sCardReader>(std::move(card_io));
    return true;
}

bool Mtvu3sDevice::Init() {
    if (!WriteReg(0x0c, 0x10)) return false;
    if (!WriteReg(0x06, 0x44)) return false;
    if (!WriteReg(0x80, 0x7c)) return false;
    SleepMs(50);
    if (!WriteReg(0x80, 0xfd)) return false;
    if (!WriteReg(0x73, 0xfd)) return false;
    SleepMs(50);

    ter_->initTer();
    sat_->initSat();
    sat_tuner_->init();
    ter_tuner_->init();

    for (uint8_t i = 0; i < 16; ++i) {
        if (!WriteReg(uint8_t(0x60 + i), 0x00)) return false;
    }
    return WriteReg(0x5d, 0xef);
}

bool Mtvu3sDevice::TuneUHF(uint32_t freq_khz) {
    band_ = Band::UHF;
    if (!WriteReg(0x50, 0x01)) return false;
    sat_->setSatActive(false);
    ter_->setTerActive(true);
    ter_->holdAgcForTune(true);
    ter_tuner_->tune(freq_khz, 6);
    ter_->holdAgcForTune(false);
    SleepMs(50);
    ter_->resetTer();
    waitUntil([this]() {
        ter_tuner_->adjustLna();
        return ter_->isTerLocked();
    }, 30, 100);
    return true;
}

bool Mtvu3sDevice::TuneSat(uint32_t freq_khz, uint16_t tsid) {
    band_ = (tsid & 0xf000) == 0x4000 ? Band::BS : Band::CS;
    ter_->setTerActive(false);
    sat_->setSatActive(true);
    sat_tuner_->tune(freq_khz);
    sat_->resetSat();
    waitUntil([this]() { return sat_->isSatTmccLocked(); }, 30, 100);
    sat_->selectSatTsid(tsid);
    waitUntil([this]() { return sat_->isSatSlotLocked(); }, 30, 100);
    return true;
}

bool Mtvu3sDevice::GetCNR(float& cnr_db) {
    if (band_ == Band::UHF) {
        cnr_db = ter_->getTerCnr();
    } else {
        cnr_db = sat_->getSatCnr();
    }
    return true;
}

bool Mtvu3sDevice::StartStream(WinUsbDevice::ChunkCallback cb) {
    if (!usb_.SetAlternateSetting(1)) return false;
    if (!WriteReg(0x5d, 0xef)) return false;
    if (!WriteReg(0x5f, 0x89)) return false;
    // 1 URB = 64 microframes (8 ms) x 32 URBs = 256 ms リング（WinUSB / xHCI で安定しやすい粒度）
    return usb_.StartAsyncIsochStream(0x84, 32, 940, 64, std::move(cb));
}

void Mtvu3sDevice::StopStream() {
    usb_.StopAsyncStream();
    Idle();
}

void Mtvu3sDevice::Idle() {
    WriteReg(0x5f, 0x08);
    usb_.SetAlternateSetting(0);
}

void Mtvu3sDevice::DescramblePacket(uint8_t pkt[188]) const {
    for (int b = 0; b < 11; ++b) {
        aes_.decryptBlock(pkt + 12 + b * 16);
    }
}

bool Mtvu3sDevice::ResetBcas() {
    std::lock_guard<std::mutex> lk(bcas_mu_);
    if (!card_ || !usb_.IsOpen()) return false;
    return card_->Reset();
}

bool Mtvu3sDevice::TransmitBcas(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp_out) {
    std::lock_guard<std::mutex> lk(bcas_mu_);
    if (!card_ || !usb_.IsOpen()) return false;
    return card_->Transmit(apdu, len, resp_out);
}

void Mtvu3sDevice::Close() {
    if (!usb_.IsOpen()) return;
    StopStream();
    {
        std::lock_guard<std::mutex> lk(bcas_mu_);
        card_.reset();
    }
    usb_.Close();
}

} // namespace friio

#endif // _WIN32
