// SPDX-License-Identifier: MIT
// Ported from arib25 / recdyna (internal/bcas, internal/b25).
// Copyright (c) 2026 waya0125
#include "B25Decoder.hpp"

#include <cstring>

namespace dyna {

namespace {

inline uint16_t ReadBE16(const uint8_t* p) {
    return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

// MPEG-2 CRC32 (polynomial 0x04c11db7, init 0xffffffff, no final xor)
uint32_t Mpeg2Crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= uint32_t(data[i]) << 24;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80000000u) ? ((crc << 1) ^ 0x04c11db7u) : (crc << 1);
        }
    }
    return crc;
}

constexpr uint8_t kCmdInitialSetting[] = {0x90, 0x30, 0x00, 0x00, 0x00};

} // namespace

void SectionAssembler::Reset() {
    buf_.clear();
    expected_ = 0;
    active_ = false;
}

std::vector<std::vector<uint8_t>> SectionAssembler::PushPacket(const uint8_t pkt[188]) {
    std::vector<std::vector<uint8_t>> out;
    if (pkt[0] != 0x47 || (pkt[1] & 0x80) != 0) return out;
    const bool pusi = (pkt[1] & 0x40) != 0;
    const uint8_t afc = (pkt[3] >> 4) & 0x03;
    if ((afc & 0x01) == 0) return out;

    size_t pos = 4;
    if (afc & 0x02) {
        pos += 1 + pkt[4];
    }
    if (pos >= 188) return out;

    size_t ptr = 0;
    if (pusi) {
        ptr = pkt[pos];
        ++pos;
        if (pos + ptr > 188) {
            Reset();
            return out;
        }
        if (active_ && ptr > 0) {
            buf_.insert(buf_.end(), pkt + pos, pkt + pos + ptr);
            if (expected_ > 0 && buf_.size() >= expected_) {
                out.emplace_back(buf_.begin(), buf_.begin() + static_cast<ptrdiff_t>(expected_));
            }
            Reset();
        }
        pos += ptr;
    }

    if (pusi) {
        while (pos < 188) {
            if (pkt[pos] == 0xff) break;
            if (pos + 3 > 188) {
                buf_.assign(pkt + pos, pkt + 188);
                expected_ = 0;
                active_ = true;
                break;
            }
            const size_t sec_len = size_t(((pkt[pos + 1] & 0x0f) << 8) | pkt[pos + 2]) + 3;
            if (sec_len < 3 || sec_len > 4096) {
                Reset();
                break;
            }
            if (pos + sec_len <= 188) {
                out.emplace_back(pkt + pos, pkt + pos + sec_len);
                pos += sec_len;
            } else {
                buf_.assign(pkt + pos, pkt + 188);
                expected_ = sec_len;
                active_ = true;
                break;
            }
        }
    } else if (active_) {
        buf_.insert(buf_.end(), pkt + pos, pkt + 188);
        if (expected_ == 0 && buf_.size() >= 3) {
            expected_ = size_t(((buf_[1] & 0x0f) << 8) | buf_[2]) + 3;
            if (expected_ < 3 || expected_ > 4096) {
                Reset();
                return out;
            }
        }
        if (expected_ > 0 && buf_.size() >= expected_) {
            out.emplace_back(buf_.begin(), buf_.begin() + static_cast<ptrdiff_t>(expected_));
            Reset();
        }
    }
    return out;
}

bool B25Decoder::Init(BcasTransport transport, int multi2_round) {
    transport_ = std::move(transport);
    round_ = multi2_round;
    ResetStream();
    ready_ = ConnectCard();
    return ready_;
}

