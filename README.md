# BonDriver_Friio

Quixun QRS-UT100B と SKNET MonsterTV U3S を、Windows の TVTest・EDCB で使う BonDriver（`IBonDriver2`）です。  
Linux の録画コマンド [recfriio](https://github.com/waya0125/recfriio) の設計をもとに、WinUSB で機器を直接操作する形で書きました。  
x64・x86 の両方に対応し、外部のランタイムは不要です。

## 対応機種

| 機種 | USB ID | 放送波 | B-CAS | 対応 Windows | 詳細・動作確認 |
|---|---|---|---|---|---|
| Quixun QRS-UT100B | `1738:5211` → `3275:7020` | 地デジ・CATV（1 チューナー） | 本体のスロット | 7〜11（x64・x86） | [docs/QRS-UT100B.md](docs/QRS-UT100B.md) |
| SKNET MonsterTV U3S | `3275:9015` | 地デジ・BS・110 度 CS（1 チューナー） | 本体のスロット | 8.1〜11（x64・x86） | [docs/SK-MTVU3S.md](docs/SK-MTVU3S.md) |

## 関連プロジェクト

- Linux の録画コマンド: [recfriio](https://github.com/waya0125/recfriio)
- 手元のチューナーの動作確認の一覧: [チューナー動作確認一覧](https://waya0125.github.io/tuners/)

## 必要なもの

- [Zadig](https://zadig.akeo.ie/)（WinUSB ドライバーの割り当てに使う）
- B-CAS カード（本体のスロットに挿す。外付けのカードリーダーも使える）
- QRS-UT100B: ファーム `asv5211-firmware.bin`。  
  付属 CD のローダー（`Quixun_AS11Loader.sys`）から、recfriio の `ut100b-firmware extract` で取り出します。  
  メーカーのサイトはなくなっていて、[製品ページ](http://web.archive.org/web/20100224054943/http://www.quixun.co.jp:80/product/qrs-ut100b.html)・[サポートページ](http://web.archive.org/web/20100212202729/http://www.quixun.co.jp:80/support/qrs-ut100b.html)が Wayback Machine に残っています。
- MonsterTV U3S: ファームやメーカーのドライバーは不要です（[製品ページ](https://sknet-web.co.jp/product/mtvu3s/index.html)・[ダウンロード](https://sknet-web.co.jp/product/mtvu3s/download.html)）。

ファームはメーカーの著作物なので、このリポジトリには含めていません。

## 入手

[Releases](https://github.com/waya0125/BonDriver_Friio/releases) から、MSVC でビルドした x64 版と x86 版の DLL を入手できます。  
TVTest・EDCB では、C++ の ABI を合わせるために、この MSVC でビルドした DLL を使ってください。

## 準備

機種ごとに手順が違います。

- QRS-UT100B: [docs/QRS-UT100B.md の「準備」](docs/QRS-UT100B.md#2-準備)（Zadig を 2 回使います）
- MonsterTV U3S: [docs/SK-MTVU3S.md の「準備」](docs/SK-MTVU3S.md#2-準備)

## 使い方

- チャンネル空間は、QRS-UT100B が `UHF`・`CATV`、MonsterTV U3S が `UHF`・`BS`・`CS` です。  
  TVTest でチャンネルスキャンをしてから使ってください。
- 本体のスロットのカードで復号する場合（`EnableB25=1`、既定）は、TVTest・EDCB 側の B-CAS カードリーダーの設定を「なし」にします。  
  外付けのカードリーダーを使う場合は、`EnableB25=0` にします。
- 2 機種を一緒につなぐ場合は、`Model` で機種を決められます。

### 設定（`BonDriver_Friio.ini`）

`BonDriver_Friio.ini.sample` を `BonDriver_Friio.ini` に名前を変えて、DLL と同じフォルダーに置きます。

```ini
[FRIIO]
; 使用する機種: auto / ut100b / u3s（既定 auto）
Model=auto

; 同じ機種を複数接続している場合のインデックス（0 始まり）
DeviceIndex=0

; 内蔵 B-CAS スロットで ARIB STD-B25 復号を行う（1=有効 / 0=無効、既定 1）
; ※QRS-UT100B・MonsterTV U3S 両対応。外付け PC/SC リーダーを使う場合は 0 に設定
EnableB25=1

; 選局直後の安定化フィルター（Settler）および B25 初期鍵待ちの頭切りを無効化する場合は 1
NoSettle=0

; QRS-UT100B 自動ファームウェア転送用（省略時は DLL と同じフォルダの asv5211-firmware.bin）
; FirmwarePath=asv5211-firmware.bin
; 起動アドレス（通常版 0x5399 = 21401、forNetbook 版 0x53e4 = 21476）
FirmwareIndex=21401
```

## ビルド

### Linux（コア単体テスト）

```bash
cmake -S . -B build-test
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

### Windows（MSVC）

```powershell
# x64
cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release

# x86
cmake -S . -B build-x86 -A Win32
cmake --build build-x86 --config Release
```

## 注意事項

- MonsterTV U3S は Windows 8.1 以降が必要です（Isochronous 転送に `WinUsb_ReadIsochPipeAsap` を使うため）。
- 機種ごとの注意は各機種の docs にあります。

## ライセンス

MIT License / BSD-2-Clause（`recfriio` と `arib25` の知見・テストベクタを使っています）
