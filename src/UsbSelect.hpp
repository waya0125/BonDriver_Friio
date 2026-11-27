// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace friio {

// 候補のデバイスを開いてみた結果
enum class DeviceProbe {
    Opened,   // 開けた（開いたまま返す）
    Busy,     // このチューナーだが、別のプロセスが使用中などで開けない
    NotTuner, // チューナーとして数えない候補（WinUSB が割り当てられていない親デバイスなど）
};

// DeviceIndex に従って開くデバイスを選ぶ。
//   device_index <  0: 空きデバイス自動選択。最初に開けた候補を採用する。
//   device_index >= 0: NotTuner を除いた並びで device_index 番目の候補に固定する。
//                      その候補が使用中なら、他の候補にずらさず失敗にする。
// probe(i) で候補 i を開いてみる。Opened のときは開いたままにし、
// 採用しなかった候補は release(i) で閉じる。
// 採用した候補の番号を返す。採用できなければ -1。
int SelectDevice(size_t count, int device_index,
                 const std::function<DeviceProbe(size_t)>& probe,
                 const std::function<void(size_t)>& release);

// INI の DeviceIndex の既定値（キーが無いとき）。-1 = 空きデバイス自動選択
constexpr int kAutoDeviceIndex = -1;

// INI の DeviceIndex の文字列を読む（前後の空白と符号を許す）。空・数字でない・範囲外は fallback。
// GetPrivateProfileIntW は負の値の扱いがドキュメント上はっきりしないので、文字列で読んで自前で解釈する。
int ParseDeviceIndex(const std::wstring& text, int fallback);

// MonsterTV U3S の Isochronous パケットサイズの既定値（Alt 1 / EP 0x84 の wMaxPacketSize）
constexpr size_t kDefaultIsochPacketSize = 940;

// WinUsb_QueryPipeEx の MaximumBytesPerInterval から Isochronous のパケットサイズを決める。
// 取得できなかった・0・範囲外（SuperSpeed の上限 48 KiB 超）のときは fallback を返す。
size_t ChooseIsochPacketSize(bool queried, uint32_t max_bytes_per_interval, size_t fallback);

// WinUsb_QueryDeviceInformation(DEVICE_SPEED) の値の名前（ログ用）
const char* UsbSpeedName(uint8_t speed);

} // namespace friio
