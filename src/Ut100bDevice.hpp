// SPDX-License-Identifier: BSD-2-Clause AND MIT
// Copyright (c) 2026 waya0125
#pragma once

#ifdef _WIN32

#include "AsieStreamCipher.hpp"
#include "WinUsbDevice.hpp"

#include <cstdint>
#include <mutex>
#include <vector>

namespace friio {

constexpr uint16_t kUt100bVid = 0x3275;
constexpr uint16_t kUt100bPid = 0x7020;

class Ut100bDevice {
public:
    Ut100bDevice() = default;
    ~Ut100bDevice() { Close(); }

    bool Open(int device_index);
    bool Init();
    void Close();
    bool IsOpen() const { return usb_.IsOpen(); }

    bool Tune(int freq_mhz);
    bool GetCNR(float& cnr_db);

    bool StartStream(WinUsbDevice::ChunkCallback cb);
    void StopStream();

    // ASIE5606 内蔵 B-CAS スロット（T=1）
    bool ResetBcas();
    bool TransmitBcas(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp_out);

    static bool ChannelToFreqMHz(int ch, bool catv, int& freq_mhz);

private:
    bool Send(uint8_t req, uint16_t val, uint16_t idx, std::vector<uint8_t>& out, uint16_t len);
    bool RegRead(uint8_t addr, uint8_t reg, uint8_t& val, bool& ok);
    bool RegWrite(uint8_t addr, uint8_t reg, uint8_t val, bool& ok);
    bool Gpio(uint8_t mask, uint8_t val);
    bool GpioEx(uint8_t mask, uint8_t val);
    bool I2cWrite(uint8_t addr, const uint8_t* data, size_t len);
    bool TunerSend(const uint8_t* data, size_t len);

    // ASIE5606 カード補助
    bool CardRead(uint8_t reg, uint8_t& val);
    bool CardWrite(uint8_t reg, uint8_t val);
    bool CardAvailable(int& count);
    bool CardWaitFor(int minimum, int polls, int& count);
    bool CardReadWindow(int start, int size, std::vector<uint8_t>& out);
    bool CardWriteWindow(const std::vector<uint8_t>& data);
    bool CardSendBlock(const std::vector<uint8_t>& block);
    bool CardReceiveBlock(bool atr, std::vector<uint8_t>& out);

    WinUsbDevice usb_;
    mutable std::mutex io_mu_;
    uint8_t card_addr_ = 0x5a;
    bool open_ = false;

    std::mutex bcas_mu_;
    bool bcas_extended_ = true;
    uint8_t bcas_seq_ = 0;
};

} // namespace friio

#endif // _WIN32
