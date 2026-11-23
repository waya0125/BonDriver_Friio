# SKNET MonsterTV U3S セットアップガイド

## 概要

- **USB ID**: `3275:9015`
- **対応放送波**: 地上デジタル（ISDB-T）・BS・110 度 CS
- **内蔵 B-CAS スロット**: **対応**（`EnableB25=1` で使用。外付け PC/SC カードリーダーを使う場合は `EnableB25=0`）
- **対応 Windows**: **Windows 8.1 / 10 / 11 専用**（x64 / x86）
- **状態**: **実機動作確認済み**（物理 Windows PC で 3 波選局・内蔵 B-CAS 復号を確認）

> [!IMPORTANT]
> - **Windows 7 非対応**: MonsterTV U3S は Isochronous 転送を使うため、Windows 8.1 以降で追加された WinUSB API（`WinUsb_ReadIsochPipeAsap`）が必要です。
> - **内蔵 B-CAS スロットの制約**: 実機確認済みの ATR・初期設定・ECM 応答形式に対応します（送信 APDU 上限 124 バイト / ECM 本体 118 バイト、ECM 失敗時のカード再接続は 1 回まで）。未対応のカードや長い ECM を使う放送では、`BonDriver_Friio.ini` で `EnableB25=0` に設定し、TVTest / EDCB 側の外付け PC/SC カードリーダーを使ってください。

## セットアップ手順

1. [Zadig](https://zadig.akeo.ie/) を開き、**`3275:9015`** に **WinUSB** ドライバをインストールします。
2. TVTest / EDCB のアーキテクチャに合わせて `BonDriver_Friio.dll`（x64 または x86）を配置します。
3. UT100B と併用する場合など機種を固定したいときは、`BonDriver_Friio.ini` に `Model=u3s` を指定します。
4. 内蔵 B-CAS スロットで復号する場合は `EnableB25=1`（既定）のまま使います（TVTest / EDCB 側のカードリーダー設定は「なし」にします）。外付け PC/SC カードリーダーを使う場合は `BonDriver_Friio.ini` に `EnableB25=0` を設定します。

## 実機動作確認（2026-11-23 時点）

検証環境: 物理 Windows PC（x64）+ WinUSB + MonsterTV U3S（`3275:9015`、内蔵 B-CAS スロット使用）+ MSVC ビルド `BonDriver_Friio.dll` / `bon_test.exe`

| 検証項目 | 結果 |
|---|---|
| `bon_test.exe`（UHF 21ch、10 秒、`EnableB25=1`） | `32.96 dB`、171,328 パケット、`sync_err=0` / `tei=0` / `cc_err=0` / `scrambled=0` |
| `bon_test.exe`（BS15_0、10 秒、`EnableB25=1`） | `13.81 dB`、143,453 パケット、`sync_err=0` / `tei=0` / `cc_err=0` / `scrambled=0` |
| **TVTest 0.10.0（64bit）** + `BonDriver_Friio.dll` | 選局・内蔵 B-CAS 復号 OK、`35.42 dB / 29.90 Mbps`、**`D 0 / E 0 / S 0`** |

> [!NOTE]
> QEMU のソフトウェア USB パススルー（`usb-host` + `qemu-xhci`）環境では、`EnableB25=0` でも Isochronous 転送が一時的に途切れて `cc_err` が発生します。実環境では物理 Windows PC の USB ポート、または PCIe USB コントローラーのパススルー環境を使ってください。
