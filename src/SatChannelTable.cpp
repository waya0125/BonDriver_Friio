// SPDX-License-Identifier: MIT
// INI [CHANNEL] format inspired by n2naokun/BonDriver_U3S (MIT).
// Copyright (c) 2026 waya0125
#include "SatChannelTable.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>

namespace friio {

namespace {

constexpr uint16_t kDefaultCsTsids[12] = {
    0x6020, 0x7040, 0x7060, 0x6080, 0x60a0, 0x70c0,
    0x70e0, 0x7100, 0x7120, 0x7140, 0x7160, 0x7180,
};

std::string Trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    size_t e = s.size();
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return s.substr(b, e - b);
}

bool ParseIntStrict(const std::string& s, int& out) {
    const std::string t = Trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(t.c_str(), &end, 10);
    if (end == t.c_str() || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

std::vector<std::string> SplitTab(const std::string& line) {
    std::vector<std::string> cols;
    size_t pos = 0;
    while (true) {
        size_t tab = line.find('\t', pos);
        if (tab == std::string::npos) {
            cols.push_back(line.substr(pos));
            break;
        }
        cols.push_back(line.substr(pos, tab - pos));
        pos = tab + 1;
    }
    return cols;
}

std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace

bool SatChannelTable::FreqIndexToKHz(int freq_index, uint32_t& freq_khz) {
    if (freq_index >= 0 && freq_index <= 11) {
        freq_khz = 1049480u + static_cast<uint32_t>(freq_index) * 38360u;
        return true;
    }
    if (freq_index >= 12 && freq_index <= 23) {
        freq_khz = 1613000u + static_cast<uint32_t>(freq_index - 12) * 40000u;
        return true;
    }
    return false;
}

SatChannelTable SatChannelTable::Default() {
    SatChannelTable tbl;

    SatSpaceEntry bs;
    bs.name = "BS";
    bs.channels.reserve(48);
    for (uint32_t ch = 0; ch < 48; ++ch) {
        const uint32_t tp = ch / 4; // 0..11 => BS01..BS23
        const uint32_t bs_num = tp * 2 + 1;
        const int slot = static_cast<int>(ch % 4);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "BS%02u_%u", bs_num, ch % 4);
        SatChannelEntry e;
        e.name = buf;
        e.freq_khz = 1049480u + tp * 38360u;
        e.tsid = 0;
        e.slot = slot;
        bs.channels.push_back(std::move(e));
    }

    SatSpaceEntry cs;
    cs.name = "CS";
    cs.channels.reserve(12);
    for (uint32_t ch = 0; ch < 12; ++ch) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "CS%u", (ch + 1) * 2);
        SatChannelEntry e;
        e.name = buf;
        e.freq_khz = 1613000u + ch * 40000u;
        e.tsid = kDefaultCsTsids[ch];
        cs.channels.push_back(std::move(e));
    }

    tbl.spaces_.push_back(std::move(bs));
    tbl.spaces_.push_back(std::move(cs));
    return tbl;
}

bool SatChannelTable::ParseIniText(const std::string& text, SatChannelTable& out) {
    // UTF-8 BOM を除去
    size_t start = 0;
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xef &&
        static_cast<unsigned char>(text[1]) == 0xbb &&
        static_cast<unsigned char>(text[2]) == 0xbf) {
        start = 3;
    }

    std::istringstream iss(text.substr(start));
    std::string raw_line;
    bool in_channel = false;

    std::map<int, std::string> space_names;
    std::map<int, std::map<int, SatChannelEntry>> space_channels;

    while (std::getline(iss, raw_line)) {
        if (!raw_line.empty() && raw_line.back() == '\r') raw_line.pop_back();
        const std::string trimmed = Trim(raw_line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            const std::string sec = ToLowerAscii(Trim(trimmed.substr(1, trimmed.size() - 2)));
            in_channel = (sec == "channel");
            continue;
        }
        if (!in_channel) continue;

        auto cols = SplitTab(raw_line);
        if (cols.size() == 2 && !cols[0].empty() && cols[0][0] == '$') {
            int sp_idx = -1;
            const std::string sp_name = Trim(cols[0].substr(1));
            if (!sp_name.empty() && ParseIntStrict(cols[1], sp_idx) && sp_idx >= 0) {
                space_names[sp_idx] = sp_name;
            }
            continue;
        }

        if (cols.size() >= 5) {
            const std::string ch_name = Trim(cols[0]);
            int sp_idx = -1, ch_idx = -1, freq_idx = -1, tsid = -1;
            uint32_t freq_khz = 0;
            if (!ch_name.empty() &&
                ParseIntStrict(cols[1], sp_idx) && sp_idx >= 0 &&
                ParseIntStrict(cols[2], ch_idx) && ch_idx >= 0 &&
                ParseIntStrict(cols[3], freq_idx) && FreqIndexToKHz(freq_idx, freq_khz) &&
                ParseIntStrict(cols[4], tsid) && tsid >= 1 && tsid <= 65519) {
                SatChannelEntry e;
                e.name = ch_name;
                e.freq_khz = freq_khz;
                e.tsid = static_cast<uint16_t>(tsid);
                space_channels[sp_idx][ch_idx] = std::move(e);
            }
        }
    }

    if (space_names.empty()) return false;

    SatChannelTable result;
    for (const auto& kv : space_names) {
        const int sp_idx = kv.first;
        auto it = space_channels.find(sp_idx);
        if (it == space_channels.end() || it->second.empty()) continue;

        SatSpaceEntry sp;
        sp.name = kv.second;
        for (const auto& ckv : it->second) {
            sp.channels.push_back(ckv.second);
        }
        result.spaces_.push_back(std::move(sp));
    }

    if (result.spaces_.empty()) return false;
    out = std::move(result);
    return true;
}

} // namespace friio
