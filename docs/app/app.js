/* ShutterClock Web Bluetooth page: project-specific part.
 * Generic pieces (from SnowGauge): cbor.js, smp.js, the panels below driven
 * by RECORD_SCHEMA / SETTINGS_SCHEMA. Project-specific: the two schemas and
 * the installation / test-shot panel (custom GATT, firmware/src/shot_gatt.h).
 */
'use strict';

const APP_VERSION = '2026-10-20a';

/* ---------- project schemas ---------- */

const DEVICE_NAME_PREFIX = 'SC-';
const FS_DIR = '/lfs1/';
const SITE_FILE = FS_DIR + 'site.json';

/* 40-byte record v1, see firmware/src/record.h / docs/record_format.md */
const RECORD_SCHEMA = {
  size: 40, magic: 0x4353, version: 1,
  fields: [
    ['epoch', 'u32', 4], ['seq', 'u32', 8],
    ['vbat_mv', 'u16', 12], ['vbat_load_mv', 'u16', 14],
    ['cam_idle_ma', 'u16', 16], ['cam_peak_ma', 'u16', 18],
    ['t_boot_ms', 'u16', 20, { none: 0xffff }], ['shot_ms', 'u16', 22],
    ['t_awake_ms', 'u16', 24, { none: 0xffff }], ['cam_on_ms', 'u16', 26],
    ['temp_c', 'i16', 28, { scale: 10 }], ['result', 'i8', 30], ['n_shots', 'u8', 31],
  ],
  flags: { 0: 'time_synced', 1: 'time_estimated', 2: 'shot_detected', 3: 'write_done', 4: 'manual', 5: 'first_after_boot', 6: 'always_on', 7: 'keepalive' },
};

/* Settings exposed over SMP settings mgmt (firmware/src/config.h). Pickers only: nothing is typed. */
const INTERVAL_CHOICES = [[0, '自動撮影しない'], [5, '5 分'], [10, '10 分'], [15, '15 分'], [20, '20 分'], [30, '30 分'], [60, '1 時間'], [120, '2 時間'], [180, '3 時間'], [240, '4 時間'], [360, '6 時間'], [720, '12 時間'], [1440, '24 時間']];
const SETTINGS_SCHEMA = [
  { key: 'sc/sched/start_min', type: 'u16', kind: 'hhmm', label: '撮影開始' },
  { key: 'sc/sched/end_min', type: 'u16', kind: 'hhmm', label: '撮影終了', help: '開始と同じなら終日。開始より前なら夜をまたぐ' },
  { key: 'sc/sched/interval_min', type: 'u16', kind: 'choice', choices: INTERVAL_CHOICES, label: '撮影間隔' },
  { key: 'sc/sched/tz_min', type: 'i16', kind: 'auto', label: 'タイムゾーン' },
  { key: 'sc/cam/mode', type: 'u8', kind: 'choice', choices: [[0, '撮影ごとに給電（省電力）'], [1, '常時給電']], label: 'カメラ電源' },
  { key: 'sc/cam/ka_min', type: 'u16', kind: 'choice', choices: [[0, 'なし'], [2, '2 分'], [5, '5 分'], [10, '10 分'], [20, '20 分'], [30, '30 分']], label: '撮影後の時計保持通電', help: '撮影ごとに給電する場合、カメラ内蔵時計の充電のために低電力で通電する時間' },
  { key: 'sc/cam/n_shots', type: 'u8', kind: 'choice', choices: [[1, '1 枚'], [2, '2 枚'], [3, '3 枚'], [5, '5 枚']], label: '1 回の枚数' },
  { key: 'sc/cam/af_ms', type: 'u16', kind: 'choice', choices: [[100, '0.1 秒'], [300, '0.3 秒'], [500, '0.5 秒'], [1000, '1 秒'], [2000, '2 秒']], label: '半押し → 全押しの間', advanced: true },
  { key: 'sc/cam/shutter_ms', type: 'u16', kind: 'choice', choices: [[100, '0.1 秒'], [200, '0.2 秒'], [500, '0.5 秒'], [1000, '1 秒']], label: '全押しの長さ', advanced: true },
  { key: 'sc/cam/boot_ms', type: 'u16', kind: 'choice', choices: [[1500, '1.5 秒'], [2500, '2.5 秒'], [4000, '4 秒'], [6000, '6 秒']], label: '給電から撮影までの待ち', advanced: true },
  { key: 'sc/cam/vbat_min_mv', type: 'u16', kind: 'choice', choices: [[0, '制限なし'], [11000, '11.0 V'], [11500, '11.5 V'], [12000, '12.0 V'], [12500, '12.5 V']], label: '撮影をやめる電池電圧', advanced: true },
  { key: 'sc/cam/shot_gap_ms', type: 'u16', kind: 'choice', choices: [[500, '0.5 秒'], [1000, '1 秒'], [2000, '2 秒'], [5000, '5 秒']], label: '連続撮影の間隔', advanced: true },
  { key: 'sc/cam/write_timeout_s', type: 'u8', kind: 'choice', choices: [[10, '10 秒'], [20, '20 秒'], [40, '40 秒'], [60, '60 秒']], label: '待機に戻るまでの最大待ち', advanced: true },
];

