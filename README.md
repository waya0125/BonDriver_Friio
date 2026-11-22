# BonDriver_Friio

Quixun **QRS-UT100B**（`3275:7020`）および SKNET **MonsterTV U3S**（`3275:9015`）向けのモダンな Windows 用 **BonDriver（`IBonDriver2`）** です。  
Linux 向け録画ツール `recfriio` の設計・検証結果をもとに、**WinUSB 直接呼び出し・64bit / 32bit 両対応・外部ランタイム依存なし** で実装しています。

## 対応機種と制約

| 機種 | USB ID | 放送波 | 内蔵 B-CAS | 対応 Windows | 状態 |
|---|---|---|---|---|---|
| Quixun **QRS-UT100B** | `3275:7020` | 地デジ（ISDB-T / CATV） | **対応**（`EnableB25=1`） | **Windows 7 / 8 / 10 / 11**（Bulk 転送） | **実機動作確認済み** |
| SKNET **MonsterTV U3S** | `3275:9015` | 3 波（ISDB-T / BS / 110 度 CS） | 非対応（外付け PC/SC 前提） | **Windows 8.1 / 10 / 11 専用**（Isochronous 転送） | 実装済み（**Windows 実機未確認**） |

> [!IMPORTANT]
> - **MonsterTV U3S は Windows 7 非対応・Windows 実機未確認です**: WinUSB の Isochronous API（`WinUsb_ReadIsochPipeAsap`）が Windows 8.1 以降で追加されたため、Windows 7 環境では UT100B のみ動作します（API は動的解決しているため、DLL のロード自体は Windows 7 でも正常に行えます）。
> - **QRS-UT100B のファームウェア**: 未初期化状態（`1738:5211`）の場合は、事前にファームウェアを転送して `3275:7020` にしておく必要があります。
> - **複合 USB デバイス（UT100B）の Zadig 設定**: `MI_00`（Interface 00）に WinUSB をインストールしてください（`MI_01` の IR は不要）。
> - **TVTest / EDCB 用は MSVC ビルド必須**: C++ ABI 互換のため、GitHub Releases の MSVC ビルド済み DLL を使用してください。

## 実機動作確認（2026-11-22 時点）

検証環境: Windows 7 x64（VM）+ Zadig 2.7（`MI_00` へ `WinUSB` 適用）+ QRS-UT100B（`3275:7020`, 内蔵 B-CAS スロット使用）

| 検証項目 | 結果 |
|---|---|
| コア単体テスト（ASIE5606 DES ヌルパケット復号・MULTI2・Settler） | 全項目 PASS（Linux `ctest` / GitHub Actions） |
| `bon_test.exe`（UHF 21ch、5 秒受信） | `OpenTuner` / `SetChannel` OK、`32.09 dB`、37,121 パケット、`sync_err=0` / `tei=0` |
| **TVTest 0.10.0（64bit）** + MSVC ビルド `BonDriver_Friio.dll` | チャンネルスキャン・選局・内蔵 B-CAS 復号 OK、`33.91 dB / 14.15 Mbps`、**`E 0 / S 0`** |
| MonsterTV U3S（`3275:9015`） | **未確認**（Windows 8.1 以降の実機環境で今後検証予定） |

## 使い方（Windows）

### QRS-UT100B の初回セットアップ手順（重要）

QRS-UT100B は USB に挿した直後はファームウェア未転送状態（**`1738:5211`**）として認識され、ファームを転送するとチューナー本体（**`3275:7020`**）として再認識されます。  
`BonDriver_Friio` は `OpenTuner()` 時に自動ファームウェア転送を行うため、古い未署名ローダードライバ（`Quixun_AS11Loader.sys`）不要で Windows 11 でも動作します。

1. **ファームウェアの準備**: 製品付属 CD の `Quixun_AS11Loader.sys` から `recfriio` の `ut100b-firmware extract` で `asv5211-firmware.bin`（16 KB）を取り出し、`BonDriver_Friio.dll` と同じフォルダに配置します。
   - ※forNetbook 版ファームの場合は `BonDriver_Friio.ini` に `FirmwareIndex=21476`（`0x53e4`）を指定してください。
2. **Zadig（1 回目: `1738:5211`）**: UT100B を挿した直後に [Zadig](https://zadig.akeo.ie/) を開き、`Options → List All Devices` にチェックを入れて **`1738:5211`** に **WinUSB** をインストールします。
3. **TVTest を一度起動**: `BonDriver_Friio.dll` を開くと自動でファームが転送され、UT100B が **`3275:7020`** として再認識されます（初回のみ `3275:7020` にまだ WinUSB が無いため一度失敗します）。
4. **Zadig（2 回目: `3275:7020` の `MI_00`）**: 再び Zadig を開き、**`3275:7020`（Interface 00 / `MI_00`）** に **WinUSB** をインストールします（`MI_01` の IR は不要）。
5. **完了**: 以後は TVTest を起動するだけで「自動ファーム転送 → 再認識 → 選局・内蔵 B-CAS 復号」まで全自動で動作します。

### MonsterTV U3S のセットアップ手順（Windows 8.1 / 10 / 11）

1. [Zadig](https://zadig.akeo.ie/) で **`3275:9015`** に **WinUSB** ドライバをインストールします。
2. TVTest / EDCB のアーキテクチャに合わせて `BonDriver_Friio.dll`（x64 または x86）を配置します。
3. 外付け PC/SC カードリーダーを TVTest 側で設定して使用します。

## ビルド方法

### Linux 上でのコア単体テスト

```bash
cmake -S . -B build-test
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

### Windows（MSVC）でのビルド

```powershell
# x64
cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release

# x86
cmake -S . -B build-x86 -A Win32
cmake --build build-x86 --config Release
```

## ライセンス

MIT License / BSD-2-Clause（`recfriio` および `arib25` の知見・テストベクタを使用）
