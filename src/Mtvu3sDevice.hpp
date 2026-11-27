// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#ifdef _WIN32

#include "Aes128Ecb.hpp"
#include "Fc0013.hpp"
#include "I2cBus.hpp"
#include "Mtvu3sCardReader.hpp"
#include "Qm1d1c0042.hpp"
#include "SatTsid.hpp"
#include "Tc90522.hpp"
#include "UsbSelect.hpp"
#include "WinUsbDevice.hpp"

#include <bcrypt.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace friio {

constexpr uint16_t kMtvu3sVid = 0x3275;
constexpr uint16_t kMtvu3sPid = 0x9015;

enum class Band { UHF, BS, CS };

class Mtvu3sDevice {
public:
    Mtvu3sDevice() = default;
    ~Mtvu3sDevice() { Close(); }

    bool Open(int device_index);
    bool Init();
    void Close();
    bool IsOpen() const { return usb_.IsOpen(); }

    bool TuneUHF(uint32_t freq_khz);
    // BS: slot はそのトランスポンダーの何番目の TS か（TMCC の一覧から空きを除いた並び）。
    // その番目の TS が無ければ false。
    bool TuneBS(uint32_t freq_khz, int slot);
    // 110 度 CS: TSID で選ぶ（ネットワークのビットは無視して当てる）。無ければ false。
    bool TuneCS(uint32_t freq_khz, uint16_t tsid);
    // BS / CS110 共通: TSID を直接指定して選ぶ（INI [CHANNEL] 用）。無ければ false。
    bool TuneSatByTsid(uint32_t freq_khz, uint16_t tsid);
    bool GetCNR(float& cnr_db);

    bool StartStream(WinUsbDevice::ChunkCallback cb);
    // 直近の StartStream で使った Isochronous パケットサイズ
    size_t IsochPacketSize() const { return iso_packet_size_; }
    void StopStream();

    void DescramblePacket(uint8_t pkt[188]) const;

    // EM28178 内蔵 B-CAS スロット（T=1）
    bool ResetBcas();
    bool TransmitBcas(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp_out);

private:
    class Em28178Bus : public I2cBus {
    public:
        explicit Em28178Bus(Mtvu3sDevice* dev) : dev_(dev) {}
        void write(uint8_t addr, const uint8_t* data, uint16_t len, bool stop = true) override;
        void read(uint8_t addr, uint8_t* data, uint16_t len, bool stop = true) override;
    private:
        Mtvu3sDevice* dev_;
    };

    bool WriteReg(uint8_t reg, uint8_t val);
    bool ReadReg(uint8_t reg, uint8_t& val);
    void Idle();
    // 衛星の周波数に合わせて TMCC の TSID の一覧を読む。空なら読み直す。読めなければ false。
    bool TuneSatAndReadTmcc(uint32_t freq_khz, std::vector<uint16_t>& ids);
    bool SelectSatTsid(uint16_t tsid);
    void InitCngAes();
    void CloseCngAes();

    static constexpr uint8_t kIsochEp = 0x84;

    WinUsbDevice usb_;
    size_t iso_packet_size_ = kDefaultIsochPacketSize;
    mutable std::mutex io_mu_;
    std::unique_ptr<Em28178Bus> bus_;
    std::unique_ptr<Tc90522> ter_;
    std::unique_ptr<Tc90522> sat_;
    std::unique_ptr<Fc0013> ter_tuner_;
    std::unique_ptr<Qm1d1c0042> sat_tuner_;
    BCRYPT_ALG_HANDLE aes_alg_ = nullptr;
    BCRYPT_KEY_HANDLE aes_key_ = nullptr;
    Aes128EcbDecryptor aes_;
    Band band_ = Band::UHF;
    std::mutex bcas_mu_;
    std::unique_ptr<Mtvu3sCardReader> card_;
};

} // namespace friio

#endif // _WIN32
