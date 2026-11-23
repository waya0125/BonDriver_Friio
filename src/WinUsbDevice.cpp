// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#ifdef _WIN32

#include "WinUsbDevice.hpp"

#include <objbase.h>
#include <setupapi.h>
#include <usbiodef.h>

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <set>

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
    std::set<std::wstring> unique_paths;
    const std::wstring needle = FormatVidPid(vid, pid);

    auto collect_by_guid = [&](const GUID& guid) {
        HDEVINFO dev_info = SetupDiGetClassDevsW(
            &guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (dev_info == INVALID_HANDLE_VALUE) return;

        SP_DEVICE_INTERFACE_DATA if_data{};
        if_data.cbSize = sizeof(if_data);
        for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(dev_info, nullptr, &guid, idx, &if_data); ++idx) {
            DWORD req_size = 0;
            SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, nullptr, 0, &req_size, nullptr);
            if (req_size == 0) continue;

            std::vector<uint8_t> detail_buf(req_size);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detail_buf.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, detail, req_size, nullptr, nullptr)) {
                std::wstring path(detail->DevicePath);
                if (ToLower(path).find(needle) != std::wstring::npos) {
                    unique_paths.insert(std::move(path));
                }
            }
        }
        SetupDiDestroyDeviceInfoList(dev_info);
    };

    // 1. 単一インターフェース機器向け
    collect_by_guid(GUID_DEVINTERFACE_USB_DEVICE);

    // 2. 複合 USB 機器（MI_00 等、Zadig が DeviceInterfaceGUIDs を登録するケース）向け
    HDEVINFO usb_devs = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (usb_devs != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA dev_data{};
        dev_data.cbSize = sizeof(dev_data);
        for (DWORD idx = 0; SetupDiEnumDeviceInfo(usb_devs, idx, &dev_data); ++idx) {
            wchar_t hw_id[512]{};
            if (!SetupDiGetDeviceRegistryPropertyW(usb_devs, &dev_data, SPDRP_HARDWAREID, nullptr,
                                                   reinterpret_cast<PBYTE>(hw_id), sizeof(hw_id), nullptr)) {
                continue;
            }
            if (ToLower(hw_id).find(needle) == std::wstring::npos) continue;

            HKEY hkey = SetupDiOpenDevRegKey(usb_devs, &dev_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
            if (hkey == INVALID_HANDLE_VALUE) continue;

            wchar_t guid_buf[256]{};
            DWORD buf_size = sizeof(guid_buf);
            DWORD type = 0;
            LONG rc = RegQueryValueExW(hkey, L"DeviceInterfaceGUIDs", nullptr, &type,
                                       reinterpret_cast<LPBYTE>(guid_buf), &buf_size);
            if (rc != ERROR_SUCCESS) {
                buf_size = sizeof(guid_buf);
                rc = RegQueryValueExW(hkey, L"DeviceInterfaceGUID", nullptr, &type,
                                      reinterpret_cast<LPBYTE>(guid_buf), &buf_size);
            }
            RegCloseKey(hkey);

            if (rc == ERROR_SUCCESS) {
                GUID custom_guid{};
                if (SUCCEEDED(CLSIDFromString(guid_buf, &custom_guid))) {
                    collect_by_guid(custom_guid);
                }
            }
        }
        SetupDiDestroyDeviceInfoList(usb_devs);
    }

    return std::vector<std::wstring>(unique_paths.begin(), unique_paths.end());
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
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        struct Slot {
            size_t offset = 0;
            std::vector<IsochPacketDesc> descs;
            OVERLAPPED ov{};
            bool pending = false;
        };

        std::vector<uint8_t> ring_buf(num_urbs * urb_bytes);
        PVOID reg_handle = nullptr;
        if (!api.Register(winusb_, ep, ring_buf.data(), static_cast<ULONG>(ring_buf.size()), &reg_handle) || !reg_handle) {
            std::fprintf(stderr, "[isoch] RegisterIsochBuffer failed: err=%lu\n", GetLastError());
            return;
        }

        std::vector<Slot> slots(num_urbs);
        for (size_t i = 0; i < num_urbs; ++i) {
            slots[i].offset = i * urb_bytes;
            slots[i].descs.resize(iso_packets);
            slots[i].ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }

        uint64_t sub_ok = 0, sub_fail = 0, ov_ok = 0, ov_fail = 0;
        uint64_t pkt_nz = 0, pkt_zero = 0, pkt_err = 0, waits_to = 0;
        DWORD last_sub_err = 0, last_ov_err = 0;
        ULONG last_pkt_status = 0;
        bool resync = true;

        auto submit = [&](Slot& s) {
            ResetEvent(s.ov.hEvent);
            const BOOL cont = resync ? FALSE : TRUE;
            BOOL ok = api.ReadAsap(
                reg_handle, static_cast<ULONG>(s.offset), static_cast<ULONG>(urb_bytes), cont,
                static_cast<ULONG>(iso_packets), s.descs.data(), &s.ov);
            DWORD err = ok ? 0 : GetLastError();
            s.pending = (ok || err == ERROR_IO_PENDING);
            if (s.pending) {
                ++sub_ok;
                resync = false;
            } else {
                ++sub_fail;
                last_sub_err = err;
                resync = true;
            }
        };

        for (auto& s : slots) submit(s);

        std::vector<uint8_t> packed;
        packed.reserve(urb_bytes);

        size_t idx = 0;
        while (streaming_.load()) {
            auto& s = slots[idx];
            idx = (idx + 1) % num_urbs;
            if (!s.pending) {
                submit(s);
                continue;
            }
            DWORD w = WaitForSingleObject(s.ov.hEvent, 200);
            if (w == WAIT_TIMEOUT) {
                ++waits_to;
                continue;
            }
            if (w != WAIT_OBJECT_0) break;

            ULONG dummy = 0;
            BOOL ok = WinUsb_GetOverlappedResult(winusb_, &s.ov, &dummy, FALSE);
            if (!ok) last_ov_err = GetLastError();
            s.pending = false;
            packed.clear();
            if (ok && streaming_.load()) {
                ++ov_ok;
                const uint8_t* base = ring_buf.data() + s.offset;
                for (const auto& d : s.descs) {
                    if (d.Status != 0) {
                        ++pkt_err;
                        last_pkt_status = d.Status;
                    } else if (d.Length > 0 && d.Offset + d.Length <= urb_bytes) {
                        ++pkt_nz;
                        packed.insert(packed.end(), base + d.Offset, base + d.Offset + d.Length);
                    } else {
                        ++pkt_zero;
                    }
                }
            } else if (!ok) {
                ++ov_fail;
                resync = true;
            }
            if (streaming_.load()) submit(s);
            if (!packed.empty() && streaming_.load()) {
                cb(packed.data(), packed.size());
            }
        }

        WinUsb_AbortPipe(winusb_, ep);
        for (auto& s : slots) {
            if (s.pending) {
                ULONG dummy = 0;
                WinUsb_GetOverlappedResult(winusb_, &s.ov, &dummy, TRUE);
            }
            if (s.ov.hEvent) CloseHandle(s.ov.hEvent);
        }
        api.Unregister(reg_handle);
        std::fprintf(stderr,
            "[isoch] sub_ok=%llu sub_fail=%llu(err=%lu) ov_ok=%llu ov_fail=%llu(err=%lu) "
            "to=%llu pkt_nz=%llu pkt_zero=%llu pkt_err=%llu(st=0x%lx)\n",
            (unsigned long long)sub_ok, (unsigned long long)sub_fail, last_sub_err,
            (unsigned long long)ov_ok, (unsigned long long)ov_fail, last_ov_err,
            (unsigned long long)waits_to, (unsigned long long)pkt_nz,
            (unsigned long long)pkt_zero, (unsigned long long)pkt_err, last_pkt_status);
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
