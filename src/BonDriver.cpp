// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#ifdef _WIN32

#include "BonDriver.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cwctype>

namespace friio {

namespace {

constexpr uint16_t kCsTsids[12] = {
    0x6020, 0x7040, 0x7060, 0x6080, 0x60a0, 0x70c0,
    0x70e0, 0x7100, 0x7120, 0x7140, 0x7160, 0x7180,
};

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

} // namespace

BonDriverFriio::BonDriverFriio(HMODULE module) {
    LoadIni(module);
}

BonDriverFriio::~BonDriverFriio() {
    CloseTuner();
}

void BonDriverFriio::LoadIni(HMODULE module) {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return;
    std::wstring ini(path);
    const size_t dot = ini.rfind(L'.');
    if (dot != std::wstring::npos) ini.replace(dot, std::wstring::npos, L".ini");
    else ini += L".ini";

    wchar_t model_buf[64]{};
    GetPrivateProfileStringW(L"FRIIO", L"Model", L"auto", model_buf, 64, ini.c_str());
    std::wstring m = ToLower(model_buf);
    if (m == L"ut100b" || m == L"hdp") cfg_.model = Model::Ut100b;
    else if (m == L"u3s" || m == L"mtvu3s") cfg_.model = Model::Mtvu3s;
    else cfg_.model = Model::Auto;

    cfg_.device_index = GetPrivateProfileIntW(L"FRIIO", L"DeviceIndex", 0, ini.c_str());
    cfg_.enable_b25 = GetPrivateProfileIntW(L"FRIIO", L"EnableB25", 1, ini.c_str()) != 0;
    cfg_.no_settle = GetPrivateProfileIntW(L"FRIIO", L"NoSettle", 0, ini.c_str()) != 0;
    cfg_.firmware_index = static_cast<uint16_t>(GetPrivateProfileIntW(L"FRIIO", L"FirmwareIndex", 0x5399, ini.c_str()));

    std::wstring default_fw(path);
    const size_t slash = default_fw.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        default_fw = default_fw.substr(0, slash + 1) + L"asv5211-firmware.bin";
    } else {
        default_fw = L"asv5211-firmware.bin";
    }
    wchar_t fw_buf[MAX_PATH]{};
    GetPrivateProfileStringW(L"FRIIO", L"FirmwarePath", default_fw.c_str(), fw_buf, MAX_PATH, ini.c_str());
    cfg_.firmware_path = fw_buf;
}

const BOOL BonDriverFriio::OpenTuner(void) {
    if (is_open_) return TRUE;

    bool try_ut = (cfg_.model == Model::Auto || cfg_.model == Model::Ut100b);
    bool try_u3s = (cfg_.model == Model::Auto || cfg_.model == Model::Mtvu3s);

    if (try_ut) {
        bool opened = ut100b_.Open(cfg_.device_index);
        if (!opened && Ut100bDevice::LoadFirmwareIfNeeded(cfg_.firmware_path, cfg_.firmware_index)) {
            opened = ut100b_.Open(cfg_.device_index);
        }
        if (opened) {
            if (ut100b_.Init()) {
                active_model_ = Model::Ut100b;
                tuner_name_ = L"BonDriver_Friio (QRS-UT100B)";
                b25_active_ = false;
                if (cfg_.enable_b25) {
                    dyna::BcasTransport tr;
                    tr.reset = [this]() { return ut100b_.ResetBcas(); };
                    tr.transmit = [this](const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp) {
                        return ut100b_.TransmitBcas(apdu, len, resp);
                    };
                    b25_active_ = b25_.Init(std::move(tr), 4);
                }
                if (b25_active_) StartDecodeWorker();
                is_open_ = true;
                return TRUE;
            }
            ut100b_.Close();
        }
    }

    if (try_u3s && u3s_.Open(cfg_.device_index)) {
        if (u3s_.Init()) {
            active_model_ = Model::Mtvu3s;
            tuner_name_ = L"BonDriver_Friio (MonsterTV U3S)";
            b25_active_ = false;
            if (cfg_.enable_b25) {
                dyna::BcasTransport tr;
                tr.reset = [this]() { return u3s_.ResetBcas(); };
                tr.transmit = [this](const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp) {
                    return u3s_.TransmitBcas(apdu, len, resp);
                };
                tr.max_apdu_len = kMtvu3sCardMaxApdu;
                // カード待機中はストリーム処理が止まるため、ECM 失敗時の再接続は 1 回までにする
                b25_active_ = b25_.Init(std::move(tr), 4, 1);
            }
            if (b25_active_) StartDecodeWorker();
            is_open_ = true;
            return TRUE;
        }
        u3s_.Close();
    }

    return FALSE;
}

