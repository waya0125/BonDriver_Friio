# BonDriver_Friio

Quixun **QRS-UT100B**（`3275:7020`）および SKNET **MonsterTV U3S**（`3275:9015`）向けのモダンな Windows 用 **BonDriver（`IBonDriver2`）** です。  
Linux 向け録画ツール `recfriio` の設計をもとに、**WinUSB 直接呼び出し・64bit / 32bit 両対応・外部ランタイム依存なし** で実装しています。

## 対応機種

| 機種 | USB ID | 放送波 | 内蔵 B-CAS | 対応 Windows | 状態 | 詳細・手順 |
|---|---|---|---|---|---|---|
| Quixun **QRS-UT100B** | `3275:7020` | 地デジ（ISDB-T / CATV） | **対応**（`EnableB25=1`） | Windows 7〜11 | **実機動作確認済み** | [docs/QRS-UT100B.md](docs/QRS-UT100B.md) |
| SKNET **MonsterTV U3S** | `3275:9015` | 3 波（ISDB-T / BS / CS） | 非対応（外付け PC/SC） | Windows 8.1〜11 | 実装済み（**実機未確認**） | [docs/SK-MTVU3S.md](docs/SK-MTVU3S.md) |

> [!NOTE]
> TVTest / EDCB 用は C++ ABI 互換のため、GitHub Releases の **MSVC ビルド済み DLL** を使用してください。

## ビルド方法

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

## ライセンス

MIT License / BSD-2-Clause（`recfriio` および `arib25` の知見・テストベクタを使用）
