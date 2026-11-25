// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#include "Aes128Ecb.hpp"
#include "AsieStreamCipher.hpp"
#include "B25Decoder.hpp"
#include "Mtvu3sCardReader.hpp"
#include "Multi2.hpp"
#include "SatTsid.hpp"
#include "Settler.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>
#include <vector>

namespace {

void TestAsieStreamCipherNullPacket() {
    const uint8_t null_payload[184] = {
        0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56, 0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56,
        0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56, 0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56,
        0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56, 0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56,
        0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56, 0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56,
        0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56, 0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56,
        0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56, 0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56,
        0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56, 0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56,
        0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56, 0x4e, 0x8c, 0x28, 0x31, 0x35, 0x43, 0xbf, 0x56,
        0xe9, 0x12, 0xe3, 0x59, 0x9d, 0x94, 0xd7, 0x8c, 0xe9, 0x12, 0xe3, 0x59, 0x9d, 0x94, 0xd7, 0x8c,
        0xe9, 0x12, 0xe3, 0x59, 0x9d, 0x94, 0xd7, 0x8c, 0xe9, 0x12, 0xe3, 0x59, 0x9d, 0x94, 0xd7, 0x8c,
        0xe9, 0x12, 0xe3, 0x59, 0x9d, 0x94, 0xd7, 0x8c, 0xe9, 0x12, 0xe3, 0x59, 0x9d, 0x94, 0xd7, 0x8c,
        0xe9, 0x12, 0xe3, 0x59, 0x9d, 0x94, 0xd7, 0x8c,
    };
    uint8_t pkt[188];
    pkt[0] = 0x47; pkt[1] = 0x1f; pkt[2] = 0xff; pkt[3] = 0x10;
    std::memcpy(pkt + 4, null_payload, 184);

    asieDecryptPacket(pkt, pkt);
    for (int i = 4; i < 188; ++i) {
        assert(pkt[i] == 0xff);
    }
}

void TestMulti2RoundTrip() {
    uint8_t sys[32], cbc[8], ks[16];
    for (int i = 0; i < 32; ++i) sys[i] = uint8_t(0x10 + i);
    for (int i = 0; i < 8; ++i) cbc[i] = uint8_t(0x80 + i);
    for (int i = 0; i < 16; ++i) ks[i] = uint8_t(0xa0 + i);

    dyna::Multi2Cipher m(sys, cbc, 4);
    m.SetScrambleKey(ks);
    std::vector<uint8_t> orig(184);
    for (size_t i = 0; i < orig.size(); ++i) orig[i] = uint8_t(i * 11 + 3);
    auto buf = orig;
    m.Encrypt(2, buf.data(), buf.size());
    m.Decrypt(2, buf.data(), buf.size());
    assert(std::memcmp(buf.data(), orig.data(), orig.size()) == 0);
}

void TestSettler() {
    dyna::Settler s(3, 100);
    std::vector<uint8_t> pkts(188 * 4, 0);
    pkts[0] = 0x47; pkts[1] = 0x00; pkts[2] = 0x00; pkts[3] = 0x10;
    pkts[188] = 0x47; pkts[189] = 0x01; pkts[190] = 0x00; pkts[191] = 0x10;
    pkts[376] = 0x47; pkts[377] = 0x01; pkts[378] = 0x00; pkts[379] = 0x11;
    pkts[564] = 0x47; pkts[565] = 0x01; pkts[566] = 0x00; pkts[567] = 0x12;
    auto out = s.Feed(pkts.data(), pkts.size());
    assert(s.Settled() && !s.GaveUp() && out.size() == pkts.size());
}

// テスト用 AES-128-ECB 暗号化（ゼロ鍵）
void Aes128EncryptZeroKey(uint8_t block[16]) {
    static const uint8_t sbox[256] = {
        0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
        0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
        0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
        0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
        0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
        0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
        0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
        0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
        0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
        0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
        0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
        0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
        0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
        0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
        0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
        0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
    };
    static const uint8_t rcon[11] = {
        0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
    };
    uint8_t rk[176]{};
    for (int i = 4; i < 44; ++i) {
        uint8_t tmp[4];
        std::memcpy(tmp, &rk[(i - 1) * 4], 4);
        if (i % 4 == 0) {
            uint8_t k = tmp[0];
            tmp[0] = sbox[tmp[1]] ^ rcon[i / 4];
            tmp[1] = sbox[tmp[2]];
            tmp[2] = sbox[tmp[3]];
            tmp[3] = sbox[k];
        }
        for (int j = 0; j < 4; ++j) rk[i * 4 + j] = rk[(i - 4) * 4 + j] ^ tmp[j];
    }
    auto xtime = [](uint8_t x) -> uint8_t {
        return static_cast<uint8_t>((x << 1) ^ (((x >> 7) & 1) * 0x1b));
    };
    auto add_rk = [&](int round) {
        for (int i = 0; i < 16; ++i) block[i] ^= rk[round * 16 + i];
    };
    auto shift_rows = [&]() {
        uint8_t t = block[1]; block[1] = block[5]; block[5] = block[9]; block[9] = block[13]; block[13] = t;
        t = block[2]; block[2] = block[10]; block[10] = t;
        t = block[6]; block[6] = block[14]; block[14] = t;
        t = block[15]; block[15] = block[11]; block[11] = block[7]; block[7] = block[3]; block[3] = t;
    };
    auto mix_cols = [&]() {
        for (int c = 0; c < 4; ++c) {
            uint8_t* col = &block[c * 4];
            uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
            uint8_t t = a0 ^ a1 ^ a2 ^ a3;
            col[0] ^= t ^ xtime(a0 ^ a1);
            col[1] ^= t ^ xtime(a1 ^ a2);
            col[2] ^= t ^ xtime(a2 ^ a3);
            col[3] ^= t ^ xtime(a3 ^ a0);
        }
    };
    add_rk(0);
    for (int round = 1; round <= 9; ++round) {
        for (int i = 0; i < 16; ++i) block[i] = sbox[block[i]];
        shift_rows();
        mix_cols();
        add_rk(round);
    }
    for (int i = 0; i < 16; ++i) block[i] = sbox[block[i]];
    shift_rows();
    add_rk(10);
}

uint8_t TestCardLrc(const std::vector<uint8_t>& data) {
    uint8_t lrc = 0;
    for (uint8_t b : data) lrc ^= b;
    return lrc;
}

std::vector<uint8_t> MakeCardFrame(uint8_t pcb, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 4);
    frame.push_back(0x00);
    frame.push_back(pcb);
    frame.push_back(static_cast<uint8_t>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.push_back(TestCardLrc(frame));
    return frame;
}

std::vector<uint8_t> MakeEncryptedReply(uint8_t pcb, const std::vector<uint8_t>& payload) {
    auto frame = MakeCardFrame(pcb, payload);
    assert(frame.size() >= 25);
    Aes128EncryptZeroKey(frame.data() + 9);
    return frame;
}

struct MockCardReply {
    std::vector<uint8_t> tx;
    std::vector<uint8_t> rx;
    bool partial = false;
};

struct MockCardUsb {
    std::deque<MockCardReply> replies;
    std::vector<uint8_t> tx;
    std::vector<uint8_t> rx;
    bool partial = false;
    bool short_read = false;
    int resets = 0;
    std::vector<uint16_t> out_offsets;
    std::vector<uint16_t> in_offsets;
    std::vector<int> waits;