bool B25Decoder::ConnectCard() {
    if (!transport_.reset || !transport_.transmit) return false;
    if (!transport_.reset()) return false;

    std::vector<uint8_t> r;
    if (!transport_.transmit(kCmdInitialSetting, sizeof(kCmdInitialSetting), r)) return false;
    if (r.size() < 57) return false;
    if (ReadBE16(r.data() + 4) != 0x2100) return false;

    std::memcpy(system_key_.data(), r.data() + 16, 32);
    std::memcpy(init_cbc_.data(), r.data() + 48, 8);
    ca_system_id_ = ReadBE16(r.data() + 6);
    return true;
}

bool B25Decoder::ProcessECM(const uint8_t* ecm, size_t len, uint8_t ks_out[16], uint16_t& return_code) {
    if (len < 1 || len > 0xff) return false;
    std::vector<uint8_t> cmd;
    cmd.reserve(6 + len);
    cmd.push_back(0x90);
    cmd.push_back(0x34);
    cmd.push_back(0x00);
    cmd.push_back(0x00);
    cmd.push_back(static_cast<uint8_t>(len));
    cmd.insert(cmd.end(), ecm, ecm + len);
    cmd.push_back(0x00);

    std::vector<uint8_t> r;
    bool ok = transport_.transmit(cmd.data(), cmd.size(), r) && r.size() >= 25;
    for (int retry = 0; !ok && retry < 3; ++retry) {
        if (!ConnectCard()) continue;
        ok = transport_.transmit(cmd.data(), cmd.size(), r) && r.size() >= 25;
    }
    if (!ok) return false;

    return_code = ReadBE16(r.data() + 4);
    std::memcpy(ks_out, r.data() + 6, 16);
    return true;
}

void B25Decoder::ResetStream() {
    pat_asm_.Reset();
    pmt_pids_.clear();
    pmt_asms_.clear();
    ecm_asms_.clear();
    ecm_states_.clear();
    es_to_ecm_.clear();
}

void B25Decoder::HandlePAT(const std::vector<uint8_t>& sec) {
    if (sec.size() < 12 || sec[0] != 0x00) return;
    if (Mpeg2Crc32(sec.data(), sec.size()) != 0) return;

    std::unordered_set<uint16_t> next_pmts;
    for (size_t i = 8; i + 4 <= sec.size() - 4; i += 4) {
        const uint16_t prog_num = ReadBE16(sec.data() + i);
        const uint16_t pid = ReadBE16(sec.data() + i + 2) & 0x1fff;
        if (prog_num != 0) {
            next_pmts.insert(pid);
        }
    }
    pmt_pids_ = std::move(next_pmts);
}

void B25Decoder::HandlePMT(uint16_t /*pmt_pid*/, const std::vector<uint8_t>& sec) {
    if (sec.size() < 16 || sec[0] != 0x02) return;
    if (Mpeg2Crc32(sec.data(), sec.size()) != 0) return;

    const size_t prog_info_len = ReadBE16(sec.data() + 10) & 0x0fff;
    size_t pos = 12;
    if (pos + prog_info_len > sec.size() - 4) return;

    uint16_t prog_ecm_pid = 0;
    auto parse_ca_desc = [&](size_t start, size_t end, uint16_t& out_ecm_pid) {
        for (size_t p = start; p + 2 <= end; ) {
            const uint8_t tag = sec[p];
            const uint8_t dlen = sec[p + 1];
            if (p + 2 + dlen > end) break;
            if (tag == 0x09 && dlen >= 4) {
                const uint16_t sys_id = ReadBE16(sec.data() + p + 2);
                const uint16_t ca_pid = ReadBE16(sec.data() + p + 4) & 0x1fff;
                if (sys_id == ca_system_id_) {
                    out_ecm_pid = ca_pid;
                }
            }
            p += 2 + dlen;
        }
    };

    parse_ca_desc(pos, pos + prog_info_len, prog_ecm_pid);
    pos += prog_info_len;

    while (pos + 5 <= sec.size() - 4) {
        const uint16_t es_pid = ReadBE16(sec.data() + pos + 1) & 0x1fff;
        const size_t es_info_len = ReadBE16(sec.data() + pos + 3) & 0x0fff;
        pos += 5;
        if (pos + es_info_len > sec.size() - 4) break;

        uint16_t es_ecm_pid = prog_ecm_pid;
        parse_ca_desc(pos, pos + es_info_len, es_ecm_pid);
        pos += es_info_len;

        if (es_ecm_pid != 0) {
            es_to_ecm_[es_pid] = es_ecm_pid;
            if (ecm_states_.find(es_ecm_pid) == ecm_states_.end()) {
                EcmState st;
                st.cipher.Init(system_key_.data(), init_cbc_.data(), round_);
                ecm_states_.emplace(es_ecm_pid, std::move(st));
            }
        }
    }
}

