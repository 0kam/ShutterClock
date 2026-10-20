# ShutterClock Web Bluetooth page

Single-page app for site visits: sync time / timezone / location, download the
shot log (CSV + raw), set the schedule and camera options, hold the camera on
for framing, fire a test shot. Talks SMP (mcumgr) over Web Bluetooth plus the
small shot GATT service (`firmware/src/shot_gatt.h`).

- **Public URL: https://0kam.github.io/ShutterClock/app/** (GitHub Pages from `docs/`).
- Device name prefix `SC-`. Works in Chrome on Android and PC; iPhone needs a
  Web-Bluetooth browser app (Bluefy). HTTPS or `localhost` only.
- Version string: `APP_VERSION` in `app.js`; bump it and the cache name in `sw.js` on every release.
- Local test: `python3 -m http.server 8787 --directory docs/app` then open http://localhost:8787/ in Chrome.
- Demo mode without a device: append `?demo=1`.

Files: `cbor.js`, `smp.js`, `sw.js` (copied from SnowGauge, see PROVENANCE.md),
`app.js` (ShutterClock schemas, panels, shot GATT), `index.html`, `manifest.webmanifest`, icons.

Panels: 1 同期 / 2 撮影ログ (download → CSV, battery chart, per-shot table with
quality flags) / 3 撮影スケジュール・カメラ設定 (window, interval, power mode,
keep-alive, shots per trigger; advanced timing under a disclosure) / 4 設置・
テスト撮影 (camera state / battery / current streamed while connected, one camera power toggle (60 min auto-off, off on disconnect), test shot with result, ERASE after download).
