<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo/logo_dark_800.png">
    <img src="docs/assets/logo/logo_light_800.png" alt="ShutterClock" width="480">
  </picture>
</p>

# ShutterClock

定点カメラ（タイムラプス・フェノロジー観測）用の低消費電力インターバルタイマです。
Nikon デジタル一眼レフをリモート端子から有線レリーズし、12 V ソーラー充電バッテリーからカメラへ給電することもできます。
時刻合わせ・撮影スケジュール設定・撮影ログ回収は、スマホ（Android Chrome）の Web Bluetooth ページで行います。
積雪深ロガー [SnowGauge](https://github.com/0kam/SnowGauge) の姉妹プロジェクトです。

*ShutterClock is a low-power BLE interval timer for fixed-point cameras: XIAO nRF52840 (Zephyr), wired Nikon remote release via optocouplers, optional camera power from a 12 V solar battery, Web Bluetooth field app. Documentation is in Japanese.*

## 状態（2026-10-11）

基板 v1.0 を JLCPCB に発注済み（5 枚、10 月中旬到着予定）、部品も発注済み。ファームウェア（`firmware/`、ビルド済み）とスマホアプリ（https://0kam.github.io/ShutterClock/app/ 、[デモ表示](https://0kam.github.io/ShutterClock/app/?demo=1)）の初版あり、実機検証はこれから。初期決定事項と未決事項を [設計仕様書 v0.15](ShutterClock_設計仕様書_v0.15.md) にまとめています。
対象機種は D7200・D7500（必須）と D7000・D7100・D800・D810（任意）。D7500 の消費電流は実測済み。次は部品・基板の到着後に実機でファームウェアを動かし、降圧モジュール・時計保持経路・撮影検出を確認します。

## 開発者向け

- 設計仕様書（要件・設計判断・未決事項・改版履歴）: [ShutterClock_設計仕様書_v0.15.md](ShutterClock_設計仕様書_v0.15.md)
- 部品購入リスト: [docs/01_parts.md](docs/01_parts.md)（[CSV](docs/01_parts.csv)、5 台分 [docs/01_order_5sets.csv](docs/01_order_5sets.csv)）
- 組み立てガイド（配置図・はんだ付け順・通電チェック）: [docs/02_assembly.md](docs/02_assembly.md)
- 基板: [pcb/README.md](pcb/README.md)（ピンマップの正）
- ファームウェア: [firmware/README.md](firmware/README.md)
- アプリ: [docs/app/README.md](docs/app/README.md)
- SnowGauge から流用したファイルの一覧: [PROVENANCE.md](PROVENANCE.md)
- 作業引き継ぎ（AI エージェント向け）: [CLAUDE.md](CLAUDE.md)
