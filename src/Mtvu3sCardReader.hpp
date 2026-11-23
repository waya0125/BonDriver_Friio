// SPDX-License-Identifier: MIT
// Ported from recfriio (internal/mtvu3s/card.go).
// Copyright (c) 2026 n2naokun
// Copyright (c) 2026 waya0125
#pragma once

#include "Aes128Ecb.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace friio {

constexpr size_t kMtvu3sCardMaxApdu = 124;
constexpr size_t kMtvu3sCardMaxReply = 254;
constexpr std::array<uint8_t, 13> kMtvu3sCardAtr = {
    0x3b, 0xf0, 0x12, 0x00, 0xff, 0x91, 0x81, 0xb1, 0x7c, 0x45, 0x1f, 0x03, 0x99
};

struct Mtvu3sCardIO {
    std::function<bool(uint8_t reg, uint8_t& val)> read_reg;
    std::function<bool(uint8_t reg, uint8_t val)> write_reg;
    std::function<bool(uint8_t req_type, uint8_t req, uint16_t val, uint16_t idx,
                       uint8_t* data, uint16_t len, uint32_t timeout_ms, size_t* transferred)> control;
    std::function<void(int ms)> sleep_ms;
};

// MonsterTV U3S (EM28178) の内蔵 B-CAS カードスロット (ISO 7816 T=1)
class Mtvu3sCardReader {
public:
    explicit Mtvu3sCardReader(Mtvu3sCardIO io);

    bool Reset();
    bool Transmit(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp_out,
                  bool* apdu_len_error = nullptr);

private:
    bool TransferBuffer(uint8_t direction, uint16_t start, uint8_t* data, size_t len);
    bool Exchange(uint8_t pcb, uint8_t expected_pcb, const uint8_t* payload, size_t payload_len,
                  std::vector<uint8_t>& resp_out);
    bool RestoreCardReply(const uint8_t* apdu, size_t apdu_len, uint8_t* frame, size_t frame_len) const;
    static uint8_t CalcLrc(const uint8_t* data, size_t len);

    Mtvu3sCardIO io_;
    Aes128EcbDecryptor aes_;
    bool ready_ = false;
    bool ifsd_set_ = false;
    uint8_t next_pcb_ = 0;
};

} // namespace friio