    friio::Mtvu3sCardReader MakeReader() {
        friio::Mtvu3sCardIO io;
        io.read_reg = [this](uint8_t reg, uint8_t& val) { return ReadReg(reg, val); };
        io.write_reg = [this](uint8_t reg, uint8_t val) { return WriteReg(reg, val); };
        io.control = [this](uint8_t req_type, uint8_t req, uint16_t val, uint16_t idx,
                            uint8_t* data, uint16_t len, uint32_t timeout_ms, size_t* transferred) {
            return Control(req_type, req, val, idx, data, len, timeout_ms, transferred);
        };
        io.sleep_ms = [this](int ms) { waits.push_back(ms); };
        return friio::Mtvu3sCardReader(std::move(io));
    }

    bool ReadReg(uint8_t reg, uint8_t& val) {
        switch (reg) {
        case 0x70: val = 0x05; return true;
        case 0x71: val = static_cast<uint8_t>(partial ? 4 : rx.size()); return true;
        case 0x72: val = static_cast<uint8_t>(partial ? 0 : (rx.size() >> 8)); return true;
        case 0x75: val = 0x04; return true;
        default: assert(false); return false;
        }
    }

    bool WriteReg(uint8_t reg, uint8_t val) {
        switch (reg) {
        case 0x73:
            assert(val == 0x00);
            ++resets;
            rx.assign(friio::kMtvu3sCardAtr.begin(), friio::kMtvu3sCardAtr.end());
            partial = false;
            return true;
        case 0x75:
            assert(val == 0x01);
            return true;
        case 0x70:
            assert(val == 0xff);
            return true;
        case 0x71: {
            assert(!replies.empty());
            auto reply = std::move(replies.front());
            replies.pop_front();
            assert(val == tx.size());
            assert(tx == reply.tx);
            rx = std::move(reply.rx);
            partial = reply.partial;
            tx.clear();
            return true;
        }
        default:
            assert(false);
            return false;
        }
    }

