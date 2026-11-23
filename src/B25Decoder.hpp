// SPDX-License-Identifier: MIT
// Ported from arib25 / recdyna (internal/bcas, internal/b25).
// Copyright (c) 2026 waya0125
#pragma once

#include "Multi2.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dyna {

// B-CAS カードへの APDU 送受信インターフェース
struct BcasTransport {
    std::function<bool()> reset;
    std::function<bool(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp)> transmit;
    // 送信可能な APDU 最大長（0 のときは制限なし）。超過時は再リセットせず即失敗する。
    size_t max_apdu_len = 0;
};

// MPEG-2 TS セクション（PAT / PMT / ECM）の組み立て器
class SectionAssembler {
public:
    void Reset();
    // パケットを投入し、完成したセクション（ヘッダ〜CRC含む）があれば返す
    std::vector<std::vector<uint8_t>> PushPacket(const uint8_t pkt[188]);

private:
    std::vector<uint8_t> buf_;
    size_t expected_ = 0;
    bool active_ = false;
};

// ARIB STD-B25 デコーダ（内蔵 B-CAS スロット用）
class B25Decoder {
public:
    B25Decoder() = default;

    // カードを初期化（Reset + INT コマンド）してシステム鍵と CBC 初期値を取得する
    bool Init(BcasTransport transport, int multi2_round = 4, int ecm_retries = 3);
    bool IsReady() const { return ready_; }

    // チャンネル切替時に PAT / PMT / ECM 状態をクリアする（カード鍵は保持）
    void ResetStream();

    // 188 バイト単位の TS パケット列をその場で復号する
    void DecodePackets(uint8_t* packets, size_t len);

private:
    bool ConnectCard();
    bool ProcessECM(const uint8_t* ecm, size_t len, uint8_t ks_out[16], uint16_t& return_code);
    void HandlePAT(const std::vector<uint8_t>& sec);
    void HandlePMT(uint16_t pmt_pid, const std::vector<uint8_t>& sec);
    void HandleECM(uint16_t ecm_pid, const std::vector<uint8_t>& sec);

    BcasTransport transport_;
    int round_ = 4;
    int ecm_retries_ = 3;
    bool ready_ = false;

    std::array<uint8_t, 32> system_key_{};
    std::array<uint8_t, 8> init_cbc_{};
    uint16_t ca_system_id_ = 0;

    SectionAssembler pat_asm_;
    std::unordered_set<uint16_t> pmt_pids_;
    std::unordered_map<uint16_t, SectionAssembler> pmt_asms_;
    std::unordered_map<uint16_t, SectionAssembler> ecm_asms_;

    struct EcmState {
        std::vector<uint8_t> last_ecm;
        Multi2Cipher cipher;
    };
    std::unordered_map<uint16_t, EcmState> ecm_states_;
    // ES PID -> ECM PID
    std::unordered_map<uint16_t, uint16_t> es_to_ecm_;
};

} // namespace dyna
