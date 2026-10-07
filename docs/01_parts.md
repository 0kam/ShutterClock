# 01. 部品購入（ShutterClock 部品表 / BOM）

対象: **ブレッドボード試作（2026-10 発注分）**。回路は設計仕様書 v0.11 §6（独立レビュー反映済み）で、PCB 化の前にブレッドボードで検証します。
購入先は秋月電子を優先し、カメラ側のアクセサリは Amazon.co.jp です。価格・在庫は 2026-10-07 時点（税込）。

> 設計の根拠は [設計仕様書](../ShutterClock_設計仕様書_v0.11.md) §6、電気的なつながりの正は（PCB 設計後）`pcb/README.md` に移します。
> 注文用の CSV（同じ内容）: [01_parts.csv](01_parts.csv)

## 1. 買い方の目安

| 作るもの | 必要な表 |
|---|---|
| ブレッドボード試作（1 台） | (a) 回路部品 + (b) ブレッドボード用 + (c) カメラ側 |
| PCB 版（後日） | (a) + (d)。基板は JLCPCB |

秋月は 10 本・20 本・100 本売りの部品が多いので、「数量」は**回路 1 台に載る個数**、「備考」に販売単位を書いています。

## 2. 部品表

### (a) 回路部品（1 台分）

| 記号 | 部品 | 値/型番 | 数量 | 購入先 | 単価 | 備考 |
|---|---|---|---|---|---|---|
| A1 | マイコン（BLE） | Seeed **XIAO nRF52840**（非 Sense、秋月表記「XIAO BLE nRF52840」） | 1 | [秋月 117341](https://akizukidenshi.com/catalog/g/g117341/) | ¥1,780 | 在庫 269。予備 +1 推奨 |
| U1 | 3.3 V LDO（常時 ON） | **NJW4181U3-33B**（SOT-89、入力 5〜35 V、Iq 9 µA、100 mA） | 1 | [秋月 113996](https://akizukidenshi.com/catalog/g/g113996/) | ¥60 | 表面実装。ブレッドボードでは変換基板 AE-SOT89 に載せる（(b)） |
| U2 | カメラ用 9 V 降圧 | **MBC2596-01**（LM2596-ADJ モジュール、入力 4〜35 V、3 A） | 1 | [秋月 131750](https://akizukidenshi.com/catalog/g/g131750/) | ¥1,900 | **在庫僅少、10 月中旬入荷予定**。2 個買って 1 個予備。組立時に出力を **9.4 V** に調整、赤 LED を外す |
| Q1, Q2, Q5 | P-ch MOSFET（逆接保護 / カメラ電源スイッチ / 時計保持スイッチ） | **2SJ334**（−60 V / −30 A） | 3 | [秋月 102846](https://akizukidenshi.com/catalog/g/g102846/) | ¥110 | ピン G-D-S。10 個入り [115414](https://akizukidenshi.com/catalog/g/g115414/) ¥990 もある |
| Q3, Q6, Q7 | N-ch MOSFET（Q2/Q5 のゲート駆動 / 時計保持フォロワ） | **2SK4017(Q)**（60 V / 5 A） | 3 | [秋月 107597](https://akizukidenshi.com/catalog/g/g107597/) | ¥30 | ピン G-D-S。在庫あり（10 月上旬入荷予定の表示あり） |
| ZD1, ZD2, ZD3 | Q1 / Q2 / Q5 の G-S 保護 | 15 V ツェナー **GDZJ15C** 500 mW | 3 | [秋月 115169](https://akizukidenshi.com/catalog/g/g115169/) | 10 本 ¥90 | 単品は [107498](https://akizukidenshi.com/catalog/g/g107498/) ¥10 |
| ZD4 | 時計保持フォロワのゲート基準 | 9.1 V ツェナー **BZX55C9V1** 500 mW | 1 | [秋月 115721](https://akizukidenshi.com/catalog/g/g115721/) | 20 本 ¥100 | 1N4148 と直列で基準 ≈ 9.7 V → 出力 ≈ 7.3〜8.5 V（10 V ツェナーは秋月に無い） |
| D6 | ZD4 と直列（+0.6 V） | **1N4148** | 1 | [秋月 100941](https://akizukidenshi.com/catalog/g/g100941/) | 50 本 ¥200 | |
| D1, D2 | TVS（2 本直列） | **P4KE15A**（400 W、DO-41） | 2 | [秋月 129633](https://akizukidenshi.com/catalog/g/g129633/) | 10 本 ¥150 | 直列でスタンドオフ 25.6 V、クランプ最大約 42 V |
| D4 | U2 出力の逆流防止 | ショットキー **1N5822**（40 V / 3 A） | 1 | [秋月 102229](https://akizukidenshi.com/catalog/g/g102229/) | 10 本 ¥350 | 5 A のスルーホール品は秋月に無い。4 A のピークは 20〜30 ms なので 3 A 定格で可 |
| F1 | ヒューズ | ガラス管 **MF51NR 250 V 5 A**（5.2×20 mm） | 1 | [秋月 107131](https://akizukidenshi.com/catalog/g/g107131/) | ¥40 | 普通溶断。タイムラグ 5 A は秋月に無いが、4 A×30 ms のピークは問題なし |
| F1 ホルダ | ヒューズクリップ | **FUC-03A**（基板用、5.2 mm） | 2 | [秋月 110521](https://akizukidenshi.com/catalog/g/g110521/) | ¥20 | 2 個で 1 本分。箱型の MF-563（114929）は 4 A 定格なので使わない |
| R3 | カメラ電流検出 | **0.1 Ω 3 W ±1 %**（RSMF3BR100F） | 1 | [秋月 111011](https://akizukidenshi.com/catalog/g/g111011/) | ¥20 | 待機 0.42 mA → 42 µV、4 A → 0.4 V。ADC で読む |
| U3, U4 | フォトカプラ（AF / SHUTTER） | **PC817**（Sharp PC817X3NSZ1B） | 2 | [秋月 113765](https://akizukidenshi.com/catalog/g/g113765/) | ¥30 | 在庫僅少。代替 UPC817CG [116090](https://akizukidenshi.com/catalog/g/g116090/) ¥15 |
| J3 | レリーズ用ジャック | 3.5 mm ステレオミニジャック **MJ-8435**（基板取付） | 1 | [秋月 109060](https://akizukidenshi.com/catalog/g/g109060/) | ¥90 | 1 = スリーブ、2 = チップ、3 = リング |
| JP1, JP2 | USB 時に外す / カメラ常時 ON | ピンヘッダ 1×2 + ジャンパピン | 2 | （ピンヘッダは (b) の 1×40 を折る。ジャンパピン [秋月 103691](https://akizukidenshi.com/catalog/g/g103691/) 要確認） | — | JP1 = U1 → XIAO 3V3、JP2 = Q3 D-S |
| SW1 | テスト撮影 / BLE 起動ボタン | タクトスイッチ 6 mm（DTS-63-N-V-BLK） | 1 | [秋月 103647](https://akizukidenshi.com/catalog/g/g103647/) | ¥15 | |
| C1 | 入力バルク | **100 µF / 50 V** ハイブリッド（Rubycon PZF） | 1 | [秋月 116871](https://akizukidenshi.com/catalog/g/g116871/) | ¥80 | 安価な代替: ニチコン HE 100 µF/50 V [108440](https://akizukidenshi.com/catalog/g/g108440/) 5 本 ¥100 |
| C2 | U1 入力（RC の C） | **10 µF / 50 V** 積セラ（5 mm） | 1 | [秋月 108155](https://akizukidenshi.com/catalog/g/g108155/) | ¥60 | |
| C3 | カメラ 9 V 側バルク | **4,700 µF / 16 V**（Rubycon ZLH） | 1 | [秋月 114096](https://akizukidenshi.com/catalog/g/g114096/) | ¥100 | φ16×25 mm。ピーク補償 |
| C4 | U1 出力 | **2.2 µF / 50 V** 積セラ（5 mm） | 1 | [秋月 108152](https://akizukidenshi.com/catalog/g/g108152/) | ¥30 | NJW4181 の指定値以上 |
| C6〜C8 | XIAO パスコン・ADC フィルタ | **0.1 µF / 50 V** 積セラ | 3 | [秋月 113582](https://akizukidenshi.com/catalog/g/g113582/) | 10 個 ¥100 | U1 入力、XIAO、A0、A1 |
| R4 | U1 入力（RC の R） | **100 Ω** 1/4 W | 1 | [秋月 125101](https://akizukidenshi.com/catalog/g/g125101/) | 100 本 ¥200 | |
| R20, R21 | J3 チップ / リング直列（保護） | **100 Ω** 1/4 W | 2 | （R4 と同じ袋） | — | |
| R6〜R9 | フォトカプラ LED 直列 / Q3・Q6 ゲート直列 | **1 kΩ** 1/4 W | 4 | [秋月 125102](https://akizukidenshi.com/catalog/g/g125102/) | 100 本 ¥100 | |
| R10〜R13 | Q3 / Q6 プルダウン、Q2 / Q5 G-S プルアップ | **100 kΩ** 1/4 W | 4 | [秋月 125104](https://akizukidenshi.com/catalog/g/g125104/) | 100 本 ¥200 | SnowGauge の R4/R5 と同じ |
| R5 | ZD4 バイアス（KA_SW から） | **47 kΩ** 1/4 W | 1 | [秋月 125473](https://akizukidenshi.com/catalog/g/g125473/) | 100 本 ¥200 | 約 60 µA、Q5 ON 中のみ。コード要確認 |
| R16, R18 | Q2 / Q5 ゲート直列（ツェナー電流制限） | **33 kΩ** 1/4 W | 2 | [秋月 125333](https://akizukidenshi.com/catalog/g/g125333/) | 100 本 ¥200 | レビュー BLOCKER 1 の修正 |
| R19 | Q7 ドレイン（突入電流制限） | **10 Ω 1 W** カーボン | 1 | [秋月 107956](https://akizukidenshi.com/catalog/g/g107956/) | 100 本 ¥350 | 4,700 µF への突入 ≤ 1.3 A |
| R1, R15, R17 | 電池分圧（上）/ Q1 ゲート / C3 ブリーダ | **1 MΩ** 1/4 W | 3 | [秋月 125105](https://akizukidenshi.com/catalog/g/g125105/) | 100 本 ¥200 | |
| R2 | 電池分圧（下） | **100 kΩ** 1/4 W | 1 | （R10〜R13 と同じ袋） | — | 30 V 入力で ADC ノード 2.7 V |
| R14 | A1 の RC フィルタ | **10 kΩ** 1/4 W | 1 | [秋月 125103](https://akizukidenshi.com/catalog/g/g125103/) | 100 本 ¥100 | |

### (b) ブレッドボード用（試作のみ）

| 部品 | 値/型番 | 数量 | 購入先 | 単価 | 備考 |
|---|---|---|---|---|---|
| ブレッドボード 830 穴 | EIC-102BJ | 1 | [秋月 100285](https://akizukidenshi.com/catalog/g/g100285/) | ¥1,980 | ジャンパ付きセット EIC-102J [102314](https://akizukidenshi.com/catalog/g/g102314/) ¥1,280 でも可 |
| ジャンパワイヤ（オス–オス） | 60 本以上セット | 1 | [秋月 130088](https://akizukidenshi.com/catalog/g/g130088/) | ¥300 | カメラ電源系は短く太い線で（4 A ピーク） |
| SOT-89 変換基板 | AE-SOT89 | 1 | [秋月 110835](https://akizukidenshi.com/catalog/g/g110835/) | 10 枚 ¥70 | U1 をブレッドボードに挿すため |
| ピンヘッダ 1×40 | PH-1x40SG | 1 | [秋月 100167](https://akizukidenshi.com/catalog/g/g100167/) | ¥35 | XIAO、変換基板、MBC2596-01 の足に折って使う |
| 安定化電源 | 12〜15 V、電流制限 **4 A 以上** | 1 | 手持ち | — | 2 A 制限ではカメラが Err になる |
| picowatt | 手持ち | 1 | — | — | 電流プロファイル用 |
| テスター | DC 電圧 0.01 V 単位 | 1 | 手持ち | — | U2 の 9.4 V 調整に必須 |

### (c) カメラ側

| 部品 | 値/型番 | 数量 | 購入先 | 価格 | 備考 |
|---|---|---|---|---|---|
| ダミーバッテリー | Nikon **EP-5B** パワーコネクター | 1 | [Amazon.co.jp B0042VJSN8](https://www.amazon.co.jp/dp/B0042VJSN8) | ¥2,318 | 全対象機種共通（EN-EL15 系）。ケーブル端をネジ端子に直接つなぐ |
| レリーズケーブル（D7000〜D7500） | 3.5 mm ステレオプラグ → MC-DC2 型 | 1 | [Amazon.co.jp B0C9YLTTSX](https://www.amazon.co.jp/dp/B0C9YLTTSX) | ¥926 | 「3.5mm-N3」。チップ / リング / スリーブの割り当てはテスターで確認 |
| レリーズケーブル（D800/D810、任意） | 2.5 mm → 10 ピン + 2.5 mm メス→3.5 mm オス変換 | 各 1 | [Amazon.co.jp B0D49L1SL3](https://www.amazon.co.jp/dp/B0D49L1SL3) + [B00KXMH1UW](https://www.amazon.co.jp/dp/B00KXMH1UW) | ¥1,701 + ¥580 | 3.5 mm 直の 10 ピンケーブルは国内に見当たらない |
| USB OTG アダプタ（§4.2 の確認用） | USB-C オス → USB-A メス | 1 | 手持ち / 量販店 | — | カメラ側は各機種の純正 USB ケーブル |

### (d) PCB 版で追加（後日、基板設計後に確定）

| 部品 | 値/型番 | 数量 | 購入先 | 備考 |
|---|---|---|---|---|
| プリント基板 | ShutterClock PCB v1.0 | 1 | JLCPCB | ガーバーは `pcb/` に置く |
| ネジ端子 2P 5.08 mm | TB111-2 | 2 | [秋月 102333](https://akizukidenshi.com/catalog/g/g102333/) ¥40 | J1 電池、J2 カメラ電源 |
| ピンソケット 1×7 | FH-1x7SG/RH | 2 | [秋月 104285](https://akizukidenshi.com/catalog/g/g104285/) ¥20 | XIAO を差し替え可能に |
| ピンソケット 1×4 | FH-1x4SG/RH | 1 | [秋月 110099](https://akizukidenshi.com/catalog/g/g110099/) ¥20 | MBC2596-01 用（単ピンに切って使う） |
| OS-CON 470 µF / 16 V | 16SEPC470M | 1 | [秋月 108292](https://akizukidenshi.com/catalog/g/g108292/) ¥80 | 任意。C3 に並列（低温 ESR 対策） |

## 3. 概算（ブレッドボード試作 1 台）

- 秋月: 約 ¥9,000（XIAO ¥1,780、MBC2596-01 ×2 ¥3,800、ブレッドボード ¥1,980、その他の袋物 ¥1,500 程度）
- Amazon: 約 ¥3,300（EP-5B ¥2,318 + レリーズケーブル ¥926）。D800/D810 用を足すと +¥2,300
- 抵抗・ツェナー・コンデンサは 10〜100 本袋なので、2 台目以降はほぼ XIAO + MBC2596-01 + EP-5B + ケーブルの約 ¥7,000 で済みます

## 4. 発注前の注意

- MBC2596-01 は在庫僅少（10 月中旬入荷予定）。先に注文しておくか、入荷を待つ。
- 1N5822 は 3 A 定格です。ブレッドボードで撮影時の発熱を触って確かめ、気になれば 2 本並列にします（10 本入りなので手持ちで足ります）。
- 2SJ334 の在庫は潤沢ですが、2SK4017 は「10 月上旬入荷予定」の表示が出ています（在庫数は十分）。
