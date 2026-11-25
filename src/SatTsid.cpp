// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#include "SatTsid.hpp"

namespace friio {

namespace {
constexpr uint16_t kBsYearMask = 0x0e00;
constexpr uint16_t kCsNetworkMask = 0x1000;
} // namespace

bool IsTmccPlaceholder(uint16_t id) {
    return id == 0x0000 || id >= 0xfff0;
}

size_t CountTmccTsids(const std::vector<uint16_t>& ids) {
    size_t n = 0;
    for (uint16_t id : ids) {
        if (!IsTmccPlaceholder(id)) ++n;
    }
    return n;
}

bool MatchTsid(uint16_t want, const std::vector<uint16_t>& ids, uint16_t& matched) {
    if (want == 0) return false;
    for (uint16_t id : ids) {
        if (id == want) { matched = id; return true; }
    }
    if ((want & 0xf000) == 0x4000) {
        for (uint16_t id : ids) {
            if ((id & 0xf000) == 0x4000 && (id & ~kBsYearMask) == (want & ~kBsYearMask)) {
                matched = id;
                return true;
            }
        }
    }
    if ((want & 0xe000) == 0x6000) {
        for (uint16_t id : ids) {
            if ((id & 0xe000) == 0x6000 && (id & ~kCsNetworkMask) == (want & ~kCsNetworkMask)) {
                matched = id;
                return true;
            }
        }
    }
    return false;
}

bool PickBsTsidBySlot(int slot, const std::vector<uint16_t>& ids, uint16_t& picked) {
    if (slot < 0) return false;
    int n = 0;
    for (uint16_t id : ids) {
        if (IsTmccPlaceholder(id)) continue;
        if (n == slot) { picked = id; return true; }
        ++n;
    }
    return false;
}

} // namespace friio
