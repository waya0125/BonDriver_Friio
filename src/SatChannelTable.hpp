// SPDX-License-Identifier: MIT
// INI [CHANNEL] format inspired by n2naokun/BonDriver_U3S (MIT).
// Copyright (c) 2026 waya0125
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace friio {

struct SatChannelEntry {
    std::string name;      // UTF-8 表示名
    uint32_t freq_khz = 0; // 周波数 (kHz)
    uint16_t tsid = 0;     // TSID (1..65519; 既定 BS は 0 で slot を使用)
    int slot = -1;         // 0 以上なら TMCC 一覧の並び順で選ぶ（既定 BS 用）
};

struct SatSpaceEntry {
    std::string name; // UTF-8 空間名 (例: "BS", "CS110")
    std::vector<SatChannelEntry> channels;
};

// MonsterTV U3S 用の衛星（BS / CS110）選局表
class SatChannelTable {
public:
    // 既定の固定表（BS 48ch + CS 12ch）を返す
    static SatChannelTable Default();

    // INI テキスト内の [CHANNEL] セクションを解析する。
    // 有効な衛星空間・チャンネルが 1 つ以上あれば true を返す。
    static bool ParseIniText(const std::string& text, SatChannelTable& out);

    // 周波数番号 (0..11=BS01..23, 12..23=CS2..24) を kHz に変換する
    static bool FreqIndexToKHz(int freq_index, uint32_t& freq_khz);

    size_t SpaceCount() const { return spaces_.size(); }
    const SatSpaceEntry* Space(size_t idx) const {
        return idx < spaces_.size() ? &spaces_[idx] : nullptr;
    }
    const SatChannelEntry* Channel(size_t space_idx, size_t ch_idx) const {
        if (space_idx >= spaces_.size()) return nullptr;
        const auto& chs = spaces_[space_idx].channels;
        return ch_idx < chs.size() ? &chs[ch_idx] : nullptr;
    }

private:
    std::vector<SatSpaceEntry> spaces_;
};

} // namespace friio
