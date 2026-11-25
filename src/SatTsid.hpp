// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace friio {

// TMCC の TSID の一覧で、空きを表す値か（実機では 0xfff8・0xfff9 が見える。0x0000 も空き扱い）。
bool IsTmccPlaceholder(uint16_t id);

// 空きを除いた TSID の数。
size_t CountTmccTsids(const std::vector<uint16_t>& ids);

// want に当たる TSID を TMCC の一覧 ids から選ぶ（recfriio の MatchTSID と同じ）。
// 完全一致が無ければ、BS は登録年のビット（0x0e00）、CS はネットワークのビット（0x1000）を無視して当てる。
bool MatchTsid(uint16_t want, const std::vector<uint16_t>& ids, uint16_t& matched);

// BS の「BSnn_s」の s（そのトランスポンダーの何番目の TS か）から TSID を選ぶ。
// TMCC の一覧から空きを除いた並びの slot 番目。recpt1・recisdb・Mirakurun の channels.yml と同じ読み方。
bool PickBsTsidBySlot(int slot, const std::vector<uint16_t>& ids, uint16_t& picked);

} // namespace friio
