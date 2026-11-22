# BonDriver_Friio

Quixun **QRS-UT100B**（`3275:7020`）および SKNET **MonsterTV U3S**（`3275:9015`）向けのモダンな Windows 用 **BonDriver（`IBonDriver2`）** です。  
Linux 向け録画ツール `recfriio` の設計・検証結果をもとに、**WinUSB 直接呼び出し・64bit / 32bit 両対応・外部ランタイム依存なし** で実装しています。

## 対応機種と制約

| 機種 | USB ID | 放送波 | 内蔵 B-CAS | 対応 Windows |
|---|---|---|---|---|
| Quixun **QRS-UT100B** | `3275:7020` | 地デジ（ISDB-T / CATV） | **対応**（`EnableB25=1`） | **Windows 7 / 8 / 10 / 11**（Bulk 転送） |
| SKNET **MonsterTV U3S** | `3275:9015` | 3 波（ISDB-T / BS / 110 度 CS） | 非対応（外付け PC/SC 前提） | **Windows 8.1 / 10 / 11 専用**（Isochronous 転送） |

> [!IMPORTANT]
> - **MonsterTV U3S は Windows 7 非対応です**: WinUSB の Isochronous API（`WinUsb_ReadIsochPipeAsap`）が Windows 8.1 以降で追加されたため、Windows 7 環境では UT100B のみ動作します（API は動的解決しているため、DLL のロード自体は Windows 7 でも正常に行えます）。
> - **QRS-UT100B のファームウェア**: 未初期化状態（`1738:5211`）の場合は、事前にファームウェアを転送して `3275:7020` にしておく必要があります。
> - **TVTest / EDCB 用は MSVC ビルド必須**: C++ ABI 互換のため、GitHub Releases の MSVC ビルド済み DLL を使用してください。

## 使い方（Windows）

1. [Zadig](https://zadig.akeo.ie/) などで対象チューナー（`3275:7020` または `3275:9015`）に **WinUSB** ドライバをインストールします。
2. TVTest / EDCB のアーキテクチャに合わせて `BonDriver_Friio.dll`（x64 または x86）を配置します。
3. 必要に応じて `BonDriver_Friio.ini.sample` を `BonDriver_Friio.ini` にリネームして `Model` や `EnableB25` を設定します。

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