void BonDriverFriio::CloseTuner(void) {
    if (!is_open_) return;
    if (active_model_ == Model::Ut100b) ut100b_.StopStream();
    else if (active_model_ == Model::Mtvu3s) u3s_.StopStream();
    StopDecodeWorker();
    if (active_model_ == Model::Ut100b) ut100b_.Close();
    else if (active_model_ == Model::Mtvu3s) u3s_.Close();
    PurgeTsStream();
    b25_active_ = false;
    is_open_ = false;
}

const BOOL BonDriverFriio::SetChannel(const BYTE bCh) {
    return SetChannel(0, bCh);
}

const BOOL BonDriverFriio::SetChannel(const DWORD dwSpace, const DWORD dwChannel) {
    if (!is_open_) return FALSE;

    if (active_model_ == Model::Ut100b) {
        int freq_mhz = 0;
        if (dwSpace == 0 && dwChannel < 50) {
            if (!Ut100bDevice::ChannelToFreqMHz(static_cast<int>(13 + dwChannel), false, freq_mhz)) return FALSE;
        } else if (dwSpace == 1 && dwChannel < 51) {
            if (!Ut100bDevice::ChannelToFreqMHz(static_cast<int>(13 + dwChannel), true, freq_mhz)) return FALSE;
        } else {
            return FALSE;
        }

        ut100b_.StopStream();
        ResetDecodePipeline();
        PurgeTsStream();
        if (!ut100b_.Tune(freq_mhz)) return FALSE;

        stale_drop_ = 2 * 16384;
        rest_.clear();
        settler_.Reset();

        if (!ut100b_.StartStream([this](const uint8_t* d, size_t n) { OnRawChunk(d, n); })) return FALSE;
        cur_space_ = dwSpace;
        cur_channel_ = dwChannel;
        return TRUE;
    }

    if (active_model_ == Model::Mtvu3s) {
        u3s_.StopStream();
        ResetDecodePipeline();
        PurgeTsStream();

        bool ok = false;
        if (dwSpace == 0 && dwChannel < 50) {
            const uint32_t freq_khz = 473143u + 6000u * dwChannel;
            ok = u3s_.TuneUHF(freq_khz);
        } else if (dwSpace == 1 && dwChannel < 12 * 4) {
            const uint32_t tp = dwChannel / 4; // 0..11 => BS01..BS23
            const uint32_t bs_num = tp * 2 + 1;
            uint32_t slot = dwChannel % 4;
            if (bs_num == 15) slot += 1;
            const uint32_t freq_khz = 1049480u + tp * 38360u;
            const uint16_t tsid = static_cast<uint16_t>(0x4000u | (bs_num << 4) | slot);
            ok = u3s_.TuneSat(freq_khz, tsid);
        } else if (dwSpace == 2 && dwChannel < 12) {
            const uint32_t freq_khz = 1613000u + dwChannel * 40000u;
            const uint16_t tsid = kCsTsids[dwChannel];
            ok = u3s_.TuneSat(freq_khz, tsid);
        } else {
            return FALSE;
        }
        if (!ok) return FALSE;

        stale_drop_ = 0;
        rest_.clear();
        settler_.Reset();

        if (!u3s_.StartStream([this](const uint8_t* d, size_t n) { OnRawChunk(d, n); })) return FALSE;
        cur_space_ = dwSpace;
        cur_channel_ = dwChannel;
        return TRUE;
    }

    return FALSE;
}

