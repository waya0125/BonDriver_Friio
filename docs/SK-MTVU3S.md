# SKNET MonsterTV U3S セットアップガイド

## 概要

- **USB ID**: `3275:9015`
- **対応放送波**: 地上デジタル（ISDB-T）・BS・110 度 CS
- **内蔵 B-CAS スロット**: **対応**（`EnableB25=1` で使用。外付け PC/SC カードリーダーを使う場合は `EnableB25=0`）
- **対応 Windows**: **Windows 8.1 / 10 / 11 専用**（x64 / x86）
- **状態**: 実装済み（**Windows 実機未確認**）

> [!IMPORTANT]
> - **Windows 7 非対応**: MonsterTV U3S は Isochronous 転送を使用するため、Windows 8.1 以降で追加された WinUSB API（`WinUsb_ReadIsochPipeAsap`）が必要です。
> - **Windows 実機未確認**: Linux 版 `recfriio` のロジック（3 波選局および内蔵 B-CAS スロット処理）を移植していますが、Windows 実機でのテストは未実施です。
> - **内蔵 B-CAS スロットの制約**: 実機確認済みの ATR・初期設定・ECM 応答形式に対応します（送信 APDU 上限 124 バイト / ECM 本体 118 バイト、ECM 失敗時のカード再接続は 1 回まで）。未対応のカードや長い ECM を使う放送では、`BonDriver_Friio.ini` で `EnableB25=0` に設定し、TVTest / EDCB 側の外付け PC/SC カードリーダーを使用してください。

## セットアップ手順

1. [Zadig](https://zadig.akeo.ie/) を開き、**`3275:9015`** に **WinUSB** ドライバをインストールします。
2. TVTest / EDCB のアーキテクチャに合わせて `BonDriver_Friio.dll`（x64 または x86）を配置します。
3. UT100B と併用する場合など機種を固定したいときは、`BonDriver_Friio.ini` に `Model=u3s` を指定します。
4. 内蔵 B-CAS スロットで復号する場合は `EnableB25=1`（既定）のまま使用します（TVTest / EDCB 側の B-CAS カードリーダー設定は「なし」にしてください）。外付け PC/SC カードリーダーを使う場合は `BonDriver_Friio.ini` に `EnableB25=0` を設定します。
