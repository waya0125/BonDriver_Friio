// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#include "UsbSelect.hpp"

namespace friio {

int SelectDevice(size_t count, int device_index,
                 const std::function<DeviceProbe(size_t)>& probe,
                 const std::function<void(size_t)>& release) {
    int tuner_no = 0;
    for (size_t i = 0; i < count; ++i) {
        const DeviceProbe r = probe(i);
        if (device_index < 0) {
            if (r == DeviceProbe::Opened) return static_cast<int>(i);
            continue;
        }
        if (r == DeviceProbe::NotTuner) continue;
        if (tuner_no == device_index) {
            // 指定の番目が使用中なら失敗（別の台にずらすと、2 つの INI が同じ台を取り合う）
            return r == DeviceProbe::Opened ? static_cast<int>(i) : -1;
        }
        if (r == DeviceProbe::Opened) release(i);
        ++tuner_no;
    }
    return -1;
}

int ParseDeviceIndex(const std::wstring& text, int fallback) {
    size_t b = 0;
    size_t e = text.size();
    while (b < e && (text[b] == L' ' || text[b] == L'\t')) ++b;
    while (e > b && (text[e - 1] == L' ' || text[e - 1] == L'\t' || text[e - 1] == L'\r')) --e;
    if (b == e) return fallback;
    bool neg = false;
    if (text[b] == L'-' || text[b] == L'+') {
        neg = (text[b] == L'-');
        ++b;
    }
    if (b == e) return fallback;
    long long v = 0;
    for (size_t i = b; i < e; ++i) {
        if (text[i] < L'0' || text[i] > L'9') return fallback;
        v = v * 10 + (text[i] - L'0');
        if (v > 255) return fallback; // 現実の接続台数を大きく超える値は打ち間違いとみなす
    }
    return neg ? -static_cast<int>(v) : static_cast<int>(v);
}

size_t ChooseIsochPacketSize(bool queried, uint32_t max_bytes_per_interval, size_t fallback) {
    // SuperSpeed の Isochronous は 1 サービス間隔あたり最大 1024 x 16 バースト x 3 = 49152 バイト
    constexpr uint32_t kMaxBytesPerInterval = 1024u * 16u * 3u;
    if (!queried || max_bytes_per_interval == 0 || max_bytes_per_interval > kMaxBytesPerInterval) {
        return fallback;
    }
    return max_bytes_per_interval;
}

const char* UsbSpeedName(uint8_t speed) {
    switch (speed) {
    case 1: return "LowSpeed";
    case 2: return "FullSpeed";
    case 3: return "HighSpeed";
    default: return "Unknown";
    }
}

} // namespace friio
