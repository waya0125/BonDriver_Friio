// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#ifdef _WIN32

#include "WinUsbDevice.hpp"

#include <setupapi.h>
#include <usbiodef.h>

#include <algorithm>
#include <cwctype>

namespace friio {

namespace {

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

std::wstring FormatVidPid(uint16_t vid, uint16_t pid) {
    wchar_t buf[32];
    swprintf(buf, 32, L"vid_%04x&pid_%04x", vid, pid);
    return std::wstring(buf);
}

// Win8.1+ Isochronous WinUSB API を動的解決する（Windows 7 でも DLL ロードが失敗しないように）
struct IsochPacketDesc {
    ULONG Offset;
    ULONG Length;
    ULONG Status;
};

using FnRegisterIsochBuffer = BOOL(WINAPI*)(WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PVOID*);
using FnUnregisterIsochBuffer = BOOL(WINAPI*)(PVOID);
using FnReadIsochPipeAsap = BOOL(WINAPI*)(PVOID, ULONG, ULONG, BOOL, ULONG, IsochPacketDesc*, LPOVERLAPPED);

struct WinUsbIsochApi {
    FnRegisterIsochBuffer Register = nullptr;
    FnUnregisterIsochBuffer Unregister = nullptr;
    FnReadIsochPipeAsap ReadAsap = nullptr;

    bool Load() {
        HMODULE mod = GetModuleHandleW(L"winusb.dll");
        if (!mod) mod = LoadLibraryW(L"winusb.dll");
        if (!mod) return false;
        Register = reinterpret_cast<FnRegisterIsochBuffer>(GetProcAddress(mod, "WinUsb_RegisterIsochBuffer"));
        Unregister = reinterpret_cast<FnUnregisterIsochBuffer>(GetProcAddress(mod, "WinUsb_UnregisterIsochBuffer"));
        ReadAsap = reinterpret_cast<FnReadIsochPipeAsap>(GetProcAddress(mod, "WinUsb_ReadIsochPipeAsap"));
        return Register && Unregister && ReadAsap;
    }
};

} // namespace

WinUsbDevice::~WinUsbDevice() {
    Close();
}

std::vector<std::wstring> WinUsbDevice::Enumerate(uint16_t vid, uint16_t pid) {
    std::vector<std::wstring> result;
    const std::wstring needle = FormatVidPid(vid, pid);

    HDEVINFO dev_info = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_USB_DEVICE, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_info == INVALID_HANDLE_VALUE) return result;

    SP_DEVICE_INTERFACE_DATA if_data{};
    if_data.cbSize = sizeof(if_data);
    for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(dev_info, nullptr, &GUID_DEVINTERFACE_USB_DEVICE, idx, &if_data); ++idx) {
        DWORD req_size = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, nullptr, 0, &req_size, nullptr);
        if (req_size == 0) continue;

        std::vector<uint8_t> detail_buf(req_size);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detail_buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, detail, req_size, nullptr, nullptr)) {
            std::wstring path(detail->DevicePath);
            if (ToLower(path).find(needle) != std::wstring::npos) {
                result.push_back(std::move(path));
            }
        }
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    std::sort(result.begin(), result.end());
    return result;
}

