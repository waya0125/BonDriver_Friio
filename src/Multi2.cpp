// SPDX-License-Identifier: MIT
// Ported from arib25 (multi2.c) / recdyna (internal/multi2/multi2.go).
// Copyright (c) 2026 waya0125
#include "Multi2.hpp"

namespace dyna {

namespace {

inline uint32_t ReadBE32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

inline void WriteBE32(uint8_t* p, uint32_t v) {
    p[0] = uint8_t(v >> 24);
    p[1] = uint8_t(v >> 16);
    p[2] = uint8_t(v >> 8);
    p[3] = uint8_t(v);
}

inline uint32_t Rol(uint32_t v, unsigned n) {
    return (v << n) | (v >> (32u - n));
}

} // namespace

Multi2Cipher::Multi2Cipher(const uint8_t system_key[32], const uint8_t cbc_init[8], int round) {
    Init(system_key, cbc_init, round);
}

void Multi2Cipher::Init(const uint8_t system_key[32], const uint8_t cbc_init[8], int round) {
    round_ = round;
    for (size_t i = 0; i < 8; ++i) {
        sys_[i] = ReadBE32(system_key + i * 4);
    }
    cbc_init_ = {ReadBE32(cbc_init), ReadBE32(cbc_init + 4)};
    ClearScrambleKey();
}

void Multi2Cipher::SetScrambleKey(const uint8_t key[16]) {
    const Data odd{ReadBE32(key), ReadBE32(key + 4)};
    const Data even{ReadBE32(key + 8), ReadBE32(key + 12)};
    work_[0] = Schedule(sys_, odd);
    work_[1] = Schedule(sys_, even);
    keyed_ = true;
}

void Multi2Cipher::ClearScrambleKey() {
    work_[0].fill(0);
    work_[1].fill(0);
    keyed_ = false;
}

Multi2Cipher::Param Multi2Cipher::Schedule(const Param& sys, Data key) {
    auto pi1 = [](Data s) -> Data { return {s.l, s.r ^ s.l}; };
    auto pi2 = [](Data s, uint32_t a) -> Data {
        uint32_t t0 = s.r + a;
        uint32_t t1 = Rol(t0, 1) + t0 - 1;
        uint32_t t2 = Rol(t1, 4) ^ t1;
        return {s.l ^ t2, s.r};
    };
    auto pi3 = [](Data s, uint32_t a, uint32_t b) -> Data {
        uint32_t t0 = s.l + a;
        uint32_t t1 = Rol(t0, 2) + t0 + 1;
        uint32_t t2 = Rol(t1, 8) ^ t1;
        uint32_t t3 = t2 + b;
        uint32_t t4 = Rol(t3, 1) - t3;
        uint32_t t5 = Rol(t4, 16) ^ (t4 | s.l);
        return {s.l, s.r ^ t5};
    };
    auto pi4 = [](Data s, uint32_t a) -> Data {
        uint32_t t0 = s.r + a;
        uint32_t t1 = Rol(t0, 2) + t0 + 1;
        return {s.l ^ t1, s.r};
    };

    Param w{};
    Data b1 = pi1(key);
    Data b2 = pi2(b1, sys[0]); w[0] = b2.l;
    Data b3 = pi3(b2, sys[1], sys[2]); w[1] = b3.r;
    Data b4 = pi4(b3, sys[3]); w[2] = b4.l;
    Data b5 = pi1(b4); w[3] = b5.r;
    Data b6 = pi2(b5, sys[4]); w[4] = b6.l;
    Data b7 = pi3(b6, sys[5], sys[6]); w[5] = b7.r;
    Data b8 = pi4(b7, sys[7]); w[6] = b8.l;
    Data b9 = pi1(b8); w[7] = b9.r;
    return w;
}

Multi2Cipher::Data Multi2Cipher::EncryptBlock(Data d, const Param& w, int round) {
    auto pi1 = [](Data s) -> Data { return {s.l, s.r ^ s.l}; };
    auto pi2 = [](Data s, uint32_t a) -> Data {
        uint32_t t0 = s.r + a;
        uint32_t t1 = Rol(t0, 1) + t0 - 1;
        uint32_t t2 = Rol(t1, 4) ^ t1;
        return {s.l ^ t2, s.r};
    };
    auto pi3 = [](Data s, uint32_t a, uint32_t b) -> Data {
        uint32_t t0 = s.l + a;
        uint32_t t1 = Rol(t0, 2) + t0 + 1;
        uint32_t t2 = Rol(t1, 8) ^ t1;
        uint32_t t3 = t2 + b;
        uint32_t t4 = Rol(t3, 1) - t3;
        uint32_t t5 = Rol(t4, 16) ^ (t4 | s.l);
        return {s.l, s.r ^ t5};
    };
    auto pi4 = [](Data s, uint32_t a) -> Data {
        uint32_t t0 = s.r + a;
        uint32_t t1 = Rol(t0, 2) + t0 + 1;
        return {s.l ^ t1, s.r};
    };

    for (int i = 0; i < round; ++i) {
        d = pi1(d); d = pi2(d, w[0]); d = pi3(d, w[1], w[2]); d = pi4(d, w[3]);
        d = pi1(d); d = pi2(d, w[4]); d = pi3(d, w[5], w[6]); d = pi4(d, w[7]);
    }
    return d;
}

Multi2Cipher::Data Multi2Cipher::DecryptBlock(Data d, const Param& w, int round) {
    auto pi1 = [](Data s) -> Data { return {s.l, s.r ^ s.l}; };
    auto pi2 = [](Data s, uint32_t a) -> Data {
        uint32_t t0 = s.r + a;
        uint32_t t1 = Rol(t0, 1) + t0 - 1;
        uint32_t t2 = Rol(t1, 4) ^ t1;
        return {s.l ^ t2, s.r};
    };
    auto pi3 = [](Data s, uint32_t a, uint32_t b) -> Data {
        uint32_t t0 = s.l + a;
        uint32_t t1 = Rol(t0, 2) + t0 + 1;
        uint32_t t2 = Rol(t1, 8) ^ t1;
        uint32_t t3 = t2 + b;
        uint32_t t4 = Rol(t3, 1) - t3;
        uint32_t t5 = Rol(t4, 16) ^ (t4 | s.l);
        return {s.l, s.r ^ t5};
    };
    auto pi4 = [](Data s, uint32_t a) -> Data {
        uint32_t t0 = s.r + a;
        uint32_t t1 = Rol(t0, 2) + t0 + 1;
        return {s.l ^ t1, s.r};
    };

    for (int i = 0; i < round; ++i) {
        d = pi4(d, w[7]); d = pi3(d, w[5], w[6]); d = pi2(d, w[4]); d = pi1(d);
        d = pi4(d, w[3]); d = pi3(d, w[1], w[2]); d = pi2(d, w[0]); d = pi1(d);
    }
    return d;
}

void Multi2Cipher::Decrypt(int scrambling, uint8_t* buf, size_t len) const {
    const Param& w = (scrambling == 2) ? work_[1] : work_[0];
    Data cbc = cbc_init_;
    size_t off = 0;
    while (len - off >= 8) {
        Data src{ReadBE32(buf + off), ReadBE32(buf + off + 4)};
        Data dst = DecryptBlock(src, w, round_);
        dst.l ^= cbc.l;
        dst.r ^= cbc.r;
        cbc = src;
        WriteBE32(buf + off, dst.l);
        WriteBE32(buf + off + 4, dst.r);
        off += 8;
    }
    if (off < len) {
        Data dst = EncryptBlock(cbc, w, round_);
        uint8_t tmp[8];
        WriteBE32(tmp, dst.l);
        WriteBE32(tmp + 4, dst.r);
        for (size_t i = 0; i < len - off; ++i) {
            buf[off + i] ^= tmp[i];
        }
    }
}

void Multi2Cipher::Encrypt(int scrambling, uint8_t* buf, size_t len) const {
    const Param& w = (scrambling == 2) ? work_[1] : work_[0];
    Data dst = cbc_init_;
    size_t off = 0;
    while (len - off >= 8) {
        Data src{ReadBE32(buf + off) ^ dst.l, ReadBE32(buf + off + 4) ^ dst.r};
        dst = EncryptBlock(src, w, round_);
        WriteBE32(buf + off, dst.l);
        WriteBE32(buf + off + 4, dst.r);
        off += 8;
    }
    if (off < len) {
        dst = EncryptBlock(dst, w, round_);
        uint8_t tmp[8];
        WriteBE32(tmp, dst.l);
        WriteBE32(tmp + 4, dst.r);
        for (size_t i = 0; i < len - off; ++i) {
            buf[off + i] ^= tmp[i];
        }
    }
}

} // namespace dyna
