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
class WinUsbDevice {
public:
    WinUsbDevice() = default;
    ~WinUsbDevice();

    WinUsbDevice(const WinUsbDevice&) = delete;
    WinUsbDevice& operator=(const WinUsbDevice&) = delete;

    static std::vector<std::wstring> Enumerate(uint16_t vid, uint16_t pid);

    bool Open(const std::wstring& device_path);
    void Close();
    bool IsOpen() const { return winusb_ != nullptr; }

    bool SetAlternateSetting(uint8_t alt_setting);
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