/* Shot GATT (firmware/src/shot_gatt.h) */
const SHOT_SVC = '53430001-6e63-4d61-8b0c-7a8e9f0b1c2d';
const SHOT_LIVE = '53430002-6e63-4d61-8b0c-7a8e9f0b1c2d';
const SHOT_CTRL = '53430003-6e63-4d61-8b0c-7a8e9f0b1c2d';
const SHOT_STATUS = '53430004-6e63-4d61-8b0c-7a8e9f0b1c2d';

/* CSV columns (docs/record_format.md) */
const CSV_COLUMNS = ['device_id', 'site_name', 'lat', 'lon', 'alt_m', 'tz_offset_min', 'time_utc', 'time_local', 'seq', 'flags',
  'shot_detected', 'vbat_mv', 'vbat_load_mv', 'cam_idle_ma', 'cam_peak_ma', 't_boot_ms', 'shot_ms', 't_awake_ms', 'cam_on_ms', 'temp_c', 'result', 'n_shots'];

/* ---------- generic helpers ---------- */

const $ = id => document.getElementById(id);
const logEl = () => $('log');
function log(msg, cls = '') {
  const line = document.createElement('div');
  line.textContent = new Date().toLocaleTimeString() + '  ' + msg;
  if (cls) line.className = cls;
  logEl().prepend(line);
  while (logEl().children.length > 200) logEl().lastChild.remove();
}
function crc16ccitt(bytes, seed = 0xffff) { // Zephyr crc16_ccitt: reflected, poly 0x8408
  let crc = seed;
  for (const b of bytes) { crc ^= b; for (let i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1; }
  return crc;
}
function readField(dv, bytes, i, type, off) {
  switch (type) {
    case 'u32': return dv.getUint32(i + off, true);
    case 'u16': return dv.getUint16(i + off, true);
    case 'i16': return dv.getInt16(i + off, true);
    case 'i8': return dv.getInt8(i + off);
    default: return bytes[i + off];
  }
}
function decodeRecords(bytes, schema = RECORD_SCHEMA) {
  const out = []; const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const m0 = schema.magic & 0xff, m1 = schema.magic >> 8;
  let i = 0, bad = 0;
  while (i + schema.size <= bytes.length) {
    const ok = dv.getUint16(i, true) === schema.magic && bytes[i + 2] === schema.version &&
      dv.getUint16(i + schema.size - 2, true) === crc16ccitt(bytes.subarray(i, i + schema.size - 2));
    if (!ok) { bad++; let j = i + 1; while (j + 1 < bytes.length && !(bytes[j] === m0 && bytes[j + 1] === m1)) j++; if (j + 1 >= bytes.length) break; i = j; continue; }
    const r = { flags_raw: bytes[i + 3], flags: [] };
    for (const [b, n] of Object.entries(schema.flags)) if (r.flags_raw & (1 << b)) r.flags.push(n);
    for (const [name, type, off, opt] of schema.fields) {
      let v = readField(dv, bytes, i, type, off);
      if (opt && opt.none !== undefined && v === opt.none) v = null;
      else if (type === 'i16' && v === -32768) v = null;
      else if (opt && opt.scale) v = v / opt.scale;
      r[name] = v;
    }
    out.push(r); i += schema.size;
  }
  return { records: out, bad };
}
function isoUTC(epoch) { return epoch ? new Date(epoch * 1000).toISOString().slice(0, 19) + 'Z' : ''; }
function isoLocal(epoch, tzMin) {
  if (!epoch) return '';
  const d = new Date((epoch + tzMin * 60) * 1000);
  const s = d.toISOString().slice(0, 19);
  const sign = tzMin < 0 ? '-' : '+', a = Math.abs(tzMin);
  return s + sign + String(Math.floor(a / 60)).padStart(2, '0') + ':' + String(a % 60).padStart(2, '0');
}
function hhmm(min) { return String(Math.floor(min / 60)).padStart(2, '0') + ':' + String(min % 60).padStart(2, '0'); }
function saveBlob(name, blob) { const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 5000); }
function toast(msg, cls = '') {
  const t = $('toast'); t.textContent = msg; t.className = 'show ' + cls;
  clearTimeout(toast._timer); toast._timer = setTimeout(() => { t.className = ''; }, 2500);
}
let busyDepth = 0;
function setAllEnabled(on) { document.querySelectorAll('button.needs-conn, select.needs-conn').forEach(el => { if (el.dataset.keep !== '1') el.disabled = !on; }); }
function busy(btn, fn) {
  return async () => {
    if (btn.disabled || busyDepth) return;
    busyDepth++;
    const label = btn.textContent; setAllEnabled(false); btn.textContent = '処理中…';
    try { const r = await fn(); if (r !== false) toast('完了'); }
    catch (e) { log(e.message, 'err'); toast('失敗: ' + e.message, 'err'); }
    finally { busyDepth--; btn.textContent = label; setAllEnabled(!!(state.smp && state.smp.connected)); $('btn-csv').disabled = $('btn-raw').disabled = state.records.length === 0; updateEraseButton(); updateCamButton(); }
  };
}
function updateEraseButton() {
  const b = $('btn-erase');
  b.disabled = !(state.smp && state.smp.connected) || !state.downloadedThisSession;
  $('erase-help').textContent = state.downloadedThisSession ? '' : '消去はこのセッションでデータをダウンロードした後に有効になります。';
}
function csvEscape(v) { if (v === null || v === undefined) return ''; const s = String(v); return /[",\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s; }

/* ---------- state ---------- */

const state = { device: null, smp: null, shot: null, camPowered: false, camAlwaysOn: false, site: null, settings: {}, records: [], rawFiles: {}, deviceId: '', timeOk: false, downloadedThisSession: false };

/* ---------- connection ---------- */

async function connect() {
  const b = $('btn-connect'); b.disabled = true; b.textContent = '接続中…';
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: DEVICE_NAME_PREFIX }],
      optionalServices: [SMP_SERVICE, SHOT_SVC],
    });
    state.device = device; state.deviceId = device.name || '';
    device.addEventListener('gattserverdisconnected', () => setConnected(false));
    state.smp = new SMPClient(log);
    await state.smp.connect(device);
    setConnected(true);
    log('接続: ' + device.name); toast('接続しました');
    await refreshStatus();
    try { await liveOn(); } catch (e) { log('カメラ状態の取得: ' + e.message, 'warn'); }
  } catch (e) { log('接続失敗: ' + e.message, 'err'); toast('接続失敗', 'err'); }
  b.disabled = false; b.textContent = '接続';
}
function setConnected(on) {
  $('btn-connect').hidden = on; $('btn-disconnect').hidden = !on;
  $('conn-state').textContent = on ? state.deviceId : '未接続';
  document.querySelectorAll('.needs-conn').forEach(el => el.disabled = !on);
  if (!on) { state.shot = null; state.downloadedThisSession = false; state.camPowered = false; $('cam-state').textContent = '-'; $('live-vbat').textContent = '---'; $('live-ma').textContent = '---'; updateCamButton(); }
  updateEraseButton();
}
async function refreshStatus() {
  try {
    const dt = await state.smp.os.getDateTime();
    const dev = new Date(dt + 'Z'), diff = Math.round((dev - Date.now()) / 1000);
    state.timeOk = Math.abs(diff) <= 60;
    $('dev-time').textContent = `${dt}Z（スマホとの差 ${diff >= 0 ? '+' : ''}${diff} 秒）`;
    $('dev-time').className = state.timeOk ? 'ok' : 'warn';
  } catch (e) { state.timeOk = false; $('dev-time').textContent = '時刻未設定または取得失敗 (' + e.message + ')'; $('dev-time').className = 'warn'; }
  await loadSettings();
  await loadSite();
}

