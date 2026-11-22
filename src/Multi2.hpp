// SPDX-License-Identifier: MIT
// Ported from arib25 (multi2.c) / recdyna (internal/multi2/multi2.go).
// Copyright (c) 2026 waya0125
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dyna {

class Multi2Cipher {
public:
    Multi2Cipher() = default;
    Multi2Cipher(const uint8_t system_key[32], const uint8_t cbc_init[8], int round = 4);

    void Init(const uint8_t system_key[32], const uint8_t cbc_init[8], int round = 4);
    void SetScrambleKey(const uint8_t key[16]);
    void ClearScrambleKey();
    bool Keyed() const { return keyed_; }

    // scrambling: 2 = even key, otherwise odd key
    void Decrypt(int scrambling, uint8_t* buf, size_t len) const;
    void Encrypt(int scrambling, uint8_t* buf, size_t len) const;

private:
    struct Data { uint32_t l = 0, r = 0; };
    using Param = std::array<uint32_t, 8>;

    static Param Schedule(const Param& sys, Data key);
    static Data EncryptBlock(Data d, const Param& w, int round);
    static Data DecryptBlock(Data d, const Param& w, int round);

    Data cbc_init_{};
    Param sys_{};
    std::array<Param, 2> work_{}; // 0: odd, 1: even
    int round_ = 4;
    bool keyed_ = false;
};

} // namespace dyna
