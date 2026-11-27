// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#ifdef _WIN32

#include <windows.h>
#include <winusb.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace friio {

// WinUSB 直接呼び出しによる USB デバイスラッパー（Bulk + Win8.1+ Isochronous 対応）
// OutputDebugString へ書く（DebugView などで見る）。先頭に "[BonDriver_Friio] " を付ける。
void DebugLog(const char* fmt, ...);

// WinUSB から取得したパイプ情報
struct WinUsbPipeInfo {
    uint8_t pipe_type = 0;              // USBD_PIPE_TYPE（1=Isochronous, 2=Bulk, 3=Interrupt）
    uint8_t pipe_id = 0;                // エンドポイントアドレス
    uint16_t max_packet_size = 0;       // wMaxPacketSize
    uint8_t interval = 0;               // bInterval
    uint32_t max_bytes_per_interval = 0; // 1 サービス間隔あたりの最大バイト数（QueryPipeEx のみ）
    bool from_query_pipe_ex = false;    // WinUsb_QueryPipeEx（Win8.1+）で取れたか
};

class WinUsbDevice {
public:
    WinUsbDevice() = default;
    ~WinUsbDevice();

    WinUsbDevice(const WinUsbDevice&) = delete;
    WinUsbDevice& operator=(const WinUsbDevice&) = delete;

    static std::vector<std::wstring> Enumerate(uint16_t vid, uint16_t pid);

    // device_path のデバイスに WinUSB ドライバーが割り当てられているか（使用中でも判定できる）
    static bool IsWinUsbBound(const std::wstring& device_path);

    bool Open(const std::wstring& device_path);
    // DeviceIndex に従って paths から 1 台開く（device_index < 0 は空きデバイス自動選択）。
    // 開いた候補の番号を返す。開けなければ -1。
    int OpenByIndex(const std::vector<std::wstring>& paths, int device_index);
    void Close();
    bool IsOpen() const { return winusb_ != nullptr; }

    bool SetAlternateSetting(uint8_t alt_setting);
    // WinUsb_QueryDeviceInformation(DEVICE_SPEED): 1=LowSpeed, 2=FullSpeed, 3=HighSpeed
    bool QueryDeviceSpeed(uint8_t& speed);
    // alt_setting のエンドポイント ep のパイプ情報。Win8.1+ は WinUsb_QueryPipeEx、
    // それ以前は WinUsb_QueryPipe（max_bytes_per_interval は 0）で取る。
    bool QueryPipeInfo(uint8_t alt_setting, uint8_t ep, WinUsbPipeInfo& out);
    bool ControlTransfer(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
                         uint8_t* data, uint16_t len, uint32_t timeout_ms, size_t* transferred = nullptr);

    using ChunkCallback = std::function<void(const uint8_t* data, size_t len)>;
    bool StartAsyncBulkStream(uint8_t ep, size_t num_urbs, size_t urb_size, ChunkCallback cb);
    // Win8.1+ 専用（Win7 では false を返す）
    bool StartAsyncIsochStream(uint8_t ep, size_t num_urbs, size_t iso_packet_size, size_t iso_packets, ChunkCallback cb);
    void StopAsyncStream();

private:
    HANDLE dev_handle_ = INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE winusb_ = nullptr;

    std::atomic<bool> streaming_{false};
    std::thread stream_thread_;
    uint8_t stream_ep_ = 0;
};

} // namespace friio

#endif // _WIN32