/* ---------- sync: time + tz + location ---------- */

async function syncTime() {
  const iso = await state.smp.os.setDateTime(new Date());
  log('時刻設定: ' + iso + 'Z');
}
function getPosition() {
  return new Promise((resolve, reject) => {
    if (!navigator.geolocation) return reject(new Error('位置情報 API なし'));
    navigator.geolocation.getCurrentPosition(p => resolve(p.coords), e => reject(new Error(e.message)), { enableHighAccuracy: true, timeout: 20000, maximumAge: 60000 });
  });
}
function phoneTzMin() { return -new Date().getTimezoneOffset(); }
async function loadSite() {
  try {
    const bytes = await state.smp.fs.download(SITE_FILE);
    state.site = JSON.parse(new TextDecoder().decode(bytes));
  } catch (e) { state.site = null; }
  renderSite();
}
function renderSite() {
  const s = state.site;
  $('site-info').textContent = s ? `${s.name || '(名称なし)'}  lat ${s.lat} lon ${s.lon} alt ${s.alt_m ?? '-'} m (±${s.accuracy_m ?? '-'} m, ${s.source})  tz ${s.tz_min} min  設定 ${s.set_at}` : 'サイト情報なし';
}
async function syncAll() {
  const problems = [];
  try { await syncTime(); } catch (e) { problems.push('時刻: ' + e.message); }
  let coords = null;
  try { coords = await getPosition(); log(`位置取得: ${coords.latitude.toFixed(6)}, ${coords.longitude.toFixed(6)} ±${Math.round(coords.accuracy)} m`); }
  catch (e) { problems.push('位置: ' + e.message + '（手動入力可）'); }
  const tz = phoneTzMin();
  try {
    if (coords) await writeSite({ lat: +coords.latitude.toFixed(6), lon: +coords.longitude.toFixed(6), alt_m: coords.altitude == null ? null : Math.round(coords.altitude), accuracy_m: Math.round(coords.accuracy), source: 'gps' }, tz);
    else await writeTz(tz);
  } catch (e) { problems.push('サイト/タイムゾーン: ' + e.message); }
  await refreshStatus();
  if (problems.length) { problems.forEach(m => log(m, 'warn')); throw new Error(problems.length + ' 件の問題（ログ参照）'); }
  log('同期完了');
}
async function writeTz(tz) {
  await state.smp.settings.write('sc/sched/tz_min', LE.i16(tz));
  await state.smp.settings.save();
  log('タイムゾーン設定: ' + tz + ' 分');
}
async function writeSite(pos, tz) {
  const prev = state.site;
  let tzErr = null;
  const entry = { ...pos, name: $('site-name').value.trim() || (prev && prev.name) || '', tz_min: tz, tz_name: Intl.DateTimeFormat().resolvedOptions().timeZone, set_at: new Date().toISOString(), device_id: state.deviceId };
  const history = (prev && prev.history) ? prev.history.slice(-20) : [];
  if (prev) { const { history: _h, ...old } = prev; history.push(old); }
  const site = { ...entry, history };
  await state.smp.fs.upload(SITE_FILE, new TextEncoder().encode(JSON.stringify(site)));
  state.site = site; renderSite();
  log('サイト情報を保存');
  try { await writeTz(tz); } catch (e) { tzErr = e; }
  if (tzErr) throw new Error('タイムゾーン設定失敗: ' + tzErr.message);
}
async function manualSite() {
  const lat = parseFloat($('m-lat').value), lon = parseFloat($('m-lon').value), alt = parseFloat($('m-alt').value);
  if (isNaN(lat) || isNaN(lon)) throw new Error('緯度経度を入力してください');
  await writeSite({ lat, lon, alt_m: isNaN(alt) ? null : alt, accuracy_m: null, source: 'manual' }, phoneTzMin());
  await refreshStatus();
}

