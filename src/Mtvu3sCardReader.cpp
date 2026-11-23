// SPDX-License-Identifier: MIT
// Ported from recfriio (internal/mtvu3s/card.go).
// Copyright (c) 2026 n2naokun
// Copyright (c) 2026 waya0125
#include "Mtvu3sCardReader.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace friio {

namespace {

constexpr uint8_t kCardStatus = 0x70;
constexpr uint8_t kCardLength = 0x71;
constexpr uint8_t kCardLenHi = 0x72;
constexpr uint8_t kCardGpioShadow = 0x73;
constexpr uint8_t kCardMode = 0x75;

constexpr uint8_t kCardBufferRequest = 0x14;
constexpr uint8_t kCardBufferIn = 0xc0;
constexpr uint8_t kCardBufferOut = 0x40;
constexpr uint16_t kCardBufferSend = 0x0200;
constexpr uint8_t kCardReadyBits = 0x05;

} // namespace

Mtvu3sCardReader::Mtvu3sCardReader(Mtvu3sCardIO io) : io_(std::move(io)) {
    const uint8_t zero_key[16]{};
    aes_.setKey(zero_key);
    if (!io_.sleep_ms) {
        io_.sleep_ms = [](int ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        };
    }
}

uint8_t Mtvu3sCardReader::CalcLrc(const uint8_t* data, size_t len) {
    uint8_t lrc = 0;
    for (size_t i = 0; i < len; ++i) {
        lrc ^= data[i];
    }
    return lrc;
}

bool Mtvu3sCardReader::TransferBuffer(uint8_t direction, uint16_t start, uint8_t* data, size_t len) {
    if (!io_.control) return false;
    size_t offset = 0;
    while (offset < len) {
        const uint16_t chunk = static_cast<uint16_t>(std::min<size_t>(len - offset, 64));
        size_t transferred = 0;
        if (!io_.control(direction, kCardBufferRequest, 0, static_cast<uint16_t>(start + offset),
                         data + offset, chunk, 1000, &transferred)) {
            return false;
        }
        if (transferred != chunk) {
            return false;
        }
        offset += transferred;
    }
    return true;
}

bool Mtvu3sCardReader::Reset() {
    ready_ = false;
    ifsd_set_ = false;
    next_pcb_ = 0;
    if (!io_.read_reg || !io_.write_reg || !io_.control) return false;

    if (!io_.write_reg(kCardGpioShadow, 0x00)) return false;
    io_.sleep_ms(700);

    uint8_t status = 0;
    if (!io_.read_reg(kCardStatus, status)) return false;
    if ((status & kCardReadyBits) != kCardReadyBits) return false;

    uint8_t length = 0;
    if (!io_.read_reg(kCardLength, length)) return false;
    if (length != kMtvu3sCardAtr.size()) return false;

    std::array<uint8_t, kMtvu3sCardAtr.size()> atr{};
    if (!TransferBuffer(kCardBufferIn, 0, atr.data(), atr.size())) return false;
    if (atr != kMtvu3sCardAtr) return false;

    ready_ = true;
    return true;
}

bool Mtvu3sCardReader::Transmit(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp_out,
                                bool* apdu_len_error) {
    if (apdu_len_error) *apdu_len_error = false;
    if (!apdu || len < 4 || len > kMtvu3sCardMaxApdu) {
        if (apdu_len_error) *apdu_len_error = true;
        return false;
    }
    if (!ready_) return false;

    ready_ = false;
    if (!ifsd_set_) {
        const uint8_t ifsd = static_cast<uint8_t>(kMtvu3sCardMaxReply);
        std::vector<uint8_t> ifsd_reply;
        if (!Exchange(0xc1, 0xe1, &ifsd, 1, ifsd_reply)) return false;
        if (ifsd_reply.size() != 1 || ifsd_reply[0] != ifsd) return false;
        ifsd_set_ = true;
    }

    std::vector<uint8_t> reply;
    if (!Exchange(next_pcb_, next_pcb_, apdu, len, reply)) return false;
    if (reply.size() < 2) return false;

    next_pcb_ ^= 0x40;
    ready_ = true;
    resp_out = std::move(reply);
    return true;
}

