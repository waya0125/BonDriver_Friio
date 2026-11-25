# SKNET MonsterTV U3S

## 1. 概要

| 項目 | 内容 |
|---|---|
| USB ID | `3275:9015`（ファームの転送は不要） |
| 放送波 | 地デジ・BS・110 度 CS |
| チューナー数 | 1 |
| B-CAS | 本体のスロット（`EnableB25=1`、既定。外付けのカードリーダーを使う場合は `EnableB25=0`） |
| 対応 Windows | 8.1〜11（x64・x86） |
| Windows | [BonDriver_Friio](../README.md) |
| Linux | [recfriio](https://github.com/waya0125/recfriio)（`--u3s`） |
| 公式ページ | [製品ページ](https://sknet-web.co.jp/product/mtvu3s/index.html)・[ダウンロード](https://sknet-web.co.jp/product/mtvu3s/download.html) |

## 2. 準備

1. [Zadig](https://zadig.akeo.ie/) を開き、**`3275:9015`** に **WinUSB** ドライバをインストールします。
2. TVTest / EDCB のアーキテクチャに合わせて `BonDriver_Friio.dll`（x64 または x86）を配置します。
3. UT100B と併用する場合など機種を固定したいときは、`BonDriver_Friio.ini` に `Model=u3s` を指定します。
4. 内蔵 B-CAS スロットで復号する場合は `EnableB25=1`（既定）のまま使います（TVTest / EDCB 側のカードリーダー設定は「なし」にします）。外付け PC/SC カードリーダーを使う場合は `BonDriver_Friio.ini` に `EnableB25=0` を設定します。

## 3. 使い方

- チャンネル空間は `UHF`・`BS`・`CS` の 3 つです。
- `BS` のチャンネル `BSnn_s` の `s` は、そのトランスポンダーの何番目の TS かを表します（TMCC の TSID の一覧から空きを除いた並びで数えます。recpt1・recisdb・Mirakurun の `channels.yml`、recfriio と同じ読み方です）。  
  そのトランスポンダーに `s` 番目の TS が無いときは、チャンネルの切り替えが失敗します（`BS15_3` など）。チャンネルスキャンでは、その枠は飛ばされます。
- 付属の `bon_test.exe` で、TVTest を使わずに受信を試せます（引数は DLL のパス、チャンネル空間、チャンネルの番号、秒数）。

## 4. 動作確認の記録

2026-11-23 時点で確かめました。  
環境は物理 Windows PC（x64）+ WinUSB + MonsterTV U3S（`3275:9015`、内蔵 B-CAS スロット使用）+ MSVC ビルド `BonDriver_Friio.dll` / `bon_test.exe` です。

| 検証項目 | 結果 |
|---|---|
| `bon_test.exe`（UHF 21ch、10 秒、`EnableB25=1`） | `32.96 dB`、171,328 パケット、`sync_err=0` / `tei=0` / `cc_err=0` / `scrambled=0` |
| `bon_test.exe`（BS15_0、10 秒、`EnableB25=1`） | `13.81 dB`、143,453 パケット、`sync_err=0` / `tei=0` / `cc_err=0` / `scrambled=0` |
| **TVTest 0.10.0（64bit）** + `BonDriver_Friio.dll` | 選局・内蔵 B-CAS 復号 OK、`35.42 dB / 29.90 Mbps`、**`D 0 / E 0 / S 0`** |

## 5. 注意事項

> [!IMPORTANT]
> - **Windows 7 非対応**: MonsterTV U3S は Isochronous 転送を使うため、Windows 8.1 以降で追加された WinUSB API（`WinUsb_ReadIsochPipeAsap`）が必要です。
> - **内蔵 B-CAS スロットの制約**: 実機確認済みの ATR・初期設定・ECM 応答形式に対応します（送信 APDU 上限 124 バイト / ECM 本体 118 バイト、ECM 失敗時のカード再接続は 1 回まで）。未対応のカードや長い ECM を使う放送では、`BonDriver_Friio.ini` で `EnableB25=0` に設定し、TVTest / EDCB 側の外付け PC/SC カードリーダーを使ってください。

> [!NOTE]
> QEMU のソフトウェア USB パススルー（`usb-host` + `qemu-xhci`）環境では、`EnableB25=0` でも Isochronous 転送が一時的に途切れて `cc_err` が発生します。実環境では物理 Windows PC の USB ポート、または PCIe USB コントローラーのパススルー環境を使ってください。

## 6. 技術メモ

機器の中身（EM28178・TC90522・FC0013・QM1D1C0042）と、Linux での動作確認の記録は、recfriio の [docs/SK-MTVU3S.md](https://github.com/waya0125/recfriio/blob/master/docs/SK-MTVU3S.md) にまとめています。
