// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dyna {

constexpr size_t kPacketSize = 188;
constexpr int kDefaultSettleClean = 5000;
constexpr int kDefaultSettleMaxDrop = 200000;

class Settler {
public:
    Settler(int clean = kDefaultSettleClean, int max_drop = kDefaultSettleMaxDrop);

    void Reset();
    void ResetRun();
    std::vector<uint8_t> Feed(const uint8_t* packets, size_t len);

    bool Settled() const { return done_; }
    bool GaveUp() const { return gave_up_; }
    int Dropped() const { return dropped_; }

private:
    int clean_;
    int max_drop_;

    int non_null_run_ = 0;
    bool seen_pat_ = false;
    std::vector<uint8_t> pending_;
    std::unordered_map<uint16_t, uint8_t> last_cc_;
    bool done_ = false;
    int dropped_ = 0;
    bool gave_up_ = false;
};

} // namespace dyna