/* ---------- settings ---------- */

function settingValue(def, bytes) { return def.type === 'u32' ? LE.getU32(bytes) : def.type === 'i16' ? LE.getI16(bytes) : def.type === 'u8' ? bytes[0] : LE.getU16(bytes); }
function settingBytes(def, v) { return def.type === 'u32' ? LE.u32(v) : def.type === 'i16' ? LE.i16(v) : def.type === 'u8' ? new Uint8Array([v & 0xff]) : LE.u16(v); }
function setSelectValue(sel, v, labelFn) {
  if (![...sel.options].some(o => +o.value === v)) { const o = document.createElement('option'); o.value = v; o.textContent = labelFn(v); sel.appendChild(o); }
  sel.value = String(v);
}
function renderSettingsForm() {
  const tb = $('settings-body'); tb.innerHTML = '';
  let advRow = null;
  for (const def of SETTINGS_SCHEMA) {
    if (def.kind === 'auto') continue;
    if (def.advanced && !advRow) {
      advRow = document.createElement('tr');
      advRow.innerHTML = '<td colspan="2"><details><summary>詳細設定（カメラのタイミング）</summary><table><tbody id="settings-adv"></tbody></table></details></td>';
      tb.appendChild(advRow);
    }
    const tr = document.createElement('tr');
    let ctl;
    if (def.kind === 'hhmm') {
      ctl = `<select id="set-${def.key}" class="needs-conn big" disabled>` + Array.from({ length: 48 }, (_, i) => `<option value="${i * 30}">${hhmm(i * 30)}</option>`).join('') + '</select>';
    } else {
      ctl = `<select id="set-${def.key}" class="needs-conn big" disabled>` + def.choices.map(([v, l]) => `<option value="${v}">${l}</option>`).join('') + '</select>';
    }
    tr.innerHTML = `<td>${def.label}<div class="help">${def.help || ''}</div></td><td>${ctl}</td>`;
    (def.advanced ? $('settings-adv') : tb).appendChild(tr);
  }
}
async function loadSettings() {
  for (const def of SETTINGS_SCHEMA) {
    try {
      const bytes = await state.smp.settings.read(def.key);
      state.settings[def.key] = bytes;
      const v = settingValue(def, bytes);
      if (def.kind === 'hhmm') setSelectValue($('set-' + def.key), v, hhmm);
      else if (def.kind === 'choice') setSelectValue($('set-' + def.key), v, x => String(x));
    } catch (e) {
      log('設定読込失敗 ' + def.key + ': ' + e.message + (e.rc === 8 ? '（本体 FW が設定グループ未対応）' : ''), 'warn');
      if (e.rc === 8) break;
    }
  }
  const tz = tzMin(); $('tz-info').textContent = `UTC${tz >= 0 ? '+' : '-'}${hhmm(Math.abs(tz))}（同期時にスマホから自動設定）`;
}
async function saveSettings() {
  for (const def of SETTINGS_SCHEMA) {
    if (def.kind !== 'hhmm' && def.kind !== 'choice') continue;
    const bytes = settingBytes(def, parseInt($('set-' + def.key).value, 10));
    await state.smp.settings.write(def.key, bytes);
  }
  await state.smp.settings.save();
  log('設定を保存');
  await loadSettings();
}
function tzMin() { const b = state.settings['sc/sched/tz_min']; return b ? LE.getI16(b) : phoneTzMin(); }
function settingNum(key) { const b = state.settings[key]; const def = SETTINGS_SCHEMA.find(d => d.key === key); return b && def ? settingValue(def, b) : 0; }

/* ---------- data ---------- */

