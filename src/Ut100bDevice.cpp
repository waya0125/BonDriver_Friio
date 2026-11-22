// SPDX-License-Identifier: BSD-2-Clause AND MIT
// Copyright (c) 2026 waya0125
#ifdef _WIN32

#include "Ut100bDevice.hpp"

#include <chrono>
#include <cmath>
#include <thread>

namespace friio {

namespace {

constexpr uint8_t kI2cDemod = 0x20;
constexpr uint8_t kI2cTuner = 0xc0;
constexpr uint8_t kI2cCardHDUS = 0x5a;
constexpr uint8_t kI2cCardHDP = 0x4a;

constexpr uint8_t kDemodInit[][2] = {
    {0x01, 0x00}, {0x02, 0x00}, {0x03, 0x00}, {0x04, 0x00}, {0x05, 0x00}, {0x06, 0x00}, {0x07, 0x00}, {0x08, 0x00},
    {0x0c, 0x00}, {0x11, 0x21}, {0x12, 0x10}, {0x13, 0x03}, {0x14, 0xe0}, {0x15, 0x42}, {0x16, 0x09}, {0x17, 0x20},
    {0x18, 0x29}, {0x19, 0x13}, {0x1a, 0x29}, {0x1b, 0x13}, {0x1c, 0x2a}, {0x1d, 0xaa}, {0x1e, 0xaa}, {0x1f, 0xa8},
    {0x20, 0x00}, {0x21, 0xff}, {0x22, 0x80}, {0x23, 0x4c}, {0x24, 0x4c}, {0x25, 0x80}, {0x26, 0x00}, {0x27, 0x0c},
    {0x28, 0x60}, {0x29, 0x6b}, {0x2a, 0x40}, {0x2b, 0x40}, {0x2c, 0xff}, {0x2d, 0x00}, {0x2e, 0xff}, {0x2f, 0x00},
    {0x30, 0x20}, {0x31, 0x0f}, {0x32, 0x84}, {0x34, 0x0f}, {0x38, 0x00}, {0x39, 0x0f}, {0x3a, 0x20}, {0x3b, 0x21},
    {0x3c, 0x3f}, {0x3d, 0x10}, {0x3e, 0x08}, {0x3f, 0x0c}, {0x40, 0x0c}, {0x41, 0x00}, {0x42, 0x00}, {0x43, 0x4f},
    {0x44, 0xff}, {0x46, 0x20}, {0x47, 0x00}, {0x48, 0x90}, {0x49, 0xe6}, {0x4a, 0x02}, {0x4b, 0x54}, {0x4c, 0x00},
    {0x50, 0x04}, {0x51, 0x58}, {0x52, 0x20}, {0x54, 0x57}, {0x55, 0xf1}, {0x56, 0x20}, {0x57, 0x70}, {0x58, 0x60},
    {0x5c, 0x50}, {0x5d, 0x00}, {0x5e, 0x01}, {0x5f, 0x80}, {0x70, 0x18}, {0x71, 0x00}, {0x72, 0x00}, {0x75, 0x22},
    {0x76, 0x00}, {0x77, 0x01}, {0x7c, 0x00}, {0x7d, 0x52}, {0xba, 0x00}, {0xbb, 0x00}, {0xbc, 0x00}, {0xc2, 0x10},
    {0xc7, 0x00}, {0xe4, 0x1a}, {0xe6, 0x2f}, {0xe9, 0x08}, {0xec, 0x00}, {0xef, 0x00},
};

constexpr uint8_t kTunerInit[][2] = {
    {0x2c, 0x44}, {0x4d, 0x40}, {0x7f, 0x02}, {0x9a, 0x52}, {0x48, 0x5a}, {0x76, 0x1a},
    {0x6a, 0x48}, {0x64, 0x28}, {0x66, 0xe6}, {0x35, 0x11}, {0x7e, 0x01}, {0x0b, 0x99},
    {0x0c, 0x00}, {0x10, 0x40}, {0x12, 0xca}, {0x16, 0x90}, {0x32, 0x36}, {0xd8, 0x18},
    {0x05, 0x01},
};

inline void SleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline uint8_t Reverse8(uint8_t b) {
    b = uint8_t(((b & 0xf0) >> 4) | ((b & 0x0f) << 4));
    b = uint8_t(((b & 0xcc) >> 2) | ((b & 0x33) << 2));
    b = uint8_t(((b & 0xaa) >> 1) | ((b & 0x55) << 1));
    return b;
}

std::vector<uint8_t> FrameT1(const std::vector<uint8_t>& block) {
    std::vector<uint8_t> out;
    out.reserve(block.size() + 1);
    uint8_t lrc = 0;
    for (uint8_t b : block) {
        uint8_t r = Reverse8(b);
        out.push_back(r);
        lrc ^= r;
    }
    out.push_back(lrc);
    return out;
}

} // namespace

bool Ut100bDevice::Open(int device_index) {
    auto paths = WinUsbDevice::Enumerate(kUt100bVid, kUt100bPid);
    int matched = 0;
    for (const auto& p : paths) {
        if (!usb_.Open(p)) continue;
        if (matched == device_index) {
            usb_.SetAlternateSetting(0);
            return true;
        }
        usb_.Close();
        ++matched;
    }
    return false;
}

bool Ut100bDevice::Send(uint8_t req, uint16_t val, uint16_t idx, std::vector<uint8_t>& out, uint16_t len) {
    std::lock_guard<std::mutex> lk(io_mu_);
    out.assign(len, 0);
    size_t got = 0;
    if (!usb_.ControlTransfer(0xc0, req, val, idx, out.data(), len, 1000, &got)) {
        return false;
    }
    out.resize(got);
    return true;
}

bool Ut100bDevice::RegRead(uint8_t addr, uint8_t reg, uint8_t& val, bool& ok) {
    std::vector<uint8_t> d;
    if (!Send(0x02, uint16_t((uint16_t(reg) << 8) | addr), 0, d, 2) || d.size() < 2) return false;
    ok = (d[0] == 1);
    val = d[1];
    return true;
}

bool Ut100bDevice::RegWrite(uint8_t addr, uint8_t reg, uint8_t val, bool& ok) {
    std::vector<uint8_t> d;
    if (!Send(0x03, uint16_t((uint16_t(reg) << 8) | addr), val, d, 2)) return false;
    ok = (!d.empty() && d[0] == 1);
    return true;
}

bool Ut100bDevice::Gpio(uint8_t mask, uint8_t val) {
    std::vector<uint8_t> d;
    return Send(0x08, uint16_t((uint16_t(mask) << 8) | val), 0, d, 1);
}

bool Ut100bDevice::GpioEx(uint8_t mask, uint8_t val) {
    std::vector<uint8_t> d;
    return Send(0x10, uint16_t((uint16_t(mask) << 8) | val), 0, d, 1);
}

bool Ut100bDevice::I2cWrite(uint8_t addr, const uint8_t* data, size_t len) {
    std::vector<uint8_t> d;
    size_t off = 0;
    while (len - off >= 3) {
        if (!Send(0x0d, uint16_t((uint16_t(data[off]) << 8) | off),
                  uint16_t((uint16_t(data[off + 2]) << 8) | data[off + 1]), d, 4)) return false;
        off += 3;
    }
    if (len - off == 2) {
        if (!Send(0x0d, uint16_t((uint16_t(data[off]) << 8) | off), data[off + 1], d, 3)) return false;
        off += 2;
    } else if (len - off == 1) {
        if (!Send(0x0d, uint16_t((uint16_t(data[off]) << 8) | off), 0, d, 2)) return false;
        off += 1;
    }
    return Send(0x0e, addr, 0, d, static_cast<uint16_t>(off + 1));
}

bool Ut100bDevice::TunerSend(const uint8_t* data, size_t len) {
    std::vector<uint8_t> buf;
    buf.reserve(len + 2);
    buf.push_back(0xfe);
    buf.push_back(kI2cTuner);
    buf.insert(buf.end(), data, data + len);
    return I2cWrite(kI2cDemod, buf.data(), buf.size());
}

bool Ut100bDevice::Init() {
    std::vector<uint8_t> d;
    bool ok = false;
    uint8_t v = 0;
    uint8_t ts_mode = 0x91;
    card_addr_ = kI2cCardHDUS;

    if (!Send(0x0c, 0, 0, d, 0x3a)) return false;
    if (!Send(0x17, 0x0002, 0, d, 3)) return false;
    if (!Gpio(0xfb, 0x07) || !GpioEx(0x02, 0x00)) return false;

    // 電源 OFF -> ON
    if (!Gpio(0xd4, 0x00) || !Gpio(0x08, 0x08)) return false;
    SleepMs(50);
    if (!Send(0x17, 0x0002, 0, d, 3)) return false;

    if (!Gpio(0x08, 0x08)) return false;
    SleepMs(50);
    if (!Gpio(0x08, 0x00)) return false;
    SleepMs(150);
    if (!Gpio(0x11, 0xff)) return false;
    SleepMs(50);
    if (!Gpio(0xd5, 0x00)) return false;
    SleepMs(50);
    if (!Gpio(0xd5, 0xff)) return false;
    SleepMs(100);

    uint8_t rst = 0xff;
    if (!TunerSend(&rst, 1)) return false;
    for (const auto& rv : kDemodInit) {
        if (!RegWrite(kI2cDemod, rv[0], rv[1], ok)) return false;
    }
    if (!RegRead(kI2cDemod, 0x1c, v, ok)) return false;
    if (!RegWrite(kI2cDemod, 0x1c, uint8_t((v & 0xeb) | 0x28), ok)) return false;
    for (const auto& rv : kTunerInit) {
        uint8_t pair[2] = {rv[0], rv[1]};
        if (!TunerSend(pair, 2)) return false;
    }

    // ASIE5606 リセット
    if (!Gpio(0x11, 0xff)) return false;
    SleepMs(50);
    if (!Gpio(0x11, 0x00)) return false;
    SleepMs(50);
    if (!Gpio(0x11, 0xff)) return false;
    SleepMs(100);

    uint8_t identity = 0;
    if (!RegRead(card_addr_, 0x09, identity, ok)) return false;
    if (!ok) {
        card_addr_ = kI2cCardHDP;
        if (!RegRead(card_addr_, 0x09, identity, ok)) return false;
    }
    if ((identity & 7) == 6) ts_mode = 0x80; // DES

    if (!Send(0x0a, 0, 0, d, 1)) return false;
    SleepMs(30);

    if (!RegWrite(card_addr_, 0x05, ts_mode, ok)) return false;
    for (uint8_t reg = 0x10; reg < 0x20; ++reg) {
        if (!RegWrite(card_addr_, reg, 0x00, ok)) return false;
    }
    if (ts_mode != 0x80) return false;
    open_ = true;
    return true;
}

bool Ut100bDevice::ChannelToFreqMHz(int ch, bool catv, int& freq_mhz) {
    if (!catv) {
        if (ch < 13 || ch > 62) return false;
        freq_mhz = 473 + (ch - 13) * 6;
        return true;
    }
    if (ch < 13 || ch > 63) return false;
    if (ch <= 22) freq_mhz = 111 + (ch - 13) * 6;
    else if (ch == 23) freq_mhz = 225;
    else if (ch <= 27) freq_mhz = 233 + (ch - 24) * 6;
    else freq_mhz = 225 + (ch - 23) * 6;
    return true;
}

bool Ut100bDevice::Tune(int freq_mhz) {
    if (!open_) return false;
    bool ok = false;
    uint8_t v = 0;
    std::vector<uint8_t> d;
    const uint16_t code = static_cast<uint16_t>(freq_mhz * 64);

    if (!RegRead(kI2cDemod, 0x1c, v, ok)) return false;
    if (!RegWrite(kI2cDemod, 0x1c, uint8_t((v & 0xfb) | 0x38), ok)) return false;
    if (!RegWrite(kI2cDemod, 0x25, 0x00, ok)) return false;
    if (!RegWrite(kI2cDemod, 0x23, 0x4d, ok)) return false;
    if (!RegWrite(kI2cDemod, 0x1e, 0xaa, ok)) return false;
    if (!RegWrite(kI2cDemod, 0x1f, 0xa8, ok)) return false;

    const uint8_t t1[2] = {0x11, 0x00};
    const uint8_t t2[2] = {0x13, 0x15};
    const uint8_t t3[2] = {0x14, uint8_t(code & 0xff)};
    const uint8_t t4[2] = {0x15, uint8_t(code >> 8)};
    const uint8_t t5[2] = {0x11, 0x02};
    if (!TunerSend(t1, 2) || !TunerSend(t2, 2) || !TunerSend(t3, 2) || !TunerSend(t4, 2) || !TunerSend(t5, 2)) return false;

    if (!RegWrite(kI2cDemod, 0x1e, 0xa2, ok)) return false;
    if (!RegWrite(kI2cDemod, 0x1f, 0x08, ok)) return false;
    if (!RegWrite(kI2cDemod, 0x01, 0x40, ok)) return false;
    if (!RegWrite(kI2cDemod, 0x23, 0x4c, ok)) return false;

    if (!Send(0x09, 0x0100, 0, d, 1)) return false;
    if (!Send(0x04, 0x0040, 0, d, 2) || d.size() < 2) return false;
    if (!Send(0x05, uint16_t((uint16_t(d[1] | 0x0f) << 8) | 0x40), 0, d, 2)) return false;
    if (!Send(0x05, uint16_t((0x1fff & 0xff00) | 0x41), 0x1fff & 0xff, d, 3)) return false;
    if (!Send(0x05, uint16_t((0x1fff & 0xff00) | 0x43), 0x1fff & 0xff, d, 3)) return false;
    if (!Send(0x06, 0, 0, d, 1)) return false;
    SleepMs(150);
    return true;
}

bool Ut100bDevice::GetCNR(float& cnr_db) {
    uint8_t b0 = 0, b1 = 0, b2 = 0;
    bool ok = false;
    if (!RegRead(kI2cDemod, 0x8b, b0, ok) || !RegRead(kI2cDemod, 0x8c, b1, ok) || !RegRead(kI2cDemod, 0x8d, b2, ok)) {
        return false;
    }
    const uint32_t raw = (uint32_t(b0) << 16) | (uint32_t(b1) << 8) | uint32_t(b2);
    if (raw == 0) { cnr_db = 0.0f; return true; }
    const double p = 10.0 * std::log10(5505024.0 / double(raw));
    double c = 0.000024 * p * p * p * p - 0.0016 * p * p * p + 0.0398 * p * p + 0.5491 * p + 3.0965;
    if (c < 0.0) c = 0.0;
    cnr_db = static_cast<float>(c);
    return true;
}

bool Ut100bDevice::StartStream(WinUsbDevice::ChunkCallback cb) {
    return usb_.StartAsyncBulkStream(0x81, 32, 16384, std::move(cb));
}

void Ut100bDevice::StopStream() {
    usb_.StopAsyncStream();
}

void Ut100bDevice::Close() {
    if (!usb_.IsOpen()) return;
    StopStream();
    if (open_) {
        open_ = false;
        std::vector<uint8_t> d;
        bool ok = false;
        SleepMs(100);
        Send(0x07, 0, 0, d, 1);
        Send(0x09, 0x0100, 0, d, 1);
        RegWrite(card_addr_, 0x80, 0x00, ok);
        RegWrite(card_addr_, 0x04, 0x00, ok);
        RegWrite(card_addr_, 0x00, 0x00, ok);
        SleepMs(50);
        Gpio(0xd4, 0x00);
        Gpio(0x08, 0x08);
        SleepMs(50);
    }
    usb_.Close();
}

// --- ASIE5606 内蔵 B-CAS スロット ---
bool Ut100bDevice::CardRead(uint8_t reg, uint8_t& val) {
    bool ok = false;
    return RegRead(card_addr_, reg, val, ok) && ok;
}

bool Ut100bDevice::CardWrite(uint8_t reg, uint8_t val) {
    bool ok = false;
    return RegWrite(card_addr_, reg, val, ok) && ok;
}

bool Ut100bDevice::CardAvailable(int& count) {
    uint8_t hi = 0, lo = 0, st = 0;
    if (bcas_extended_ && !CardRead(0x39, hi)) return false;
    if (!CardRead(0x03, lo) || !CardRead(0x04, st)) return false;
    if ((st & 0x01) == 0) return false;
    count = int(lo) | (int(hi & 1) << 8);
    return true;
}

bool Ut100bDevice::CardWaitFor(int minimum, int polls, int& count) {
    for (int i = 0; i < polls; ++i) {
        SleepMs(50);
        if (!CardAvailable(count)) return false;
        if (count >= minimum) return true;
    }
    return false;
}

bool Ut100bDevice::CardReadWindow(int start, int size, std::vector<uint8_t>& out) {
    for (int off = start; off < start + size; ++off) {
        int page = off / 0x40, within = off % 0x40;
        if (bcas_extended_ && within == 0 && !CardWrite(0x3a, uint8_t(page))) return false;
        uint8_t v = 0;
        if (!CardRead(uint8_t(0x40 + within), v)) return false;
        out.push_back(v);
    }
    return true;
}

bool Ut100bDevice::CardWriteWindow(const std::vector<uint8_t>& data) {
    for (size_t off = 0; off < data.size(); ++off) {
        int page = int(off / 0x40), within = int(off % 0x40);
        if (bcas_extended_ && within == 0 && !CardWrite(0x3a, uint8_t(page))) return false;
        if (!CardWrite(uint8_t(0x40 + within), data[off])) return false;
    }
    return true;
}

bool Ut100bDevice::CardSendBlock(const std::vector<uint8_t>& block) {
    if (!CardWrite(0x02, uint8_t(block.size()))) return false;
    if (bcas_extended_ && !CardWrite(0x38, uint8_t(block.size() >> 8))) return false;
    if (!CardWriteWindow(block)) return false;
    if (!CardWrite(0x00, 0x0a)) return false;
    SleepMs(100);
    return true;
}

bool Ut100bDevice::CardReceiveBlock(bool atr, std::vector<uint8_t>& out) {
    int count = 0;
    if (!CardWaitFor(atr ? 13 : 3, 42, count)) return false;
    out.clear();
    if (!CardReadWindow(0, count, out)) return false;
    if (!atr) {
        int total = int(Reverse8(out[2])) + 4;
        if (total > count) {
            int second = 0;
            if (!CardWaitFor(total, 8, second)) return false;
            if (!CardReadWindow(count, second - count, out)) return false;
        }
    }
    return CardWrite(0x00, 0x0c);
}

bool Ut100bDevice::ResetBcas() {
    std::lock_guard<std::mutex> lk(bcas_mu_);
    uint8_t ver = 0, st = 0;
    if (!CardRead(0x09, ver)) return false;
    bcas_extended_ = (((ver & 0x3e) >> 1) != 0);

    if (!CardWrite(0x00, 0x80)) return false;
    SleepMs(10);
    if (!CardWrite(0x00, 0x00) || !CardWrite(0x04, 0x00) || !CardWrite(0x00, 0x00)) return false;
    SleepMs(100);

    if (!CardRead(0x04, st) || (st & 0x01) == 0) return false;
    if (!CardWrite(0x00, 0x08) || !CardWrite(0x04, 0x80)) return false;
    SleepMs(200);
    if (!CardWrite(0x00, 0x09)) return false;

    std::vector<uint8_t> atr;
    if (!CardReceiveBlock(true, atr)) return false;
    if (!CardWrite(0x01, 0x01)) return false;
    bcas_seq_ = 0;

    if (bcas_extended_) {
        if (!CardSendBlock(FrameT1({0x00, 0xc1, 0x01, 0xfe}))) return false;
        if (!CardWrite(0x00, 0x0c)) return false;
    }
    return true;
}

bool Ut100bDevice::TransmitBcas(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp_out) {
    std::lock_guard<std::mutex> lk(bcas_mu_);
    const size_t buf_size = bcas_extended_ ? 0x140 : 0x40;
    if (len == 0 || len > 254 || len + 4 > buf_size) return false;

    std::vector<uint8_t> req;
    req.reserve(len + 3);
    req.push_back(0x00);
    req.push_back(uint8_t(bcas_seq_ << 6));
    req.push_back(uint8_t(len));
    req.insert(req.end(), apdu, apdu + len);
    std::vector<uint8_t> block = FrameT1(req);

    resp_out.clear();
    for (size_t iter = 0; iter < buf_size / 4 + 1; ++iter) {
        if (!CardSendBlock(block)) return false;
        std::vector<uint8_t> rec;
        if (!CardReceiveBlock(false, rec) || rec.size() < 4) return false;
        uint8_t lrc = 0;
        for (uint8_t b : rec) lrc ^= b;
        if (lrc != 0) return false;

        const uint8_t pcb = Reverse8(rec[1]);
        const size_t info_len = Reverse8(rec[2]);
        if (info_len + 4 > rec.size()) return false;
        for (size_t i = 0; i < info_len; ++i) {
            resp_out.push_back(Reverse8(rec[3 + i]));
        }
        if ((pcb & 0x20) == 0) {
            bcas_seq_ ^= 1;
            return true;
        }
        const uint8_t next_r = uint8_t((pcb & 0x40) ? 0x80 : 0x90);
        block = FrameT1({0x00, next_r, 0x00});
    }
    return false;
}

} // namespace friio

#endif // _WIN32