    bool Control(uint8_t req_type, uint8_t req, uint16_t val, uint16_t idx,
                 uint8_t* data, uint16_t len, uint32_t timeout_ms, size_t* transferred) {
        assert(req == 0x14 && val == 0 && len <= 64 && timeout_ms == 1000);
        if (req_type == 0x40) {
            assert(idx == static_cast<uint16_t>(0x0200 + tx.size()));
            out_offsets.push_back(idx);
            tx.insert(tx.end(), data, data + len);
            if (transferred) *transferred = len;
            return true;
        }
        if (req_type == 0xc0) {
            in_offsets.push_back(idx);
            partial = false;
            if (short_read) {
                if (transferred) *transferred = len > 0 ? len - 1 : 0;
                return true;
            }
            const size_t avail = idx < rx.size() ? rx.size() - idx : 0;
            const size_t n = std::min<size_t>(len, avail);
            if (n > 0) std::memcpy(data, rx.data() + idx, n);
            if (transferred) *transferred = n;
            return true;
        }
        assert(false);
        return false;
    }
};

void TestMtvu3sCardInitialSettingAndEcm() {
    std::vector<uint8_t> init_resp(61, 0);
    init_resp[1] = 57;
    init_resp[4] = 0x21;
    init_resp[7] = 5;
    init_resp[59] = 0x90;
    for (size_t i = 16; i < 56; ++i) init_resp[i] = static_cast<uint8_t>(i);

    std::vector<uint8_t> ecm_resp(25, 0);
    ecm_resp[1] = 21;
    ecm_resp[4] = 0x08;
    ecm_resp[23] = 0x90;
    std::memcpy(ecm_resp.data() + 6, "0123456789abcdef", 16);

    MockCardUsb usb;
    usb.replies.push_back({{0x00, 0xc1, 0x01, 0xfe, 0x3e}, {0x00, 0xe1, 0x01, 0xfe, 0x1e}, false});
    usb.replies.push_back({MakeCardFrame(0x00, {0x90, 0x30, 0x00, 0x00, 0x00}),
                           MakeEncryptedReply(0x00, init_resp), true});
    usb.replies.push_back({MakeCardFrame(0x40, {0x90, 0x34, 0x00, 0x00, 0x01, 0x01, 0x00}),
                           MakeEncryptedReply(0x40, ecm_resp), false});

    auto reader = usb.MakeReader();
    assert(reader.Reset());

    const uint8_t init_cmd[] = {0x90, 0x30, 0x00, 0x00, 0x00};
    std::vector<uint8_t> got_init;
    assert(reader.Transmit(init_cmd, sizeof(init_cmd), got_init));
    assert(got_init.size() == 61);
    assert(got_init[7] == 5 && got_init[16] == 16 && got_init[48] == 48);

    const uint8_t ecm_cmd[] = {0x90, 0x34, 0x00, 0x00, 0x01, 0x01, 0x00};
    std::vector<uint8_t> got_ecm;
    assert(reader.Transmit(ecm_cmd, sizeof(ecm_cmd), got_ecm));
    assert(got_ecm.size() == 25);
    assert(got_ecm[4] == 0x08 && got_ecm[5] == 0x00);
    assert(std::memcmp(got_ecm.data() + 6, "0123456789abcdef", 16) == 0);

    const std::vector<uint16_t> expected_in = {0, 0, 0, 0, 64, 0};
    const std::vector<int> expected_waits = {700, 80, 80, 5, 80};
    assert(usb.in_offsets == expected_in);
    assert(usb.waits == expected_waits);

    std::vector<uint8_t> too_long(125, 0);
    std::vector<uint8_t> dummy;
    bool len_err = false;
    assert(!reader.Transmit(too_long.data(), too_long.size(), dummy, &len_err));
    assert(len_err && usb.resets == 1);
}

void TestMtvu3sCardBadLrcRequiresReset() {
    std::vector<uint8_t> apdu(124, 0);
    std::vector<uint8_t> plain = {0x6d, 0x00};
    MockCardReply ifs{{0x00, 0xc1, 0x01, 0xfe, 0x3e}, {0x00, 0xe1, 0x01, 0xfe, 0x1e}, false};
    auto bad = MakeCardFrame(0x00, plain);
    bad.back() ^= 1;

    MockCardUsb usb;
    usb.replies.push_back(ifs);
    usb.replies.push_back({MakeCardFrame(0x00, apdu), bad, false});
    usb.replies.push_back(ifs);
    usb.replies.push_back({MakeCardFrame(0x00, apdu), MakeCardFrame(0x00, plain), false});

    auto reader = usb.MakeReader();
    assert(reader.Reset());
    std::vector<uint8_t> resp;
    assert(!reader.Transmit(apdu.data(), apdu.size(), resp));
    assert(!reader.Transmit(apdu.data(), apdu.size(), resp));
    assert(reader.Reset());
    assert(reader.Transmit(apdu.data(), apdu.size(), resp));
    assert(resp == plain);

    const std::vector<uint16_t> expected_out = {0x200, 0x200, 0x240, 0x200, 0x200, 0x240};
    assert(usb.out_offsets == expected_out);
}

void TestMtvu3sCardRejectsShortAtrTransfer() {
    MockCardUsb usb;
    usb.short_read = true;
    auto reader = usb.MakeReader();
    assert(!reader.Reset());
}

void TestB25DecoderWithMtvu3sCard() {
    std::vector<uint8_t> init_resp(61, 0);
    init_resp[1] = 57;
    init_resp[4] = 0x21;
    init_resp[7] = 5;
    init_resp[59] = 0x90;
    for (size_t i = 16; i < 56; ++i) init_resp[i] = static_cast<uint8_t>(i);

    MockCardUsb usb;
    usb.replies.push_back({{0x00, 0xc1, 0x01, 0xfe, 0x3e}, {0x00, 0xe1, 0x01, 0xfe, 0x1e}, false});
    usb.replies.push_back({MakeCardFrame(0x00, {0x90, 0x30, 0x00, 0x00, 0x00}),
                           MakeEncryptedReply(0x00, init_resp), false});

    auto reader = usb.MakeReader();
    dyna::BcasTransport tr;
    tr.reset = [&reader]() { return reader.Reset(); };
    tr.transmit = [&reader](const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp) {
        return reader.Transmit(apdu, len, resp);
    };
    tr.max_apdu_len = friio::kMtvu3sCardMaxApdu;

    dyna::B25Decoder b25;
    assert(b25.Init(std::move(tr), 4, 1));
    assert(b25.IsReady());
    assert(!b25.IsSynced());
    assert(usb.resets == 1);

    // 未同期時のスクランブルパケットは頭切りされて出力されないこと
    std::vector<uint8_t> pre_pkt(188, 0xff);
    pre_pkt[0] = 0x47;
    pre_pkt[1] = 0x01;
    pre_pkt[2] = 0x00;
    pre_pkt[3] = 0xd0; // tsc=3 (scrambled)
    auto trimmed = b25.DecodeStream(pre_pkt.data(), pre_pkt.size(), true);
    assert(trimmed.empty());
    assert(!b25.IsSynced());
}

// recfriio issue #4 のログにある実際の TMCC の一覧で、BSnn_s の s を並びとして選ぶこと。
void TestPickBsTsidBySlot() {
    using friio::PickBsTsidBySlot;
    const std::vector<uint16_t> bs09 = {0x4090, 0x4092, 0xffff, 0xffff, 0xfff8, 0xffff,
                                        0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
    const std::vector<uint16_t> bs23 = {0x4770, 0x4971, 0xfff9, 0x4973, 0xffff, 0xffff,
                                        0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
    const std::vector<uint16_t> bs15 = {0x40f1, 0x40f2, 0x48f3, 0xffff, 0xffff, 0xffff,
                                        0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
    uint16_t id = 0;
    assert(PickBsTsidBySlot(0, bs09, id) && id == 0x4090);
    assert(PickBsTsidBySlot(1, bs09, id) && id == 0x4092); // 以前は 0x4091（無い）を頼んでいた
    assert(!PickBsTsidBySlot(2, bs09, id));
    assert(PickBsTsidBySlot(0, bs23, id) && id == 0x4770);
    assert(PickBsTsidBySlot(1, bs23, id) && id == 0x4971); // 空き（0xfff9）は数えない
    assert(PickBsTsidBySlot(2, bs23, id) && id == 0x4973);
    assert(!PickBsTsidBySlot(3, bs23, id));
    // BS15 は 1 つずらさなくても、並びで数えれば conf どおりになる
    assert(PickBsTsidBySlot(0, bs15, id) && id == 0x40f1);
    assert(PickBsTsidBySlot(2, bs15, id) && id == 0x48f3);
    assert(!PickBsTsidBySlot(-1, bs15, id));
    const std::vector<uint16_t> empty(12, 0xffff);
    assert(!PickBsTsidBySlot(0, empty, id));
    assert(friio::CountTmccTsids(bs23) == 3);
    assert(friio::CountTmccTsids(empty) == 0);
    assert(friio::CountTmccTsids(std::vector<uint16_t>(12, 0x0000)) == 0);
}

void TestMatchTsid() {
    using friio::MatchTsid;
    const std::vector<uint16_t> cs = {0x7040, 0xffff};
    uint16_t id = 0;
    assert(MatchTsid(0x7040, cs, id) && id == 0x7040);
    assert(MatchTsid(0x6040, cs, id) && id == 0x7040); // ネットワークのビットを無視
    assert(!MatchTsid(0x7060, cs, id));
    const std::vector<uint16_t> bs = {0x4770, 0x4971};
    assert(MatchTsid(0x4171, bs, id) && id == 0x4971); // 登録年のビットを無視
    assert(!MatchTsid(0, bs, id));
}

} // namespace

int main() {
    TestAsieStreamCipherNullPacket();
    TestMulti2RoundTrip();
    TestSettler();
    TestMtvu3sCardInitialSettingAndEcm();
    TestMtvu3sCardBadLrcRequiresReset();
    TestMtvu3sCardRejectsShortAtrTransfer();
    TestB25DecoderWithMtvu3sCard();
    TestPickBsTsidBySlot();
    TestMatchTsid();
    std::cout << "ALL FRIIO CORE TESTS PASSED\n";
    return 0;
}
