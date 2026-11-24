# Quixun QRS-UT100B

## 1. 概要

| 項目 | 内容 |
|---|---|
| USB ID | `1738:5211`（ファームが無い状態）→ `3275:7020`（ファームを送ったあと） |
| 放送波 | 地デジ（UHF）・CATV |
| チューナー数 | 1 |
| B-CAS | 本体のスロット（`EnableB25=1`、既定。外付けのカードリーダーも可） |
| 対応 Windows | 7〜11（x64・x86） |
| Windows | [BonDriver_Friio](../README.md) |
| Linux | [recfriio](https://github.com/waya0125/recfriio)（`--hdp`） |
| 公式ページ | メーカーのサイトはなくなっている。[製品ページ](http://web.archive.org/web/20100224054943/http://www.quixun.co.jp:80/product/qrs-ut100b.html)・[サポートページ](http://web.archive.org/web/20100212202729/http://www.quixun.co.jp:80/support/qrs-ut100b.html)（Wayback Machine） |

## 2. 準備

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

## 3. 使い方

- チャンネル空間は `UHF`・`CATV` の 2 つです。
- 本体のスロットのカードで復号する場合は、TVTest・EDCB 側の B-CAS カードリーダーの設定を「なし」にします。
- 付属の `bon_test.exe` で、TVTest を使わずに受信を試せます。

```powershell
# DLL のパス、チャンネル空間（0=UHF）、チャンネルの番号（8 = 21ch）、秒数
bon_test.exe BonDriver_Friio.dll 0 8 5
```

## 4. 動作確認の記録

2026-11-22 時点で確かめました。  
環境は Windows 7 x64（VM）+ Zadig 2.7（`MI_00` へ `WinUSB` 適用）+ QRS-UT100B（`3275:7020`, 内蔵 B-CAS スロット使用）です。

| 検証項目 | 結果 |
|---|---|
| `bon_test.exe`（UHF 21ch、5 秒受信） | `OpenTuner` / `SetChannel` OK、`32.09 dB`、37,121 パケット、`sync_err=0` / `tei=0` |
| **TVTest 0.10.0（64bit）** + MSVC ビルド `BonDriver_Friio.dll` | チャンネルスキャン・選局・内蔵 B-CAS 復号 OK、`33.91 dB / 14.15 Mbps`、**`E 0 / S 0`** |

## 5. 注意事項

- 初回だけ、`3275:7020` にまだ WinUSB が割り当てられていないため、TVTest の 1 回目の起動は失敗します（準備の 3・4）。
- forNetbook 版のファームの場合は、`BonDriver_Friio.ini` に `FirmwareIndex=21476`（`0x53e4`）を指定してください。

## 6. 技術メモ

機器の中身（ASV5211・ASIE5606・復調部・チューナー）と、Linux での動作確認の記録は、recfriio の [docs/QRS-UT100B.md](https://github.com/waya0125/recfriio/blob/master/docs/QRS-UT100B.md) にまとめています。