std::vector<uint8_t> BonDriverFriio::AlignAndDecrypt(const uint8_t* data, size_t len) {
    if (stale_drop_ > 0) {
        const size_t drop = std::min(len, stale_drop_);
        stale_drop_ -= drop;
        data += drop;
        len -= drop;
        if (len == 0) return {};
    }

    std::vector<uint8_t> buf;
    buf.reserve(rest_.size() + len);
    buf.insert(buf.end(), rest_.begin(), rest_.end());
    buf.insert(buf.end(), data, data + len);

    const size_t size = buf.size();
    std::vector<uint8_t> out;
    out.reserve((size / 188) * 188);

    size_t pos = 0;
    while (size - pos >= 188) {
        if (buf[pos] != 0x47) {
            if (size - pos <= 188 * 2) break;
            bool found = false;
            while (pos + 188 * 2 < size) {
                if (buf[pos] == 0x47 && buf[pos + 188] == 0x47 && buf[pos + 376] == 0x47) {
                    found = true;
                    break;
                }
                ++pos;
            }
            if (!found) break;
        }
        const size_t start = out.size();
        out.insert(out.end(), buf.begin() + static_cast<ptrdiff_t>(pos),
                   buf.begin() + static_cast<ptrdiff_t>(pos + 188));
        if (active_model_ == Model::Ut100b) {
            asieDecryptPacket(out.data() + start, out.data() + start);
        } else if (active_model_ == Model::Mtvu3s) {
            u3s_.DescramblePacket(out.data() + start);
        }
        pos += 188;
    }

    rest_.assign(buf.begin() + static_cast<ptrdiff_t>(pos), buf.end());
    return out;
}

void BonDriverFriio::StartDecodeWorker() {
    StopDecodeWorker();
    {
        std::lock_guard<std::mutex> lk(raw_mu_);
        raw_queue_.clear();
        decode_stop_ = false;
        decode_busy_ = false;
    }
    decode_thread_ = std::thread([this]() {
        while (true) {
            std::vector<uint8_t> packets;
            {
                std::unique_lock<std::mutex> lk(raw_mu_);
                raw_cv_.wait(lk, [this]() { return decode_stop_ || !raw_queue_.empty(); });
                if (decode_stop_) break;
                packets = std::move(raw_queue_.front());
                raw_queue_.pop_front();
                decode_busy_ = true;
            }

            if (!packets.empty()) {
                b25_.DecodePackets(packets.data(), packets.size());
                PushOutput(std::move(packets));
            }

            {
                std::lock_guard<std::mutex> lk(raw_mu_);
                decode_busy_ = false;
                if (raw_queue_.empty()) decode_idle_cv_.notify_all();
            }
        }
    });
}

void BonDriverFriio::StopDecodeWorker() {
    {
        std::lock_guard<std::mutex> lk(raw_mu_);
        decode_stop_ = true;
        raw_queue_.clear();
    }
    raw_cv_.notify_all();
    if (decode_thread_.joinable()) {
        decode_thread_.join();
    }
    std::lock_guard<std::mutex> lk(raw_mu_);
    decode_stop_ = false;
    decode_busy_ = false;
}

void BonDriverFriio::ResetDecodePipeline() {
    if (!b25_active_) return;
    std::unique_lock<std::mutex> lk(raw_mu_);
    raw_queue_.clear();
    decode_idle_cv_.wait(lk, [this]() { return !decode_busy_; });
    b25_.ResetStream();
}

void BonDriverFriio::PushOutput(std::vector<uint8_t>&& packets) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    while (queue_.size() >= kMaxQueueChunks) queue_.pop_front();
    queue_.push_back(std::move(packets));
    queue_cv_.notify_one();
}

void BonDriverFriio::OnRawChunk(const uint8_t* data, size_t len) {
    auto packets = AlignAndDecrypt(data, len);
    if (!cfg_.no_settle && !packets.empty()) {
        packets = settler_.Feed(packets.data(), packets.size());
    }
    if (packets.empty()) return;

    if (b25_active_) {
        std::lock_guard<std::mutex> lk(raw_mu_);
        while (raw_queue_.size() >= kMaxRawQueueChunks) raw_queue_.pop_front();
        raw_queue_.push_back(std::move(packets));
        raw_cv_.notify_one();
        return;
    }

    PushOutput(std::move(packets));
}

const float BonDriverFriio::GetSignalLevel(void) {
    if (!is_open_) return 0.0f;
    float cnr = 0.0f;
    if (active_model_ == Model::Ut100b) ut100b_.GetCNR(cnr);
    else if (active_model_ == Model::Mtvu3s) u3s_.GetCNR(cnr);
    return cnr;
}