bool Mtvu3sCardReader::Exchange(uint8_t pcb, uint8_t expected_pcb, const uint8_t* payload, size_t payload_len,
                                std::vector<uint8_t>& resp_out) {
    std::vector<uint8_t> tx(payload_len + 4, 0);
    tx[0] = 0x00;
    tx[1] = pcb;
    tx[2] = static_cast<uint8_t>(payload_len);
    if (payload_len > 0) {
        std::memcpy(tx.data() + 3, payload, payload_len);
    }
    tx[tx.size() - 1] = CalcLrc(tx.data(), tx.size() - 1);

    if (!TransferBuffer(kCardBufferOut, kCardBufferSend, tx.data(), tx.size())) return false;

    uint8_t mode = 0;
    if (!io_.read_reg(kCardMode, mode)) return false;
    if (!io_.write_reg(kCardMode, static_cast<uint8_t>((mode & 0xfb) | 0x01))) return false;
    if (!io_.write_reg(kCardStatus, 0xff)) return false;
    if (!io_.write_reg(kCardLength, static_cast<uint8_t>(tx.size()))) return false;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    // 応答長を早すぎる時点で読むと失敗するため、試験 DLL / recfriio と同じ時間だけ待つ。
    io_.sleep_ms(80);

    std::array<uint8_t, kMtvu3sCardMaxReply + 4> rx{};
    for (; std::chrono::steady_clock::now() < deadline; io_.sleep_ms(5)) {
        uint8_t status = 0;
        if (!io_.read_reg(kCardStatus, status)) return false;
        if ((status & kCardReadyBits) != kCardReadyBits) continue;

        uint8_t lo = 0;
        uint8_t hi = 0;
        if (!io_.read_reg(kCardLength, lo)) return false;
        if (!io_.read_reg(kCardLenHi, hi)) return false;
        const size_t count = static_cast<size_t>(lo) | (static_cast<size_t>(hi) << 8);
        if (count < 4) continue;
        if (count > rx.size()) return false;

        if (!TransferBuffer(kCardBufferIn, 0, rx.data(), count)) return false;

        const size_t size = rx[2];
        // 受信完了ビットは T=1 ブロック全体が届く前にも立つ。
        if (count < size + 4) continue;
        if (count != size + 4 || rx[0] != 0x00 || rx[1] != expected_pcb) return false;

        if (!RestoreCardReply(payload, payload_len, rx.data(), count)) return false;

        resp_out.assign(rx.begin() + 3, rx.begin() + static_cast<ptrdiff_t>(3 + size));
        return true;
    }
    return false;
}

bool Mtvu3sCardReader::RestoreCardReply(const uint8_t* apdu, size_t apdu_len,
                                        uint8_t* frame, size_t frame_len) const {
    const bool initial = (frame_len == 65 && apdu_len >= 5 && apdu[1] == 0x30);
    const bool ecm = (frame_len == 29 && apdu_len >= 5 && apdu[1] == 0x34);
    if ((initial || ecm) && apdu[0] == 0x90 && apdu[2] == 0x00 && apdu[3] == 0x00 &&
        frame[3] == 0x00 && static_cast<size_t>(frame[4]) == frame_len - 8) {
        aes_.decryptBlock(frame + 9);
        if (frame[frame_len - 3] != 0x90 || frame[frame_len - 2] != 0x00 ||
            (initial && (frame[7] != 0x21 || frame[8] != 0x00 || frame[9] != 0x00 || frame[10] != 0x05))) {
            return false;
        }
    }
    return CalcLrc(frame, frame_len) == 0;
}

} // namespace friio
