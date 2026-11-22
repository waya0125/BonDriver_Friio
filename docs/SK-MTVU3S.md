# SKNET MonsterTV U3S セットアップガイド

## 概要

- **USB ID**: `3275:9015`
- **対応放送波**: 地上デジタル（ISDB-T）・BS・110 度 CS
- **内蔵 B-CAS スロット**: 非対応（外付け PC/SC カードリーダー前提）
- **対応 Windows**: **Windows 8.1 / 10 / 11 専用**（x64 / x86）
- **状態**: 実装済み（**Windows 実機未確認**）

> [!IMPORTANT]
> - **Windows 7 非対応**: MonsterTV U3S は Isochronous 転送を使用するため、Windows 8.1 以降で追加された WinUSB API（`WinUsb_ReadIsochPipeAsap`）が必要です。
> - **Windows 実機未確認**: Linux 版 `recfriio` で 3 波（地デジ・BS・CS 全 12 トランスポンダー）の動作を確認済みのロジックを移植していますが、Windows 実機でのテストは未実施です。

## セットアップ手順

1. [Zadig](https://zadig.akeo.ie/) を開き、**`3275:9015`** に **WinUSB** ドライバをインストールします。
2. TVTest / EDCB のアーキテクチャに合わせて `BonDriver_Friio.dll`（x64 または x86）を配置します。
3. UT100B と併用する場合など機種を固定したいときは、`BonDriver_Friio.ini` に `Model=u3s` を指定します。
4. B-CAS 復号は TVTest / EDCB 側で外付け PC/SC カードリーダーを設定して使用します。