bool WinUsbDevice::Open(const std::wstring& device_path) {
    Close();
    dev_handle_ = CreateFileW(
        device_path.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (dev_handle_ == INVALID_HANDLE_VALUE) return false;

    if (!WinUsb_Initialize(dev_handle_, &winusb_)) {
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        winusb_ = nullptr;
        return false;
    }
    return true;
}

void WinUsbDevice::Close() {
    StopAsyncStream();
    if (winusb_) {
        WinUsb_Free(winusb_);
        winusb_ = nullptr;
    }
    if (dev_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool WinUsbDevice::SetAlternateSetting(uint8_t alt_setting) {
    if (!winusb_) return false;
    return WinUsb_SetCurrentAlternateSetting(winusb_, alt_setting) != FALSE;
}

bool WinUsbDevice::ControlTransfer(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
                                   uint8_t* data, uint16_t len, uint32_t timeout_ms, size_t* transferred) {
    if (!winusb_) return false;
    ULONG t = timeout_ms;
    WinUsb_SetPipePolicy(winusb_, 0x00, PIPE_TRANSFER_TIMEOUT, sizeof(t), &t);

    WINUSB_SETUP_PACKET pkt{};
    pkt.RequestType = request_type;
    pkt.Request = request;
    pkt.Value = value;
    pkt.Index = index;
    pkt.Length = len;

    ULONG got = 0;
    BOOL ok = WinUsb_ControlTransfer(winusb_, pkt, data, len, &got, nullptr);
    if (transferred) *transferred = got;
    return ok != FALSE;
}

bool WinUsbDevice::StartAsyncBulkStream(uint8_t ep, size_t num_urbs, size_t urb_size, ChunkCallback cb) {
    if (!winusb_) return false;
    StopAsyncStream();

    UCHAR raw_io = 1;
    WinUsb_SetPipePolicy(winusb_, ep, RAW_IO, sizeof(raw_io), &raw_io);
    WinUsb_FlushPipe(winusb_, ep);

    stream_ep_ = ep;
    streaming_.store(true);

    stream_thread_ = std::thread([this, ep, num_urbs, urb_size, cb = std::move(cb)]() {
        struct Slot {
            std::vector<uint8_t> buf;
            OVERLAPPED ov{};
            bool pending = false;
        };
        std::vector<Slot> slots(num_urbs);
        for (auto& s : slots) {
            s.buf.resize(urb_size);
            s.ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }

        auto submit = [&](Slot& s) {
            ResetEvent(s.ov.hEvent);
            ULONG got = 0;
            BOOL ok = WinUsb_ReadPipe(winusb_, ep, s.buf.data(), static_cast<ULONG>(s.buf.size()), &got, &s.ov);
            s.pending = (ok || GetLastError() == ERROR_IO_PENDING);
        };

        for (auto& s : slots) submit(s);

        size_t idx = 0;
        while (streaming_.load()) {
            auto& s = slots[idx];
            idx = (idx + 1) % num_urbs;
            if (!s.pending) { submit(s); continue; }
            DWORD w = WaitForSingleObject(s.ov.hEvent, 200);
            if (w == WAIT_TIMEOUT) continue;
            if (w != WAIT_OBJECT_0) break;

            ULONG transferred = 0;
            BOOL ok = WinUsb_GetOverlappedResult(winusb_, &s.ov, &transferred, FALSE);
            s.pending = false;
            if (ok && transferred > 0 && streaming_.load()) {
                cb(s.buf.data(), transferred);
            }
            if (streaming_.load()) submit(s);
        }

        WinUsb_AbortPipe(winusb_, ep);
        for (auto& s : slots) {
            if (s.pending) {
                ULONG dummy = 0;
                WinUsb_GetOverlappedResult(winusb_, &s.ov, &dummy, TRUE);
            }
            if (s.ov.hEvent) CloseHandle(s.ov.hEvent);
        }
    });
    return true;
}

bool WinUsbDevice::StartAsyncIsochStream(uint8_t ep, size_t num_urbs, size_t iso_packet_size, size_t iso_packets, ChunkCallback cb) {
    if (!winusb_) return false;
    WinUsbIsochApi api;
    if (!api.Load()) return false;

    StopAsyncStream();
    stream_ep_ = ep;
    streaming_.store(true);

    const size_t urb_bytes = iso_packet_size * iso_packets;
    stream_thread_ = std::thread([this, api, ep, num_urbs, iso_packets, urb_bytes, cb = std::move(cb)]() {
        struct Slot {
            std::vector<uint8_t> buf;
            std::vector<IsochPacketDesc> descs;
            PVOID reg_handle = nullptr;
            OVERLAPPED ov{};
            bool pending = false;
        };

        std::vector<Slot> slots(num_urbs);
        for (auto& s : slots) {
            s.buf.resize(urb_bytes);
            s.descs.resize(iso_packets);
            s.ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            api.Register(winusb_, ep, s.buf.data(), static_cast<ULONG>(s.buf.size()), &s.reg_handle);
        }

        bool first = true;
        auto submit = [&](Slot& s) {
            if (!s.reg_handle) return;
            ResetEvent(s.ov.hEvent);
            BOOL ok = api.ReadAsap(
                s.reg_handle, 0, static_cast<ULONG>(urb_bytes), first ? FALSE : TRUE,
                static_cast<ULONG>(iso_packets), s.descs.data(), &s.ov);
            first = false;
            s.pending = (ok || GetLastError() == ERROR_IO_PENDING);
        };

        for (auto& s : slots) submit(s);

        std::vector<uint8_t> packed;
        packed.reserve(urb_bytes);

        size_t idx = 0;
        while (streaming_.load()) {
            auto& s = slots[idx];
            idx = (idx + 1) % num_urbs;
            if (!s.pending) { submit(s); continue; }
            DWORD w = WaitForSingleObject(s.ov.hEvent, 200);
            if (w == WAIT_TIMEOUT) continue;
            if (w != WAIT_OBJECT_0) break;

            ULONG dummy = 0;
            BOOL ok = WinUsb_GetOverlappedResult(winusb_, &s.ov, &dummy, FALSE);
            s.pending = false;
            if (ok && streaming_.load()) {
                packed.clear();
                for (const auto& d : s.descs) {
                    if (d.Status == 0 && d.Length > 0 && d.Offset + d.Length <= s.buf.size()) {
                        packed.insert(packed.end(), s.buf.data() + d.Offset, s.buf.data() + d.Offset + d.Length);
                    }
                }
                if (!packed.empty()) {
                    cb(packed.data(), packed.size());
                }
            }
            if (streaming_.load()) submit(s);
        }

        WinUsb_AbortPipe(winusb_, ep);
        for (auto& s : slots) {
            if (s.pending) {
                ULONG dummy = 0;
                WinUsb_GetOverlappedResult(winusb_, &s.ov, &dummy, TRUE);
            }
            if (s.reg_handle) api.Unregister(s.reg_handle);
            if (s.ov.hEvent) CloseHandle(s.ov.hEvent);
        }
    });
    return true;
}

void WinUsbDevice::StopAsyncStream() {
    if (streaming_.exchange(false)) {
        if (winusb_ && stream_ep_ != 0) {
            WinUsb_AbortPipe(winusb_, stream_ep_);
        }
    }
    if (stream_thread_.joinable()) {
        stream_thread_.join();
    }
}

} // namespace friio

#endif // _WIN32
