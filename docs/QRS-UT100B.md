# Quixun QRS-UT100B セットアップガイド & 検証結果

## 概要

- **USB ID**: `1738:5211`（ファームウェア未転送）→ `3275:7020`（ファームウェア転送後）
- **対応放送波**: 地上デジタル（ISDB-T / CATV）
- **内蔵 B-CAS スロット**: 対応（`EnableB25=1` 既定）
- **対応 Windows**: Windows 7 / 8 / 10 / 11（x64 / x86）

## 初回セットアップ手順

QRS-UT100B は USB 接続直後はファームウェア未転送状態（**`1738:5211`**）として認識され、ファームを転送するとチューナー本体（**`3275:7020`**）として再認識されます。  
`BonDriver_Friio` は `OpenTuner()` 時に自動ファームウェア転送を行うため、古い未署名ローダードライバ（`Quixun_AS11Loader.sys`）不要で Windows 11（HVCI 有効）でも動作します。

1. **ファームウェアの準備**
   - 製品付属 CD の `Quixun_AS11Loader.sys` から `recfriio` の `ut100b-firmware extract` で `asv5211-firmware.bin`（16 KB）を取り出し、`BonDriver_Friio.dll` と同じフォルダに配置します。
   - ※forNetbook 版ファームの場合は `BonDriver_Friio.ini` に `FirmwareIndex=21476`（`0x53e4`）を指定してください（通常版は既定の `21401` = `0x5399`）。
2. **Zadig（1 回目: `1738:5211`）**
   - UT100B を挿した直後に [Zadig](https://zadig.akeo.ie/) を開き、`Options → List All Devices` にチェックを入れて **`1738:5211`** に **WinUSB** をインストールします。
3. **TVTest を一度起動**
   - `BonDriver_Friio.dll` を開くと自動でファームが転送され、UT100B が **`3275:7020`** として再認識されます（初回のみ `3275:7020` にまだ WinUSB が無いため一度失敗します）。
4. **Zadig（2 回目: `3275:7020` の `MI_00`）**
   - 再び Zadig を開き、**`3275:7020`（Interface 00 / `MI_00`）** に **WinUSB** をインストールします（`MI_01` の IR は不要）。
5. **完了**
   - 以後は TVTest を起動するだけで「自動ファーム転送 → 再認識 → 選局・内蔵 B-CAS 復号」まで全自動で動作します。

## 実機動作確認（2026-11-22 時点）

検証環境: Windows 7 x64（VM）+ Zadig 2.7（`MI_00` へ `WinUSB` 適用）+ QRS-UT100B（`3275:7020`, 内蔵 B-CAS スロット使用）

| 検証項目 | 結果 |
|---|---|
| `bon_test.exe`（UHF 21ch、5 秒受信） | `OpenTuner` / `SetChannel` OK、`32.09 dB`、37,121 パケット、`sync_err=0` / `tei=0` |
| **TVTest 0.10.0（64bit）** + MSVC ビルド `BonDriver_Friio.dll` | チャンネルスキャン・選局・内蔵 B-CAS 復号 OK、`33.91 dB / 14.15 Mbps`、**`E 0 / S 0`** |