void B25Decoder::HandleECM(uint16_t ecm_pid, const std::vector<uint8_t>& sec) {
    if (sec.size() < 8 || sec[0] != 0x82) return;
    auto it = ecm_states_.find(ecm_pid);
    if (it == ecm_states_.end()) return;

    // section_syntax_indicator == 1 の場合は 8 バイトヘッダ、0 の場合は 3 バイトヘッダ（末尾 4B は CRC）
    const size_t hdr_len = (sec[1] & 0x80) ? 8 : 3;
    if (sec.size() <= hdr_len + 4) return;
    const uint8_t* payload = sec.data() + hdr_len;
    const size_t plen = sec.size() - hdr_len - 4;
    if (it->second.last_ecm.size() == plen &&
        std::memcmp(it->second.last_ecm.data(), payload, plen) == 0) {
        return;
    }

    uint8_t ks[16]{};
    uint16_t rc = 0;
    if (ProcessECM(payload, plen, ks, rc)) {
        it->second.last_ecm.assign(payload, payload + plen);
        if (rc == 0x0800 || rc == 0x0400 || rc == 0x0200 || rc == 0x0000) {
            it->second.cipher.SetScrambleKey(ks);
        } else {
            it->second.cipher.ClearScrambleKey();
        }
    }
}

void B25Decoder::DecodePackets(uint8_t* packets, size_t len) {
    if (!ready_) return;
    for (size_t p = 0; p + 188 <= len; p += 188) {
        uint8_t* pkt = packets + p;
        if (pkt[0] != 0x47 || (pkt[1] & 0x80) != 0) continue;

        const uint16_t pid = uint16_t((pkt[1] & 0x1f) << 8) | uint16_t(pkt[2]);
        const uint8_t tsc = (pkt[3] >> 6) & 0x03;

        if (pid == 0x0000 && tsc == 0) {
            for (const auto& sec : pat_asm_.PushPacket(pkt)) HandlePAT(sec);
            continue;
        }
        if (tsc == 0 && pmt_pids_.count(pid)) {
            for (const auto& sec : pmt_asms_[pid].PushPacket(pkt)) HandlePMT(pid, sec);
            continue;
        }
        if (tsc == 0 && ecm_states_.count(pid)) {
            for (const auto& sec : ecm_asms_[pid].PushPacket(pkt)) HandleECM(pid, sec);
            continue;
        }

        if (tsc >= 2) {
            auto map_it = es_to_ecm_.find(pid);
            if (map_it == es_to_ecm_.end()) continue;
            auto ecm_it = ecm_states_.find(map_it->second);
            if (ecm_it == ecm_states_.end() || !ecm_it->second.cipher.Keyed()) continue;

            const uint8_t afc = (pkt[3] >> 4) & 0x03;
            if ((afc & 0x01) == 0) continue;
            size_t payload_off = 4;
            if (afc & 0x02) {
                payload_off += 1 + pkt[4];
            }
            if (payload_off >= 188) continue;

            ecm_it->second.cipher.Decrypt(tsc, pkt + payload_off, 188 - payload_off);
            pkt[3] &= 0x3f; // clear transport_scrambling_control
        }
    }
}

} // namespace dyna
