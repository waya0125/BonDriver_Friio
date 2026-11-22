// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#include "IBonDriver2.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <unordered_map>

using CreateBonDriverFn = IBonDriver* (*)();

int main(int argc, char** argv) {
    const char* dll_path = (argc > 1) ? argv[1] : "BonDriver_Friio.dll";
    const DWORD space = (argc > 2) ? static_cast<DWORD>(std::atoi(argv[2])) : 0;   // 0=UHF
    const DWORD ch_idx = (argc > 3) ? static_cast<DWORD>(std::atoi(argv[3])) : 8;  // 8 => 21ch
    const int seconds = (argc > 4) ? std::atoi(argv[4]) : 5;

    HMODULE mod = LoadLibraryA(dll_path);
    if (!mod) {
        std::printf("LoadLibraryA(%s) failed: err=%lu\n", dll_path, GetLastError());
        return 1;
    }
    auto create_fn = reinterpret_cast<CreateBonDriverFn>(GetProcAddress(mod, "CreateBonDriver"));
    if (!create_fn) {
        std::printf("GetProcAddress(CreateBonDriver) failed\n");
        FreeLibrary(mod);
        return 1;
    }

    auto* bon = dynamic_cast<IBonDriver2*>(create_fn());
    if (!bon) {
        std::printf("CreateBonDriver returned null or non-IBonDriver2\n");
        FreeLibrary(mod);
        return 1;
    }

    std::wprintf(L"Tuner: %ls\n", bon->GetTunerName());
    if (!bon->OpenTuner()) {
        std::printf("OpenTuner() failed\n");
        bon->Release();
        FreeLibrary(mod);
        return 1;
    }
    std::printf("OpenTuner() OK\n");

    const wchar_t* sp_name = bon->EnumTuningSpace(space);
    const wchar_t* ch_name = bon->EnumChannelName(space, ch_idx);
    std::wprintf(L"SetChannel(space=%lu [%ls], ch=%lu [%ls])...\n",
                 space, sp_name ? sp_name : L"?", ch_idx, ch_name ? ch_name : L"?");
    if (!bon->SetChannel(space, ch_idx)) {
        std::printf("SetChannel() failed\n");
        bon->CloseTuner();
        bon->Release();
        FreeLibrary(mod);
        return 1;
    }
    std::printf("SetChannel() OK, SignalLevel=%.2f\n", bon->GetSignalLevel());

    uint64_t total_pkts = 0, sync_err = 0, tei = 0, cc_err = 0;
    std::unordered_map<uint16_t, uint8_t> last_cc;
    const auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    while (std::chrono::steady_clock::now() < end_time) {
        if (bon->WaitTsStream(200) != WAIT_OBJECT_0) continue;
        BYTE* buf = nullptr;
        DWORD size = 0, remain = 0;
        while (bon->GetTsStream(&buf, &size, &remain) && size > 0) {
            for (DWORD p = 0; p + 188 <= size; p += 188) {
                const uint8_t* pkt = buf + p;
                ++total_pkts;
                if (pkt[0] != 0x47) { ++sync_err; continue; }
                if (pkt[1] & 0x80) { ++tei; continue; }
                const uint16_t pid = uint16_t((pkt[1] & 0x1f) << 8) | uint16_t(pkt[2]);
                if (pid == 0x1fff) continue;
                const uint8_t afc = (pkt[3] >> 4) & 0x03;
                if (afc & 0x01) {
                    const uint8_t cc = pkt[3] & 0x0f;
                    const bool disc = (afc & 0x02) && (pkt[4] > 0) && (pkt[5] & 0x80);
                    auto it = last_cc.find(pid);
                    if (it != last_cc.end() && !disc && cc != ((it->second + 1) & 0x0f) && cc != it->second) {
                        ++cc_err;
                    }
                    last_cc[pid] = cc;
                }
            }
            if (remain == 0) break;
        }
    }

    std::printf("Done (%ds): SignalLevel=%.2f packets=%llu sync_err=%llu tei=%llu cc_err=%llu\n",
                seconds, bon->GetSignalLevel(),
                static_cast<unsigned long long>(total_pkts),
                static_cast<unsigned long long>(sync_err),
                static_cast<unsigned long long>(tei),
                static_cast<unsigned long long>(cc_err));

    bon->CloseTuner();
    bon->Release();
    FreeLibrary(mod);
    return 0;
}