const DWORD BonDriverFriio::WaitTsStream(const DWORD dwTimeOut) {
    std::unique_lock<std::mutex> lk(queue_mu_);
    if (!queue_.empty()) return WAIT_OBJECT_0;
    if (dwTimeOut == 0) return WAIT_TIMEOUT;
    if (dwTimeOut == INFINITE) {
        queue_cv_.wait(lk, [this]() { return !queue_.empty() || !is_open_; });
        return !queue_.empty() ? WAIT_OBJECT_0 : WAIT_ABANDONED;
    }
    const bool ready = queue_cv_.wait_for(lk, std::chrono::milliseconds(dwTimeOut), [this]() {
        return !queue_.empty() || !is_open_;
    });
    return (ready && !queue_.empty()) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}

const DWORD BonDriverFriio::GetReadyCount(void) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    return static_cast<DWORD>(queue_.size());
}

const BOOL BonDriverFriio::GetTsStream(BYTE* pDst, DWORD* pdwSize, DWORD* pdwRemain) {
    BYTE* src = nullptr;
    if (!GetTsStream(&src, pdwSize, pdwRemain)) return FALSE;
    if (*pdwSize > 0 && pDst != nullptr && src != nullptr) {
        std::memcpy(pDst, src, *pdwSize);
    }
    return TRUE;
}

const BOOL BonDriverFriio::GetTsStream(BYTE** ppDst, DWORD* pdwSize, DWORD* pdwRemain) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    if (queue_.empty()) {
        if (pdwSize) *pdwSize = 0;
        if (pdwRemain) *pdwRemain = 0;
        return FALSE;
    }
    last_out_ = std::move(queue_.front());
    queue_.pop_front();
    if (ppDst) *ppDst = last_out_.data();
    if (pdwSize) *pdwSize = static_cast<DWORD>(last_out_.size());
    if (pdwRemain) *pdwRemain = static_cast<DWORD>(queue_.size());
    return TRUE;
}

void BonDriverFriio::PurgeTsStream(void) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    queue_.clear();
}

LPCTSTR BonDriverFriio::GetTunerName(void) {
    return tuner_name_.c_str();
}

const BOOL BonDriverFriio::IsTunerOpening(void) {
    return is_open_ ? TRUE : FALSE;
}

LPCTSTR BonDriverFriio::EnumTuningSpace(const DWORD dwSpace) {
    if (active_model_ == Model::Mtvu3s) {
        if (dwSpace == 0) return L"UHF";
        if (dwSpace == 1) return L"BS";
        if (dwSpace == 2) return L"CS";
        return nullptr;
    }
    if (dwSpace == 0) return L"UHF";
    if (dwSpace == 1) return L"CATV";
    return nullptr;
}

LPCTSTR BonDriverFriio::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel) {
    wchar_t buf[32]{};
    if (active_model_ == Model::Mtvu3s) {
        if (dwSpace == 0 && dwChannel < 50) {
            swprintf(buf, 32, L"%luch", 13 + dwChannel);
        } else if (dwSpace == 1 && dwChannel < 48) {
            swprintf(buf, 32, L"BS%02lu_%lu", (dwChannel / 4) * 2 + 1, dwChannel % 4);
        } else if (dwSpace == 2 && dwChannel < 12) {
            swprintf(buf, 32, L"CS%lu", (dwChannel + 1) * 2);
        } else {
            return nullptr;
        }
    } else {
        if (dwSpace == 0 && dwChannel < 50) {
            swprintf(buf, 32, L"%luch", 13 + dwChannel);
        } else if (dwSpace == 1 && dwChannel < 51) {
            swprintf(buf, 32, L"C%luch", 13 + dwChannel);
        } else {
            return nullptr;
        }
    }
    channel_name_buf_ = buf;
    return channel_name_buf_.c_str();
}

const DWORD BonDriverFriio::GetCurSpace(void) { return cur_space_; }
const DWORD BonDriverFriio::GetCurChannel(void) { return cur_channel_; }
void BonDriverFriio::Release(void) { delete this; }

} // namespace friio

#endif // _WIN32
