#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>KVM Switcher</title>
<link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect x='10' y='20' width='80' height='60' rx='8' fill='%231a1a1f' stroke='%23555' stroke-width='4'/><circle cx='35' cy='50' r='12' fill='%231D9E75'/><circle cx='65' cy='50' r='12' fill='%237F77DD'/></svg>">
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#111;color:#ccc;min-height:100vh;display:flex;justify-content:center;align-items:safe center;padding:1rem 0}
  .card{background:#1a1a1f;border:1px solid #2a2a30;border-radius:12px;padding:1.5rem;max-width:440px;width:92%}
  /* Touch / mobile: keep the centred-card look, just scale typography and tap
     targets up so nothing requires a pinch-to-zoom. Triggered by a coarse
     pointer OR a narrow viewport so it catches Pixel-class devices that
     report a >440 CSS-px viewport but are still touch-driven. */
  @media (pointer:coarse), (max-width:768px){
    .card{padding:1.5rem;max-width:480px}
    .top h1{font-size:18px}
    .sw{padding:22px 8px}
    .sw .n{font-size:32px}
    .sw .l{font-size:15px;margin-top:5px}
    .mon{padding:16px}
    .mon-name,.mon-input{font-size:16px}
    .mon-dot{width:9px;height:9px}
    .tag{font-size:14px;padding:7px 14px}
    .hk-label{font-size:14px}
    .hk-val{font-size:14px;padding:6px 12px}
    .btn{padding:16px;font-size:17px}
    .btn-secondary{font-size:15px;padding:13px}
    .hk-form p{font-size:14px}
    .hk-input{font-size:16px;padding:14px}
    .field label{flex:0 0 90px;font-size:14px}
    .field input{font-size:15px;padding:11px 13px}
    details.settings summary{font-size:15px;padding:13px 15px}
    .wifi-row{font-size:14px;padding:10px 13px}
    .foot{font-size:13px}
  }
  .top{display:flex;align-items:center;justify-content:space-between;margin-bottom:1.5rem}
  .top h1{font-size:14px;font-weight:500;color:#888;letter-spacing:.06em;text-transform:uppercase}
  .dot{width:8px;height:8px;border-radius:50%;transition:all .3s}
  .dot.i1{background:#1D9E75;box-shadow:0 0 6px #1D9E7580}
  .dot.i2{background:#7F77DD;box-shadow:0 0 6px #7F77DD80}
  .dot.off{background:#444;box-shadow:none}
  .switcher{display:flex;background:#111115;border-radius:10px;padding:4px;gap:4px;margin-bottom:1rem}
  .sw{flex:1;padding:14px 8px;border-radius:8px;text-align:center;cursor:pointer;border:1.5px solid transparent;transition:all .15s}
  .sw .n{font-size:20px;font-weight:500}
  .sw .l{font-size:11px;margin-top:3px}
  .sw.s1{background:#0F2D24;border-color:#1D9E7560}
  .sw.s1 .n{color:#5DCAA5}
  .sw.s1 .l{color:#5DCAA5;opacity:.7}
  .sw.s2{background:#1E1B3A;border-color:#7F77DD60}
  .sw.s2 .n{color:#AFA9EC}
  .sw.s2 .l{color:#AFA9EC;opacity:.7}
  .sw.off .n,.sw.off .l{color:#444}
  .monitors{display:flex;flex-direction:column;gap:6px;margin-bottom:1rem}
  .mon{display:flex;align-items:center;justify-content:space-between;background:#111115;border-radius:8px;padding:10px 12px}
  .mon-name{font-size:12px;color:#888;font-weight:500}
  .mon-right{display:flex;align-items:center;gap:8px}
  .mon-input{font-size:12px;font-weight:500}
  .mon-input.i1{color:#5DCAA5}
  .mon-input.i2{color:#AFA9EC}
  .mon-input.na{color:#444}
  .mon-dot{width:6px;height:6px;border-radius:50%}
  .mon-dot.ok{background:#1D9E75}
  .mon-dot.fail{background:#E24B4A}
  .tags{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:.75rem}
  .tag{font-size:11px;padding:4px 10px;border-radius:6px;letter-spacing:.02em}
  .t-awake{background:#0F2D24;color:#5DCAA5}
  .t-asleep{background:#2D2106;color:#EF9F27}
  .t-off{background:#1a1a20;color:#666}
  .t-down{background:#2a1418;color:#cc7a82}
  .hk-row{display:flex;align-items:center;justify-content:space-between;margin-bottom:1.25rem}
  .hk-label{font-size:11px;color:#555}
  .hk-val{font-size:11px;color:#888;background:#111115;border-radius:5px;padding:3px 8px;font-family:monospace}
  .btn{width:100%;padding:11px;font-size:13px;font-weight:500;background:#252530;border:1px solid #3a3a44;border-radius:8px;color:#ccc;cursor:pointer;transition:background .1s;letter-spacing:.02em}
  .btn:hover{background:#30303c}
  .btn:active{background:#1a1a24}
  .btn:disabled{opacity:.4;cursor:not-allowed}
  .btn-row{display:flex;gap:6px}
  .btn-row .btn{flex:1}
  .btn-secondary{background:#1a1a24;border-color:#2a2a34;color:#888;font-size:12px;padding:9px}
  .btn-secondary:hover{background:#22222e;color:#aaa}
  details.settings{margin-top:10px;border:1px solid #2a2a30;border-radius:8px;overflow:hidden}
  details.settings summary{padding:9px 12px;font-size:12px;color:#666;cursor:pointer;user-select:none;list-style:none;display:flex;align-items:center;justify-content:space-between}
  details.settings summary::-webkit-details-marker{display:none}
  details.settings summary::after{content:'›';font-size:16px;transition:transform .2s;display:inline-block}
  details[open].settings summary::after{transform:rotate(90deg)}
  details.settings summary:hover{color:#999;background:#1f1f26}
  .hk-form{padding:12px;display:flex;flex-direction:column;gap:8px;border-top:1px solid #2a2a30}
  .hk-form p{font-size:11px;color:#555;line-height:1.5}
  .hk-input{width:100%;padding:9px 12px;background:#111115;border:1px solid #3a3a44;border-radius:7px;color:#ccc;font-size:13px;font-family:monospace;cursor:pointer;outline:none;text-align:center;user-select:none;-webkit-user-select:none;line-height:1.6}
  .hk-input:focus{border-color:#555;background:#0d0d12}
  .hk-input.capturing{border-color:#7F77DD80;color:#AFA9EC}
  .hk-save{padding:9px;font-size:12px}
  .field{display:flex;align-items:center;gap:8px}
  .field label{font-size:11px;color:#666;flex:0 0 70px}
  .field input{flex:1;padding:7px 10px;background:#111115;border:1px solid #3a3a44;border-radius:6px;color:#ccc;font-size:12px;font-family:monospace;outline:none;min-width:0}
  .field input:focus{border-color:#555;background:#0d0d12}
  .btn-danger{background:#2a1418;border-color:#5a2a30;color:#cc7a82}
  .btn-danger:hover{background:#3a1c20}
  .status-msg{font-size:11px;color:#666;text-align:center;min-height:14px}
  .status-msg.ok{color:#5DCAA5}
  .status-msg.err{color:#E24B4A}
  .foot{margin-top:10px;text-align:center;font-size:10px;color:#444}
  .foot a{color:#555;text-decoration:none}
  .foot a:hover{color:#777}
  .wifi-row{display:flex;justify-content:space-between;align-items:center;margin-top:10px;padding:6px 10px;background:#111115;border-radius:7px;font-size:11px;color:#555}
  .wifi-ip{font-family:monospace;color:#666}
  .rssi-good{color:#1D9E75}
  .rssi-ok{color:#EF9F27}
  .rssi-weak{color:#E24B4A}
</style>
</head>
<body>
<div class="card">
  <div class="top">
    <h1>KVM switcher</h1>
    <div class="dot off" id="dot"></div>
  </div>
  <div class="switcher">
    <div class="sw off" id="sw1" onclick="setInput(1)">
      <div class="n">1</div>
      <div class="l">Laptop 1</div>
    </div>
    <div class="sw off" id="sw2" onclick="setInput(2)">
      <div class="n">2</div>
      <div class="l">Laptop 2</div>
    </div>
  </div>
  <div class="monitors">
    <div class="mon">
      <span class="mon-name">U3821DW</span>
      <div class="mon-right">
        <span class="mon-input na" id="inpA">--</span>
        <span class="mon-dot fail" id="dotA"></span>
      </div>
    </div>
    <div class="mon">
      <span class="mon-name">U2419H</span>
      <div class="mon-right">
        <span class="mon-input na" id="inpB">--</span>
        <span class="mon-dot fail" id="dotB"></span>
      </div>
    </div>
  </div>
  <div class="tags">
    <span class="tag t-asleep" id="sleepTag">--</span>
    <span class="tag t-off" id="linkTag">--</span>
    <span class="tag t-off" id="pcTag">--</span>
    <span class="tag t-off" id="kbTag">--</span>
  </div>
  <div class="hk-row">
    <span class="hk-label">Hotkey</span>
    <span class="hk-val" id="hotkeyDisplay">--</span>
  </div>
  <button class="btn" id="switchBtn" onclick="doSwitch()">Switch input</button>
  <div class="btn-row" style="margin-top:6px">
    <button class="btn btn-secondary" id="wakeBtn" onclick="doWake()">Wake current PC</button>
  </div>
  <details class="settings">
    <summary>Hotkey settings</summary>
    <div class="hk-form">
      <p>Click the field below, then press any key combination to set a new hotkey. Modifier-only combos are not allowed.</p>
      <div id="hkInput" class="hk-input" tabindex="0">Click here, then press a key…</div>
      <button class="btn hk-save" id="saveHkBtn" onclick="saveHotkey()" disabled>Save hotkey</button>
    </div>
  </details>
  <details class="settings">
    <summary>MQTT settings</summary>
    <div class="hk-form">
      <p>Leave host blank to disable. Changes apply immediately, no reboot needed.</p>
      <div class="field"><label>Host</label><input id="mqHost" type="text" placeholder="10.0.0.10 or blank"></div>
      <div class="field"><label>Port</label><input id="mqPort" type="number" min="1" max="65535" placeholder="1883"></div>
      <div class="field"><label>User</label><input id="mqUser" type="text" placeholder="(optional)"></div>
      <div class="field"><label>Password</label><input id="mqPass" type="password" placeholder="(optional)"></div>
      <button class="btn hk-save" id="saveMqBtn" onclick="saveMqtt()">Save MQTT</button>
      <div class="status-msg" id="mqStatus"></div>
    </div>
  </details>
  <details class="settings">
    <summary>WiFi</summary>
    <div class="hk-form">
      <p>Forget the stored WiFi network and reboot into the setup portal. The device will appear as the open AP <code>KVM-Switcher-Setup</code>; connect to it from your phone to enter new credentials.</p>
      <button class="btn btn-danger hk-save" id="forgetWifiBtn" onclick="forgetWifi()">Forget WiFi &amp; reboot</button>
    </div>
  </details>
  <div class="wifi-row">
    <span class="wifi-ip" id="wifiIp">--</span>
    <span id="wifiRssi">--</span>
  </div>
  <div class="foot"><span id="uptime"></span> &nbsp;·&nbsp; <a href="/update">Firmware update</a></div>
</div>
<script>
// ── HID keycode tables ───────────────────────────────────────────────────────
var CODE_TO_HID = {
  'KeyA':4,'KeyB':5,'KeyC':6,'KeyD':7,'KeyE':8,'KeyF':9,
  'KeyG':10,'KeyH':11,'KeyI':12,'KeyJ':13,'KeyK':14,'KeyL':15,
  'KeyM':16,'KeyN':17,'KeyO':18,'KeyP':19,'KeyQ':20,'KeyR':21,
  'KeyS':22,'KeyT':23,'KeyU':24,'KeyV':25,'KeyW':26,'KeyX':27,
  'KeyY':28,'KeyZ':29,
  'Digit1':30,'Digit2':31,'Digit3':32,'Digit4':33,'Digit5':34,
  'Digit6':35,'Digit7':36,'Digit8':37,'Digit9':38,'Digit0':39,
  'Enter':40,'Escape':41,'Backspace':42,'Tab':43,'Space':44,
  'Minus':45,'Equal':46,'BracketLeft':47,'BracketRight':48,
  'Backslash':49,'Semicolon':51,'Quote':52,'Backquote':53,
  'Comma':54,'Period':55,'Slash':56,'CapsLock':57,
  'F1':58,'F2':59,'F3':60,'F4':61,'F5':62,'F6':63,
  'F7':64,'F8':65,'F9':66,'F10':67,'F11':68,'F12':69,
  'PrintScreen':70,'ScrollLock':71,'Pause':72,
  'Insert':73,'Home':74,'PageUp':75,
  'Delete':76,'End':77,'PageDown':78,
  'ArrowRight':79,'ArrowLeft':80,'ArrowDown':81,'ArrowUp':82,
  'NumLock':83,'NumpadDivide':84,'NumpadMultiply':85,
  'NumpadSubtract':86,'NumpadAdd':87,'NumpadEnter':88,
  'IntlBackslash':100
};
var HID_KEY_NAMES = {
  40:'Enter',41:'Esc',42:'Backspace',43:'Tab',44:'Space',
  45:'-',46:'=',47:'[',48:']',49:'\\',51:';',52:"'",53:'`',
  54:',',55:'.',56:'/',57:'Caps Lock',
  70:'Print Screen',71:'Scroll Lock',72:'Pause',
  73:'Insert',74:'Home',75:'Page Up',
  76:'Delete',77:'End',78:'Page Down',
  79:'→',80:'←',81:'↓',82:'↑',
  83:'Num Lock',84:'Num /',85:'Num *',86:'Num -',87:'Num +',88:'Num Enter',
  100:'\\|'
};
var MOD_KEYS = ['ControlLeft','ControlRight','ShiftLeft','ShiftRight',
                'AltLeft','AltRight','MetaLeft','MetaRight'];
var KEY_TO_HID = {
  '`':53,'~':53,'-':45,'_':45,'=':46,'+':46,
  '[':47,'{':47,']':48,'}':48,'\\':49,'|':49,
  ';':51,':':51,"'":52,'"':52,',':54,'<':54,
  '.':55,'>':55,'/':56,'?':56,'#':49,
  'Enter':40,'Escape':41,'Backspace':42,'Tab':43,' ':44,
  'CapsLock':57,'PrintScreen':70,'ScrollLock':71,'Pause':72,
  'Insert':73,'Home':74,'PageUp':75,'Delete':76,'End':77,
  'PageDown':78,'ArrowRight':79,'ArrowLeft':80,'ArrowDown':81,'ArrowUp':82,
  'NumLock':83
};

function hidKeyName(k) {
  if (HID_KEY_NAMES[k]) return HID_KEY_NAMES[k];
  if (k >= 4  && k <= 29) return String.fromCharCode(65 + k - 4);   // A–Z
  if (k >= 30 && k <= 38) return String.fromCharCode(49 + k - 30);  // 1–9
  if (k === 39) return '0';
  if (k >= 58 && k <= 69) return 'F' + (k - 58 + 1);               // F1–F12
  return '0x' + k.toString(16).toUpperCase();
}

function hotkeyLabel(key, mod) {
  var parts = [];
  if (mod & 0x08 || mod & 0x80) parts.push('Win');
  if (mod & 0x01 || mod & 0x10) parts.push('Ctrl');
  if (mod & 0x02 || mod & 0x20) parts.push('Shift');
  if (mod & 0x04 || mod & 0x40) parts.push('Alt');
  parts.push(hidKeyName(key));
  return parts.join('+');
}

// ── Key capture ───────────────────────────────────────────────────────────────
var pendingKey = 0, pendingMod = 0;
var capturing = false;
var hkInput = document.getElementById('hkInput');

hkInput.addEventListener('focus', function() {
  this.textContent = '';
  this.classList.add('capturing');
  capturing = true;
  pendingKey = 0;
  document.getElementById('saveHkBtn').disabled = true;
});
hkInput.addEventListener('blur', function() {
  this.classList.remove('capturing');
  capturing = false;
  if (!pendingKey) this.textContent = 'Click here, then press a key\u2026';
});
document.addEventListener('keydown', function(e) {
  if (!capturing) return;
  e.preventDefault();
  e.stopPropagation();
  if (MOD_KEYS.indexOf(e.code) >= 0) return;
  var hid = CODE_TO_HID[e.code] || KEY_TO_HID[e.key];
  if (!hid) {
    hkInput.textContent = 'Unknown: ' + (e.code || '?') + ' / ' + (e.key || '?');
    return;
  }
  var mod = 0;
  if (e.ctrlKey)  mod |= 0x01;
  if (e.shiftKey) mod |= 0x02;
  if (e.altKey)   mod |= 0x04;
  if (e.metaKey)  mod |= 0x08;
  pendingKey = hid;
  pendingMod = mod;
  hkInput.textContent = hotkeyLabel(pendingKey, pendingMod);
  document.getElementById('saveHkBtn').disabled = false;
  hkInput.blur();
}, true);

function saveHotkey() {
  if (!pendingKey) return;
  var btn = document.getElementById('saveHkBtn');
  btn.disabled = true;
  btn.textContent = 'Saving\u2026';
  fetch('/api/hotkey/set?key=' + pendingKey + '&mod=' + pendingMod)
    .then(function(r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return r.json();
    })
    .then(function(d) {
      if (d.ok) {
        document.getElementById('hotkeyDisplay').textContent = hotkeyLabel(pendingKey, pendingMod);
        btn.textContent = 'Saved!';
        setTimeout(function() { btn.textContent = 'Save hotkey'; }, 1500);
      } else {
        btn.textContent = d.error || 'Error';
        btn.disabled = false;
      }
    })
    .catch(function(e) {
      console.error('saveHotkey failed:', e);
      btn.textContent = e.message || 'Error';
      btn.disabled = false;
    });
}

// ── Status polling ────────────────────────────────────────────────────────────
function doSwitch() {
  var b = document.getElementById('switchBtn');
  b.disabled = true; b.textContent = 'Switching…';
  fetch('/api/switch').then(function(r) { return r.json(); })
    .then(function() { setTimeout(poll, 600); })
    .catch(function() {})
    .finally(function() { b.disabled = false; b.textContent = 'Switch input'; });
}
function setInput(n) {
  fetch('/api/input/' + n).then(function(r) { return r.json(); })
    .then(function() { setTimeout(poll, 600); }).catch(function() {});
}
function doWake() {
  var b = document.getElementById('wakeBtn');
  var prev = b.textContent;
  b.disabled = true; b.textContent = 'Waking…';
  fetch('/api/wake', { method: 'POST' })
    .then(function(r) { return r.json(); })
    .catch(function() {})
    .finally(function() {
      setTimeout(function() {
        b.disabled = false; b.textContent = prev;
        poll();
      }, 1500);
    });
}
function poll() {
  fetch('/api/status').then(function(r) { return r.json(); }).then(function(d) {
    var i = d.input;
    document.getElementById('sw1').className = 'sw ' + (i === 1 ? 's1' : 'off');
    document.getElementById('sw2').className = 'sw ' + (i === 2 ? 's2' : 'off');
    var dot = document.getElementById('dot');
    dot.className = 'dot ' + (d.monitors_awake ? (i === 1 ? 'i1' : 'i2') : 'off');
    var inpA = document.getElementById('inpA');
    inpA.textContent = d.input_name_a || '--';
    inpA.className = 'mon-input ' + (d.monitors_awake ? (i === 1 ? 'i1' : 'i2') : 'na');
    var inpB = document.getElementById('inpB');
    inpB.textContent = d.input_name_b || '--';
    inpB.className = 'mon-input ' + (d.monitors_awake ? (i === 1 ? 'i1' : 'i2') : 'na');
    document.getElementById('dotA').className = 'mon-dot ' + (d.ddc_a_ok ? 'ok' : 'fail');
    document.getElementById('dotB').className = 'mon-dot ' + (d.ddc_b_ok ? 'ok' : 'fail');
    var st = document.getElementById('sleepTag');
    st.textContent = d.monitors_awake ? 'Monitors awake' : 'Monitors asleep';
    st.className = 'tag ' + (d.monitors_awake ? 't-awake' : 't-asleep');
    var kb = document.getElementById('kbTag');
    if (!d.kb_connected) {
      kb.textContent = 'Keyboard ?'; kb.className = 'tag t-down';
    } else if (d.kb_age_ms == null) {
      kb.textContent = 'Keyboard idle'; kb.className = 'tag t-off';
    } else if (d.kb_age_ms < 2000) {
      kb.textContent = 'Keyboard active'; kb.className = 'tag t-awake';
    } else if (d.kb_age_ms < 60000) {
      kb.textContent = 'Keyboard ' + Math.round(d.kb_age_ms/1000) + 's';
      kb.className = 'tag t-off';
    } else {
      kb.textContent = 'Keyboard idle'; kb.className = 'tag t-off';
    }
    var lk = document.getElementById('linkTag');
    if (d.hid_link_up) {
      lk.textContent = 'Link OK';   lk.className = 'tag t-awake';
    } else {
      lk.textContent = 'Link down'; lk.className = 'tag t-down';
    }
    var pc = document.getElementById('pcTag');
    if (!d.hid_link_up) {
      pc.textContent = 'PC ?';     pc.className = 'tag t-off';
    } else if (!d.usb_mounted) {
      pc.textContent = 'PC off';    pc.className = 'tag t-off';
    } else if (d.usb_suspended) {
      pc.textContent = 'PC asleep'; pc.className = 'tag t-asleep';
    } else {
      pc.textContent = 'PC awake';  pc.className = 'tag t-awake';
    }
    if (d.uptime) {
      var m = Math.floor(d.uptime / 60);
      var h = Math.floor(m / 60);
      document.getElementById('uptime').textContent = h + 'h ' + (m % 60) + 'm';
    }
    if (d.ip) document.getElementById('wifiIp').textContent = d.ip;
    if (d.wifi_rssi) {
      var r = d.wifi_rssi;
      var cls = r >= -60 ? 'rssi-good' : r >= -70 ? 'rssi-ok' : 'rssi-weak';
      var bar = r >= -60 ? '▂▄▆█' : r >= -70 ? '▂▄▆' : r >= -80 ? '▂▄' : '▂';
      var el = document.getElementById('wifiRssi');
      el.textContent = bar + ' ' + r + ' dBm';
      el.className = cls;
    }
  }).catch(function() {});
}
function loadHotkey() {
  fetch('/api/hotkey').then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('hotkeyDisplay').textContent = d.label || '--';
  }).catch(function() {});
}

// ── MQTT settings ────────────────────────────────────────────────────────────
function loadMqtt() {
  fetch('/api/mqtt').then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('mqHost').value = d.host || '';
    document.getElementById('mqPort').value = d.port || 1883;
    document.getElementById('mqUser').value = d.user || '';
    document.getElementById('mqPass').value = d.pass || '';
  }).catch(function() {});
}
function saveMqtt() {
  var btn = document.getElementById('saveMqBtn');
  var msg = document.getElementById('mqStatus');
  btn.disabled = true; btn.textContent = 'Saving…';
  msg.textContent = ''; msg.className = 'status-msg';
  var body = new URLSearchParams();
  body.set('host', document.getElementById('mqHost').value);
  body.set('port', document.getElementById('mqPort').value || '1883');
  body.set('user', document.getElementById('mqUser').value);
  body.set('pass', document.getElementById('mqPass').value);
  fetch('/api/mqtt/set', { method: 'POST', body: body })
    .then(function(r) { return r.json(); })
    .then(function(d) {
      if (d.ok) {
        msg.textContent = 'Saved'; msg.className = 'status-msg ok';
      } else {
        msg.textContent = d.error || 'Error'; msg.className = 'status-msg err';
      }
    })
    .catch(function(e) {
      msg.textContent = e.message || 'Error'; msg.className = 'status-msg err';
    })
    .finally(function() {
      btn.disabled = false; btn.textContent = 'Save MQTT';
    });
}
function forgetWifi() {
  if (!confirm('Forget the stored WiFi network and reboot into the setup portal?')) return;
  var btn = document.getElementById('forgetWifiBtn');
  btn.disabled = true; btn.textContent = 'Rebooting…';
  // Fire and forget — the device reboots before responding.
  fetch('/api/wifi/reset', { method: 'POST' }).catch(function() {});
}

poll();
loadHotkey();
loadMqtt();
setInterval(poll, 3000);
</script>
</body>
</html>
)rawliteral";
