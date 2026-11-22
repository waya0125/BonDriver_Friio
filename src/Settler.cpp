// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#include "Settler.hpp"

namespace dyna {

Settler::Settler(int clean, int max_drop) : clean_(clean), max_drop_(max_drop) {}

void Settler::Reset() {
    non_null_run_ = 0;
    seen_pat_ = false;
    pending_.clear();
    last_cc_.clear();
    done_ = false;
    dropped_ = 0;
    gave_up_ = false;
}

void Settler::ResetRun() {
    if (done_) return;
    dropped_ += static_cast<int>(pending_.size() / kPacketSize);
    non_null_run_ = 0;
    seen_pat_ = false;
    pending_.clear();
    last_cc_.clear();
}

std::vector<uint8_t> Settler::Feed(const uint8_t* packets, size_t len) {
    if (done_) {
        return std::vector<uint8_t>(packets, packets + len);
    }

    for (size_t p = 0; p + kPacketSize <= len; p += kPacketSize) {
        const uint8_t* pkt = packets + p;
        bool bad = (pkt[0] != 0x47) || ((pkt[1] & 0x80) != 0);
        bool is_null = false;

        if (!bad) {
            const uint16_t pid = uint16_t((pkt[1] & 0x1f) << 8) | uint16_t(pkt[2]);
            if (pid == 0x1fff) {
                is_null = true;
            } else {
                if (pid == 0) {
                    seen_pat_ = true;
                }
                const uint8_t afc = (pkt[3] >> 4) & 0x03;
                if ((afc & 0x01) != 0) {
                    const uint8_t cc = pkt[3] & 0x0f;
                    const bool disc = ((afc & 0x02) != 0) && (pkt[4] > 0) && ((pkt[5] & 0x80) != 0);
                    auto it = last_cc_.find(pid);
                    if (it != last_cc_.end() && !disc) {
                        if (cc != ((it->second + 1) & 0x0f) && cc != it->second) {
                            bad = true;
                        }
                    }
                    if (!bad) {
                        last_cc_[pid] = cc;
                    }
                }
            }
        }

        if (bad) {
            dropped_ += static_cast<int>(pending_.size() / kPacketSize) + 1;
            non_null_run_ = 0;
            seen_pat_ = false;
            pending_.clear();
            last_cc_.clear();
        } else {
            if (!is_null) {
                ++non_null_run_;
            }
            pending_.insert(pending_.end(), pkt, pkt + kPacketSize);
        }

        if (seen_pat_ && non_null_run_ >= clean_) {
            done_ = true;
            std::vector<uint8_t> out;
            out.reserve(pending_.size() + (len - (p + kPacketSize)));
            out.insert(out.end(), pending_.begin(), pending_.end());
            out.insert(out.end(), packets + p + kPacketSize, packets + len);
            pending_.clear();
            pending_.shrink_to_fit();
            last_cc_.clear();
            return out;
        }

        if (dropped_ + static_cast<int>(pending_.size() / kPacketSize) >= max_drop_) {
            done_ = true;
            gave_up_ = true;
            std::vector<uint8_t> out;
            out.reserve(pending_.size() + (len - (p + kPacketSize)));
            out.insert(out.end(), pending_.begin(), pending_.end());
            out.insert(out.end(), packets + p + kPacketSize, packets + len);
            pending_.clear();
            pending_.shrink_to_fit();
            last_cc_.clear();
            return out;
        }
    }

    return {};
}

} // namespace dyna
