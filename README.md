# ShutterClock

定点カメラ（タイムラプス・フェノロジー観測）用の低消費電力インターバルタイマです。
Nikon デジタル一眼レフをリモート端子から有線レリーズし、12 V ソーラー充電バッテリーからカメラへ給電することもできます。
時刻合わせ・撮影スケジュール設定・撮影ログ回収は、スマホ（Android Chrome）の Web Bluetooth ページで行います。
積雪深ロガー [SnowGauge](https://github.com/0kam/SnowGauge) の姉妹プロジェクトです。

*ShutterClock is a low-power BLE interval timer for fixed-point cameras: XIAO nRF52840 (Zephyr), wired Nikon remote release via optocouplers, optional camera power from a 12 V solar battery, Web Bluetooth field app. Documentation is in Japanese.*

## 状態（2026-10-06）

設計の初期段階です。初期決定事項と未決事項を [設計仕様書 v0.4](ShutterClock_設計仕様書_v0.4.md) にまとめています。
対象機種は D7200・D7500（必須）と D7000・D7100・D800・D810（任意）。次は実カメラでの WebUSB 動作確認と、タイミング・消費電流の測定です。

## 開発者向け

- 設計仕様書（要件・設計判断・未決事項・改版履歴）: [ShutterClock_設計仕様書_v0.4.md](ShutterClock_設計仕様書_v0.4.md)
- SnowGauge から流用したファイルの一覧: [PROVENANCE.md](PROVENANCE.md)
- 作業引き継ぎ（AI エージェント向け）: [CLAUDE.md](CLAUDE.md)
