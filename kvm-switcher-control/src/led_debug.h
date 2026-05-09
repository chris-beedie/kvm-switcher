#pragma once
#include "config.h"

// Macro stringification — lets us inject config integer values into the PROGMEM string
// at compile time by breaking out of the raw literal at the right spots.
#define _TOSTR(x) #x
#define TOSTR(x)  _TOSTR(x)

const char LED_DEBUG_HTML[] PROGMEM =
// ---- head + styles ----
R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LED Colour Tuner</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#111;color:#ccc;min-height:100vh;display:flex;justify-content:center;align-items:center}
  .card{background:#1a1a1f;border:1px solid #2a2a30;border-radius:12px;padding:1.5rem;max-width:420px;width:92%}
  h1{font-size:14px;font-weight:500;color:#888;letter-spacing:.06em;text-transform:uppercase;margin-bottom:1.5rem}
  .preview{width:60px;height:60px;border-radius:50%;margin:0 auto 1.5rem;border:2px solid #333;transition:background .05s}
  .slider-row{display:flex;align-items:center;gap:12px;margin-bottom:12px}
  .slider-row label{width:16px;font-size:14px;font-weight:500;text-align:center}
  .slider-row label.lr{color:#E24B4A}
  .slider-row label.lg{color:#1D9E75}
  .slider-row label.lb{color:#60a5fa}
  .slider-row input[type=range]{flex:1;height:6px;-webkit-appearance:none;appearance:none;background:#252530;border-radius:3px;outline:none}
  .slider-row input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;cursor:pointer}
  .slider-row input.sr::-webkit-slider-thumb{background:#E24B4A}
  .slider-row input.sg::-webkit-slider-thumb{background:#1D9E75}
  .slider-row input.sb::-webkit-slider-thumb{background:#60a5fa}
  .slider-row .val{width:36px;font-size:13px;text-align:right;font-variant-numeric:tabular-nums;color:#888}
  .hex{text-align:center;font-family:monospace;font-size:15px;color:#aaa;margin:1rem 0;letter-spacing:.05em}
  .presets{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:1.25rem}
  .preset{padding:6px 12px;font-size:11px;background:#252530;border:1px solid #3a3a44;border-radius:6px;color:#ccc;cursor:pointer;letter-spacing:.02em}
  .preset:hover{background:#30303c}
  .code{background:#111115;border-radius:8px;padding:10px 12px;font-family:monospace;font-size:12px;color:#5DCAA5;word-break:break-all;margin-bottom:1rem}
  .foot{text-align:center;font-size:11px}
  .foot a{color:#555;text-decoration:none}
  .foot a:hover{color:#777}
</style>
</head>
<body>
<div class="card">
  <h1>LED colour tuner</h1>
  <div class="preview" id="preview"></div>
  <div class="slider-row">
    <label class="lr">R</label>
    <input type="range" class="sr" min="0" max="255" value=")rawliteral" TOSTR(COL_TEAL_R) R"rawliteral(" id="r" oninput="update()">
    <span class="val" id="rv">)rawliteral" TOSTR(COL_TEAL_R) R"rawliteral(</span>
  </div>
  <div class="slider-row">
    <label class="lg">G</label>
    <input type="range" class="sg" min="0" max="255" value=")rawliteral" TOSTR(COL_TEAL_G) R"rawliteral(" id="g" oninput="update()">
    <span class="val" id="gv">)rawliteral" TOSTR(COL_TEAL_G) R"rawliteral(</span>
  </div>
  <div class="slider-row">
    <label class="lb">B</label>
    <input type="range" class="sb" min="0" max="255" value=")rawliteral" TOSTR(COL_TEAL_B) R"rawliteral(" id="b" oninput="update()">
    <span class="val" id="bv">)rawliteral" TOSTR(COL_TEAL_B) R"rawliteral(</span>
  </div>
  <div class="hex" id="hex"></div>
  <div class="presets">
)rawliteral"
// ---- preset buttons — values come from config.h ----
"<div class=\"preset\" onclick=\"set(" TOSTR(COL_TEAL_R)   "," TOSTR(COL_TEAL_G)   "," TOSTR(COL_TEAL_B)   ")\">Teal (input 1)</div>\n"
"<div class=\"preset\" onclick=\"set(" TOSTR(COL_PURPLE_R) "," TOSTR(COL_PURPLE_G) "," TOSTR(COL_PURPLE_B) ")\">Purple (input 2)</div>\n"
"<div class=\"preset\" onclick=\"set(" TOSTR(COL_AMBER_R)  "," TOSTR(COL_AMBER_G)  "," TOSTR(COL_AMBER_B)  ")\">Amber (switching)</div>\n"
"<div class=\"preset\" onclick=\"set(" TOSTR(COL_RED_R)    "," TOSTR(COL_RED_G)    "," TOSTR(COL_RED_B)    ")\">Red (error)</div>\n"
"<div class=\"preset\" onclick=\"set(0,0,0)\">Off</div>\n"
// ---- rest of page ----
R"rawliteral(  </div>
  <div class="code" id="code"></div>
  <div class="foot"><a href="/">Back to KVM</a></div>
</div>
<script>
var timer=null;
function update(){
  var r=+document.getElementById('r').value;
  var g=+document.getElementById('g').value;
  var b=+document.getElementById('b').value;
  document.getElementById('rv').textContent=r;
  document.getElementById('gv').textContent=g;
  document.getElementById('bv').textContent=b;
  document.getElementById('preview').style.background='rgb('+r+','+g+','+b+')';
  document.getElementById('preview').style.boxShadow='0 0 20px rgba('+r+','+g+','+b+',0.4)';
  var hr=r.toString(16).padStart(2,'0');
  var hg=g.toString(16).padStart(2,'0');
  var hb=b.toString(16).padStart(2,'0');
  document.getElementById('hex').textContent='#'+hr+hg+hb;
  document.getElementById('code').textContent='strip.Color('+r+', '+g+', '+b+')';
  if(timer) clearTimeout(timer);
  timer=setTimeout(function(){
    fetch('/debug/led?r='+r+'&g='+g+'&b='+b);
  },80);
}
function set(r,g,b){
  document.getElementById('r').value=r;
  document.getElementById('g').value=g;
  document.getElementById('b').value=b;
  update();
}
update();
</script>
</body>
</html>)rawliteral";

#undef _TOSTR
#undef TOSTR
