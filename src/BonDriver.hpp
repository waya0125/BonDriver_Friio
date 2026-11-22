// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#ifdef _WIN32

#include "IBonDriver2.h"
#include "AsieStreamCipher.hpp"
#include "B25Decoder.hpp"
#include "Mtvu3sDevice.hpp"
#include "Settler.hpp"
#include "Ut100bDevice.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace friio {

enum class Model { Auto, Ut100b, Mtvu3s };

struct Config {
    Model model = Model::Auto;
    int device_index = 0;
    bool enable_b25 = true;
    bool no_settle = false;
};

class BonDriverFriio : public IBonDriver2 {
public:
    explicit BonDriverFriio(HMODULE module);
    ~BonDriverFriio();

    const BOOL OpenTuner(void) override;
    void CloseTuner(void) override;

    const BOOL SetChannel(const BYTE bCh) override;
    const float GetSignalLevel(void) override;

    const DWORD WaitTsStream(const DWORD dwTimeOut = 0) override;
    const DWORD GetReadyCount(void) override;

    const BOOL GetTsStream(BYTE* pDst, DWORD* pdwSize, DWORD* pdwRemain) override;
    const BOOL GetTsStream(BYTE** ppDst, DWORD* pdwSize, DWORD* pdwRemain) override;

    void PurgeTsStream(void) override;

    LPCTSTR GetTunerName(void) override;
    const BOOL IsTunerOpening(void) override;

    LPCTSTR EnumTuningSpace(const DWORD dwSpace) override;
    LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel) override;

    const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel) override;

    const DWORD GetCurSpace(void) override;
    const DWORD GetCurChannel(void) override;

    void Release(void) override;

private:
    void LoadIni(HMODULE module);
    void OnRawChunk(const uint8_t* data, size_t len);
    std::vector<uint8_t> AlignAndDecrypt(const uint8_t* data, size_t len);

    Config cfg_;
    Model active_model_ = Model::Auto;
    Ut100bDevice ut100b_;
    Mtvu3sDevice u3s_;
    bool is_open_ = false;

    DWORD cur_space_ = 0;
    DWORD cur_channel_ = 0;
    std::wstring tuner_name_ = L"BonDriver_Friio";
    std::wstring channel_name_buf_;

    // ストリーム処理（ストリームスレッド専用）
    size_t stale_drop_ = 0;
    std::vector<uint8_t> rest_;
    dyna::Settler settler_{5000, 200000};
    dyna::B25Decoder b25_;
    bool b25_active_ = false;

    // 出力キュー
    static constexpr size_t kMaxQueueChunks = 512;
    std::mutex queue_mu_;
    std::condition_variable queue_cv_;
    std::deque<std::vector<uint8_t>> queue_;
    std::vector<uint8_t> last_out_;
};

} // namespace friio

#endif // _WIN32