async function listRecordFiles() {
  const names = [];
  const now = new Date(); let y = now.getUTCFullYear(), m = now.getUTCMonth() + 1, missing = 0;
  for (let k = 0; k < 36 && missing < 3; k++) {
    const name = `${FS_DIR}rec_${y}${String(m).padStart(2, '0')}.bin`;
    try { const st = await state.smp.fs.stat(name); names.push({ name, len: st.len }); missing = 0; }
    catch (e) { missing++; }
    if (--m === 0) { m = 12; y--; }
  }
  try { const st = await state.smp.fs.stat(FS_DIR + 'rec_notime.bin'); names.push({ name: FS_DIR + 'rec_notime.bin', len: st.len }); } catch (e) { /* none */ }
  return names;
}
async function downloadAll() {
  const files = await listRecordFiles();
  log(`ファイル ${files.length} 件: ` + files.map(f => `${f.name.split('/').pop()} (${f.len} B)`).join(', '));
  state.records = []; state.rawFiles = {};
  for (const f of files) {
    const t0 = Date.now();
    const bytes = await state.smp.fs.download(f.name, (o, t) => { $('dl-progress').textContent = `${f.name.split('/').pop()}: ${o}/${t} B`; });
    state.rawFiles[f.name.split('/').pop()] = bytes;
    const { records, bad } = decodeRecords(bytes);
    if (bad) log(`${f.name}: 不正レコード ${bad} 件をスキップ`, 'warn');
    state.records.push(...records);
    log(`${f.name.split('/').pop()}: ${records.length} 件 (${((Date.now() - t0) / 1000).toFixed(1)} 秒)`);
  }
  state.records.sort((a, b) => (a.epoch - b.epoch) || (a.seq - b.seq));
  $('dl-progress').textContent = `合計 ${state.records.length} 件`;
  renderData();
  if (state.records.length) { exportCSV(); log('CSV を保存しました（端末のダウンロードフォルダ）'); }
  else log('レコードがありません（CSV は作成しません）');
  state.downloadedThisSession = true;
}
function qualityFlags(r, intervalMin, prev) {
  const q = [];
  if (r.result === -95) q.push('電池不足で未撮影');
  else if (!r.flags.includes('shot_detected')) q.push('撮影を検出できず');
  if (r.result !== -95 && !r.flags.includes('write_done')) q.push('待機に戻らず');
  if (!r.flags.includes('time_synced')) q.push(r.flags.includes('time_estimated') ? '時刻推定' : '時刻なし');
  if (prev && intervalMin && r.epoch && prev.epoch && (r.epoch - prev.epoch) > intervalMin * 60 * 1.5 && !r.flags.includes('manual')) q.push('欠測あり');
  if (r.vbat_mv && r.vbat_mv < 11800) q.push('電池低');
  return q;
}
function renderData() {
  const tz = tzMin();
  const iv = settingNum('sc/sched/interval_min');
  const tb = $('data-body'); tb.innerHTML = '';
  let prev = null, nWarn = 0;
  const limit = state.showAll ? 5000 : 30;
  const rows = [];
  for (const r of state.records) { const q = qualityFlags(r, iv, prev); if (q.length) nWarn++; rows.push([r, q]); prev = r; }
  for (const [r, q] of rows.slice(-limit)) {
    const tr = document.createElement('tr'); if (q.length) tr.className = 'warnrow';
    const t = r.epoch ? isoLocal(r.epoch, tz).slice(5, 16).replace('T', ' ') : '(時刻なし) #' + r.seq;
    const shot = r.result === -95 ? '電池' : r.flags.includes('shot_detected') ? '○' : '×';
    tr.innerHTML = `<td>${t}</td><td>${shot}${r.flags.includes('manual') ? ' 手動' : ''}</td><td>${r.cam_peak_ma}</td><td>${(r.vbat_mv / 1000).toFixed(2)}</td><td>${r.t_boot_ms ?? '-'}</td><td>${r.t_awake_ms == null ? '-' : (r.t_awake_ms / 1000).toFixed(1)}</td>`;
    tb.appendChild(tr);
    if (q.length) { const tq = document.createElement('tr'); tq.className = 'warnrow flagrow'; tq.innerHTML = `<td colspan="6">要確認: ${q.join('、')}</td>`; tb.appendChild(tq); }
  }
  $('data-summary').textContent = `${state.records.length} 件、要確認 ${nWarn} 件（一覧は新しい ${Math.min(limit, rows.length)} 件、現地時刻 月-日 時:分）`;
  $('btn-showall').hidden = state.records.length <= 30; $('btn-showall').textContent = state.showAll ? '新しい 30 件だけ表示' : 'すべて表示';
  drawChart();
  $('btn-csv').disabled = $('btn-raw').disabled = state.records.length === 0;
}
function drawChart() {
  const cv = $('chart'), ctx = cv.getContext('2d');
  const W = cv.width = cv.clientWidth * devicePixelRatio, H = cv.height = 220 * devicePixelRatio;
  ctx.clearRect(0, 0, W, H);
  const rs = state.records.filter(r => r.epoch);
  if (rs.length < 2) return;
  const t0 = rs[0].epoch, t1 = rs[rs.length - 1].epoch || t0 + 1;
  const pad = 40 * devicePixelRatio;
  const ys = rs.map(r => r.vbat_mv / 1000);
  const ymin = Math.min(...ys, 11), ymax = Math.max(...ys, 14.5), yr = (ymax - ymin) || 1;
  const X = e => pad + (e - t0) / (t1 - t0) * (W - 2 * pad), Y = v => H - pad - (v - ymin) / yr * (H - 2 * pad);
  ctx.font = `${12 * devicePixelRatio}px sans-serif`; ctx.fillStyle = '#666';
  ctx.fillText('電池 V（赤点 = 撮影を検出できず）', 4, 14 * devicePixelRatio);
  ctx.fillText(ymax.toFixed(1), 4, Y(ymax) + 4); ctx.fillText(ymin.toFixed(1), 4, Y(ymin) + 4);
  ctx.strokeStyle = '#1976d2'; ctx.lineWidth = 2 * devicePixelRatio; ctx.beginPath();
  rs.forEach((r, i) => { const x = X(r.epoch), y = Y(ys[i]); i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
  ctx.stroke();
  ctx.fillStyle = '#d32f2f';
  rs.forEach((r, i) => { if (!r.flags.includes('shot_detected')) ctx.fillRect(X(r.epoch) - 2 * devicePixelRatio, Y(ys[i]) - 2 * devicePixelRatio, 4 * devicePixelRatio, 4 * devicePixelRatio); });
  ctx.fillStyle = '#666';
  ctx.fillText(isoLocal(t0, tzMin()).slice(0, 16), pad, H - 8); ctx.fillText(isoLocal(t1, tzMin()).slice(0, 16), W - pad - 130 * devicePixelRatio, H - 8);
}
function exportCSV() {
  const tz = tzMin(), s = state.site || {};
  const lines = [CSV_COLUMNS.join(',')];
  for (const r of state.records) {
    const row = { device_id: state.deviceId, site_name: s.name ?? '', lat: s.lat ?? '', lon: s.lon ?? '', alt_m: s.alt_m ?? '', tz_offset_min: tz,
      time_utc: isoUTC(r.epoch), time_local: isoLocal(r.epoch, tz), seq: r.seq, flags: r.flags.join('|'),
      shot_detected: r.flags.includes('shot_detected') ? 1 : 0, vbat_mv: r.vbat_mv, vbat_load_mv: r.vbat_load_mv, cam_idle_ma: r.cam_idle_ma,
      cam_peak_ma: r.cam_peak_ma, t_boot_ms: r.t_boot_ms, shot_ms: r.shot_ms, t_awake_ms: r.t_awake_ms, cam_on_ms: r.cam_on_ms, temp_c: r.temp_c, result: r.result, n_shots: r.n_shots };
    lines.push(CSV_COLUMNS.map(c => csvEscape(row[c])).join(','));
  }
  const stamp = new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-');
  saveBlob(`${state.deviceId || 'shutterclock'}_${stamp}.csv`, new Blob(['﻿' + lines.join('\n') + '\n'], { type: 'text/csv;charset=utf-8' }));
  log('CSV 保存: ' + state.records.length + ' 件');
}
function exportRaw() {
  for (const [name, bytes] of Object.entries(state.rawFiles)) saveBlob(`${state.deviceId || 'shutterclock'}_${name}`, new Blob([bytes], { type: 'application/octet-stream' }));
  if (state.site) saveBlob(`${state.deviceId || 'shutterclock'}_site.json`, new Blob([JSON.stringify(state.site, null, 2)], { type: 'application/json' }));
}

/* ---------- installation / test shot (custom GATT) ---------- */

function camStateText(st, hold) {
  if (st & 8) return '常時給電（常に ON）';
  if (st & 1) return 'ON' + (hold ? `（あと ${Math.ceil(hold / 60)} 分で自動 OFF）` : '');
  if (st & 2) return '時計保持中（撮影後の充電。カメラは待機）';
  return 'OFF';
}
function updateCamUi(st, hold, vbat, ma) {
  state.camPowered = !!(st & 1); state.camAlwaysOn = !!(st & 8);
  $('cam-state').textContent = camStateText(st, hold);
  if (vbat !== undefined) $('live-vbat').textContent = (vbat / 1000).toFixed(2);
  if (ma !== undefined) $('live-ma').textContent = ma;
  updateCamButton();
}
function updateCamButton() {
  const b = $('btn-cam-toggle');
  if (state.camAlwaysOn) { b.textContent = '常時給電モード（常に ON）'; b.disabled = true; return; }
  b.textContent = state.camPowered ? 'カメラ電源を OFF にする' : 'カメラ電源を ON にする';
  b.className = 'needs-conn' + (state.camPowered ? ' secondary' : '');
}
async function shotConnect() {
  if (state.shot) return state.shot;
  let svc;
  try { svc = await state.device.gatt.getPrimaryService(SHOT_SVC); }
  catch (e) { throw new Error('テスト撮影用サービスが本体にありません'); }
  const live = await svc.getCharacteristic(SHOT_LIVE);
  const ctrl = await svc.getCharacteristic(SHOT_CTRL);
  const status = await svc.getCharacteristic(SHOT_STATUS);
  live.addEventListener('characteristicvaluechanged', ev => {
    const dv = ev.target.value;
    const vbat = dv.getUint16(0, true), ma = dv.getUint16(2, true), st = dv.getUint8(4), hold = dv.getUint16(5, true);
    updateCamUi(st, hold, vbat, ma);
  });
  await live.startNotifications();
  state.shot = { live, ctrl, status };
  return state.shot;
}
async function shotCmd(bytes) { const c = await shotConnect(); await c.ctrl.writeValue(new Uint8Array(bytes)); }
async function shotStatus() {
  const c = await shotConnect(); const dv = await c.status.readValue();
  const st = dv.getUint8(0), res = dv.getInt8(1), busyFlag = dv.getUint8(2), seq = dv.getUint8(3);
  const vbat = dv.getUint16(4, true), ma = dv.getUint16(6, true), peak = dv.getUint16(8, true), awake = dv.getUint16(10, true), hold = dv.getUint16(12, true);
  updateCamUi(st, hold, vbat, ma);
  return { st, res, busy: busyFlag, seq, vbat, ma, peak, awake, hold };
}
/* Send a control command and wait for the device to finish it (busy flag + seq). */
async function shotRun(bytes, timeoutMs = 15000) {
  const before = await shotStatus();
  if (before.busy) throw new Error('本体が前のコマンドを処理中です');
  await shotCmd(bytes);
  const want = (before.seq + 1) & 0xff, end = Date.now() + timeoutMs;
  let st;
  do { await new Promise(r => setTimeout(r, 500)); st = await shotStatus(); }
  while ((st.busy || st.seq !== want) && Date.now() < end);
  if (st.busy || st.seq !== want) throw new Error('本体の応答がありません（タイムアウト）');
  if (st.res) {
    const msg = { '-13': 'ダウンロード前の消去は拒否されました', '-5': '撮影を検出できませんでした（ケーブル・カメラ設定を確認）', '-95': '電池電圧が下限未満のため撮影しません', '-1': 'カメラ電源が入っていません' }[String(st.res)] || ('本体エラー ' + st.res);
    throw new Error(msg);
  }
  return st;
}
/* Live status (battery, camera current, camera state) streams while connected; the device stops it on disconnect. */
async function liveOn() { const st = await shotStatus(); if (!(st.st & 4)) await shotRun([0x01], 5000); }
async function testShot() {
  const r = $('shot-result'); r.className = 'status'; r.textContent = '撮影中…（最大 30 秒）';
  try {
    const st = await shotRun([0x10], 60000);
    const awake = st.awake === 0xffff ? '待機に戻らず（書き込みタイムアウト）' : `待機まで ${(st.awake / 1000).toFixed(1)} 秒`;
    r.className = 'status ok'; r.textContent = `撮影を検出しました: ピーク ${(st.peak / 1000).toFixed(2)} A、${awake}`;
    log(`テスト撮影: ピーク ${st.peak} mA、${awake}`);
  } catch (e) { r.className = 'status err'; r.textContent = '失敗: ' + e.message; throw e; }
}
async function camHold(min) { await shotRun([0x11, min & 255, min >> 8], 5000); await liveOn(); }
async function camToggle() {
  if (state.camPowered) { await camHold(0); toast('カメラ電源を OFF にしました'); }
  else { await camHold(60); toast('カメラ電源 ON（60 分で自動 OFF）'); }
}
/* Two taps at least 1.5 s apart (glove bounce / impatient re-taps do not count), within 6 s. */
function twoTap(btn, label, action) {
  const now = Date.now();
  if (btn.dataset.armed) {
    if (now - +btn.dataset.armed < 1500) return;
    delete btn.dataset.armed; btn.textContent = label; action(); return;
  }
  btn.dataset.armed = String(now); btn.textContent = '確認: 1.5 秒後にもう一度押すと実行';
  toast('もう一度押すと実行します（1.5 秒後から有効）');
  setTimeout(() => { if (btn.dataset.armed === String(now)) { delete btn.dataset.armed; btn.textContent = label; } }, 6000);
}
async function doErase() {
  if (!state.downloadedThisSession && state.records.length === 0) throw new Error('先にデータをダウンロードしてください');
  await shotRun([0x20, 0x45, 0x52, 0x41, 0x53, 0x45], 30000);
  log('全レコードを消去しました', 'warn'); state.records = []; state.rawFiles = {}; renderData();
}

/* ---------- demo mode (?demo=1): sample data without a device ---------- */

function demoRecords(days = 10) {
  const out = []; const t0 = Math.floor(Date.now() / 1000 / 86400) * 86400 - days * 86400 - 9 * 3600;
  let seq = 0;
  for (let d = 0; d < days; d++) {
    for (let h = 7; h <= 18; h++) {
      const epoch = t0 + d * 86400 + h * 3600;
      if (d === 4 && h === 12) continue; // a gap
      const bad = (d === 7 && h === 15);
      out.push({ flags_raw: bad ? 0x01 : 0x0d, flags: bad ? ['time_synced', 'write_done'] : ['time_synced', 'shot_detected', 'write_done'], epoch, seq: seq++,
        vbat_mv: Math.round(12650 - d * 25 + Math.sin(h / 3) * 120), vbat_load_mv: Math.round(12500 - d * 25), cam_idle_ma: 265, cam_peak_ma: bad ? 320 : 3050 + (h % 3) * 40,
        t_boot_ms: 540, shot_ms: bad ? 0 : 420, t_awake_ms: 9800 + (h % 4) * 100, cam_on_ms: 13200, temp_c: 8 + h / 2, result: bad ? -5 : 0, n_shots: 1 });
    }
  }
  return out;
}
function enterDemo() {
  state.deviceId = 'SC-DEMO';
  state.settings = { 'sc/sched/start_min': LE.u16(7 * 60), 'sc/sched/end_min': LE.u16(18 * 60), 'sc/sched/interval_min': LE.u16(60), 'sc/sched/tz_min': LE.i16(540),
    'sc/cam/mode': new Uint8Array([0]), 'sc/cam/ka_min': LE.u16(5), 'sc/cam/n_shots': new Uint8Array([1]), 'sc/cam/af_ms': LE.u16(300), 'sc/cam/shutter_ms': LE.u16(200),
    'sc/cam/boot_ms': LE.u16(2500), 'sc/cam/vbat_min_mv': LE.u16(11500), 'sc/cam/shot_gap_ms': LE.u16(1000), 'sc/cam/write_timeout_s': new Uint8Array([20]) };
  state.site = { name: 'デモ観測点', lat: 36.0548, lon: 140.1235, alt_m: 25, accuracy_m: 6, source: 'gps', tz_min: 540, set_at: new Date().toISOString() };
  state.records = demoRecords(); state.downloadedThisSession = true; state.timeOk = true;
  $('conn-state').textContent = 'SC-DEMO'; $('btn-connect').hidden = true; $('btn-disconnect').hidden = false;
  document.querySelectorAll('.needs-conn').forEach(el => el.disabled = false);
  $('dev-time').textContent = new Date().toISOString().slice(0, 19) + 'Z（スマホとの差 +1 秒）'; $('dev-time').className = 'ok';
  renderSite();
  for (const def of SETTINGS_SCHEMA) {
    if (def.kind === 'auto') continue;
    const v = settingValue(def, state.settings[def.key]);
    if (def.kind === 'hhmm') setSelectValue($('set-' + def.key), v, hhmm); else setSelectValue($('set-' + def.key), v, x => String(x));
  }
  $('tz-info').textContent = 'UTC+09:00（同期時にスマホから自動設定）';
  updateCamUi(0, 0, 12630, 0);
  $('dl-progress').textContent = '合計 ' + state.records.length + ' 件（デモデータ）';
  renderData(); updateEraseButton();
  log('デモ表示モード: 本体には接続していません。?demo=1 を外すと通常動作に戻ります', 'warn');
}

/* ---------- init ---------- */

if (typeof window !== 'undefined') window.addEventListener('load', () => {
  renderSettingsForm();
  if (!navigator.bluetooth) { $('nosupport').hidden = false; $('btn-connect').disabled = true; }
  $('btn-connect').onclick = connect;
  $('btn-disconnect').onclick = () => {
    if (!state.smp) return;
    if (!state.timeOk) { twoTap($('btn-disconnect'), '切断', () => state.smp.disconnect()); toast('時刻が未同期です。同期せずに切断するならもう一度押してください', 'err'); return; }
    state.smp.disconnect();
  };
  $('btn-sync').onclick = busy($('btn-sync'), syncAll);
  $('btn-time').onclick = busy($('btn-time'), async () => { await syncTime(); await refreshStatus(); });
  $('btn-site-manual').onclick = busy($('btn-site-manual'), manualSite);
  $('btn-settings-save').onclick = busy($('btn-settings-save'), saveSettings);
  $('btn-settings-reload').onclick = busy($('btn-settings-reload'), loadSettings);
  $('btn-download').onclick = busy($('btn-download'), downloadAll);
  $('btn-showall').onclick = () => { state.showAll = !state.showAll; renderData(); };
  $('btn-csv').onclick = () => { exportCSV(); toast('CSV をもう一度保存しました'); };
  $('btn-raw').onclick = () => { exportRaw(); toast('生データを保存しました'); };
  $('btn-cam-toggle').onclick = busy($('btn-cam-toggle'), camToggle);
  $('btn-shot').onclick = () => twoTap($('btn-shot'), 'テスト撮影', busy($('btn-shot'), testShot));
  $('btn-erase').onclick = () => twoTap($('btn-erase'), '全レコード消去', busy($('btn-erase'), doErase));
  window.addEventListener('resize', () => state.records.length && drawChart());
  $('app-version').textContent = 'アプリ版 ' + APP_VERSION;
  if (new URLSearchParams(location.search).get('demo')) { setTimeout(enterDemo, 50); }
  if ('serviceWorker' in navigator && location.protocol === 'https:') {
    navigator.serviceWorker.register('sw.js').then(reg => {
      reg.addEventListener('updatefound', () => {
        const w = reg.installing;
        w && w.addEventListener('statechange', () => { if (w.state === 'installed' && navigator.serviceWorker.controller) toast('新しい版を取得しました。次回起動時に切り替わります'); });
      });
    }).catch(() => {});
  }
});
if (typeof module !== 'undefined') module.exports = { decodeRecords, crc16ccitt, isoLocal, RECORD_SCHEMA, CSV_COLUMNS };
