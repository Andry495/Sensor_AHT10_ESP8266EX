#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

#include <ESPAsyncWebServer.h>
#include <cctype>
#include <cmath>
#include <cstring>
#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

namespace esphome {
namespace custom_ui {

class CustomUI : public AsyncWebHandler, public Component {
 public:
  explicit CustomUI(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override {
    this->base_->init();
    // Регистрируемся раньше web_server, чтобы перехватить "/"
    this->base_->add_handler(this);
  }

  // Выше WebServer (WIFI - 1), чтобы handler встал первым в списке AsyncWebServer
  float get_setup_priority() const override { return setup_priority::WIFI - 0.5f; }

  bool canHandle(AsyncWebServerRequest *request) const override {
    const String &url = request->url();
    return url == "/" || url == "/ui" || url == "/ui/" || url == "/ui/api" || url == "/esp" || url == "/esp/" ||
           url == "/prom" || url == "/prom/";
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    const String &url = request->url();
    if (url == "/ui/api") {
      this->handle_api_(request);
      return;
    }
    if (url == "/prom" || url == "/prom/") {
      this->handle_prom_(request);
      return;
    }
    if (url == "/esp" || url == "/esp/") {
      this->handle_esp_(request);
      return;
    }
    this->handle_ui_(request);
  }

 protected:
  static bool finite_state_(sensor::Sensor *s, float &out) {
    if (s == nullptr || !s->has_state())
      return false;
    out = s->state;
    return std::isfinite(out);
  }

  void append_metric_(String &out, const char *name, float value, int decimals) {
    char line[96];
    snprintf(line, sizeof(line), "%s %.*f\n", name, decimals, value);
    out += line;
  }

  void append_metric_i_(String &out, const char *name, long value) {
    char line[96];
    snprintf(line, sizeof(line), "%s %ld\n", name, value);
    out += line;
  }

  void append_metric_u_(String &out, const char *name, unsigned long value) {
    char line[96];
    snprintf(line, sizeof(line), "%s %lu\n", name, value);
    out += line;
  }

  // Prometheus metric names: [a-zA-Z_:][a-zA-Z0-9_:]*
  static void sanitize_metric_suffix_(const char *src, char *dst, size_t dst_len) {
    if (dst_len == 0)
      return;
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_len; i++) {
      char c = src[i];
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
        dst[j++] = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      } else if (c == ' ' || c == '-' || c == '.') {
        if (j == 0 || dst[j - 1] != '_')
          dst[j++] = '_';
      }
    }
    while (j > 0 && dst[j - 1] == '_')
      j--;
    if (j == 0) {
      dst[0] = 'x';
      j = 1;
    }
    dst[j] = '\0';
  }

  void handle_prom_(AsyncWebServerRequest *request) {
    float temperature = NAN;
    float humidity = NAN;
    float wifi_rssi = NAN;

    String body;
    body.reserve(1400);
    body += F("#Prometey TempDataEthernet\n");

    // Все сенсоры контроллера (Temp/Lucht/WiFi + любые будущие)
    for (auto *s : App.get_sensors()) {
      if (s == nullptr || !s->has_state() || !std::isfinite(s->state))
        continue;
      const auto &name = s->get_name();
      if (name.find("Temp") != std::string::npos)
        temperature = s->state;
      else if (name.find("Lucht") != std::string::npos || name.find("Humidity") != std::string::npos)
        humidity = s->state;
      else if (name.find("WiFi") != std::string::npos || name.find("wifi") != std::string::npos)
        wifi_rssi = s->state;

      char suffix[48];
      this->sanitize_metric_suffix_(name.c_str(), suffix, sizeof(suffix));
      char metric[64];
      snprintf(metric, sizeof(metric), "tde_sensor_%s", suffix);
      this->append_metric_(body, metric, s->state, 2);
    }

    // Бинарные сенсоры (Status и др.)
    int status_on = 0;
    for (auto *b : App.get_binary_sensors()) {
      if (b == nullptr || !b->has_state())
        continue;
      const auto &name = b->get_name();
      if (name.find("Status") != std::string::npos)
        status_on = b->state ? 1 : 0;

      char suffix[48];
      this->sanitize_metric_suffix_(name.c_str(), suffix, sizeof(suffix));
      char metric[64];
      snprintf(metric, sizeof(metric), "tde_binary_%s", suffix);
      this->append_metric_i_(body, metric, b->state ? 1 : 0);
    }

    // Совместимость со scrapers (алиасы tde_* как на temp_*)
    if (std::isfinite(temperature))
      this->append_metric_(body, "tde_temperature", temperature, 2);
    if (std::isfinite(humidity))
      this->append_metric_(body, "tde_humidity", humidity, 2);

#ifdef USE_NUMBER
    for (auto *n : App.get_numbers()) {
      if (n == nullptr || !n->has_state() || !std::isfinite(n->state))
        continue;
      const auto &name = n->get_name();
      if (name.find("Temp Offset") != std::string::npos)
        this->append_metric_(body, "tde_temp_offset", n->state, 2);
      else if (name.find("Humidity Offset") != std::string::npos)
        this->append_metric_(body, "tde_humidity_offset", n->state, 2);
    }
#endif

    // Системные / Wi‑Fi метрики ESP8266
    if (std::isfinite(wifi_rssi))
      this->append_metric_(body, "tde_wifi_rssi", wifi_rssi, 0);
    else
      this->append_metric_i_(body, "tde_wifi_rssi", WiFi.RSSI());

    this->append_metric_i_(body, "tde_wifi_connected", WiFi.status() == WL_CONNECTED ? 1 : 0);
    this->append_metric_i_(body, "tde_wifi_status", static_cast<long>(WiFi.status()));
    this->append_metric_i_(body, "tde_wifi_mode", static_cast<long>(WiFi.getMode()));
    this->append_metric_i_(body, "tde_wifi_channel", static_cast<long>(WiFi.channel()));
    this->append_metric_i_(body, "tde_status", status_on);
    this->append_metric_i_(body, "tde_uptime_seconds", static_cast<long>(millis() / 1000UL));
    this->append_metric_u_(body, "tde_loop_count", static_cast<unsigned long>(ESP.getCycleCount()));
    this->append_metric_i_(body, "tde_heap_free_bytes", static_cast<long>(ESP.getFreeHeap()));
#ifdef USE_ESP8266
    this->append_metric_i_(body, "tde_heap_max_block_bytes", static_cast<long>(ESP.getMaxFreeBlockSize()));
    this->append_metric_i_(body, "tde_heap_fragmentation_percent", static_cast<long>(ESP.getHeapFragmentation()));
    this->append_metric_i_(body, "tde_flash_chip_size_bytes", static_cast<long>(ESP.getFlashChipSize()));
    this->append_metric_i_(body, "tde_flash_chip_speed_hz", static_cast<long>(ESP.getFlashChipSpeed()));
    this->append_metric_i_(body, "tde_flash_chip_id", static_cast<long>(ESP.getFlashChipId()));
    this->append_metric_i_(body, "tde_sketch_size_bytes", static_cast<long>(ESP.getSketchSize()));
    this->append_metric_i_(body, "tde_free_sketch_space_bytes", static_cast<long>(ESP.getFreeSketchSpace()));
    this->append_metric_i_(body, "tde_cpu_freq_mhz", static_cast<long>(ESP.getCpuFreqMHz()));
    this->append_metric_i_(body, "tde_chip_id", static_cast<long>(ESP.getChipId()));
    this->append_metric_i_(body, "tde_boot_version", static_cast<long>(ESP.getBootVersion()));
    this->append_metric_i_(body, "tde_boot_mode", static_cast<long>(ESP.getBootMode()));
    auto *rst = ESP.getResetInfoPtr();
    if (rst != nullptr)
      this->append_metric_i_(body, "tde_reset_reason", static_cast<long>(rst->reason));
#endif

    IPAddress ip = WiFi.localIP();
    this->append_metric_i_(body, "tde_ip_o1", ip[0]);
    this->append_metric_i_(body, "tde_ip_o2", ip[1]);
    this->append_metric_i_(body, "tde_ip_o3", ip[2]);
    this->append_metric_i_(body, "tde_ip_o4", ip[3]);

    IPAddress gw = WiFi.gatewayIP();
    this->append_metric_i_(body, "tde_gw_o1", gw[0]);
    this->append_metric_i_(body, "tde_gw_o2", gw[1]);
    this->append_metric_i_(body, "tde_gw_o3", gw[2]);
    this->append_metric_i_(body, "tde_gw_o4", gw[3]);

    uint8_t bssid[6] = {0};
    if (WiFi.BSSID() != nullptr)
      memcpy(bssid, WiFi.BSSID(), 6);
    this->append_metric_i_(body, "tde_bssid_o1", bssid[0]);
    this->append_metric_i_(body, "tde_bssid_o2", bssid[1]);
    this->append_metric_i_(body, "tde_bssid_o3", bssid[2]);
    this->append_metric_i_(body, "tde_bssid_o4", bssid[3]);
    this->append_metric_i_(body, "tde_bssid_o5", bssid[4]);
    this->append_metric_i_(body, "tde_bssid_o6", bssid[5]);

    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain; charset=utf-8", body);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  }

  void handle_api_(AsyncWebServerRequest *request) {
    float t = NAN;
    float h = NAN;
    float t_off = NAN;
    float h_off = NAN;
    for (auto *s : App.get_sensors()) {
      if (s == nullptr || !s->has_state())
        continue;
      const auto &name = s->get_name();
      if (name.find("Temp") != std::string::npos)
        t = s->state;
      if (name.find("Lucht") != std::string::npos || name.find("Humidity") != std::string::npos)
        h = s->state;
    }
#ifdef USE_NUMBER
    for (auto *n : App.get_numbers()) {
      if (n == nullptr || !n->has_state())
        continue;
      const auto &name = n->get_name();
      if (name.find("Temp Offset") != std::string::npos)
        t_off = n->state;
      else if (name.find("Humidity Offset") != std::string::npos)
        h_off = n->state;
    }
#endif

    std::string api_key;
#ifdef USE_API
    if (api::global_api_server != nullptr) {
      auto &ctx = api::global_api_server->get_noise_ctx();
      if (ctx.has_psk()) {
        const auto &psk = ctx.get_psk();
        api_key = base64_encode(psk.data(), psk.size());
      }
    }
#endif

    char buf[640];
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"hostname\":\"%s\",\"ip\":\"%s\",\"ssid\":\"%s\","
             "\"t\":%.2f,\"rh\":%.2f,\"rssi\":%d,\"temp_offset\":%.2f,\"humidity_offset\":%.2f,"
             "\"api_encryption_key\":\"%s\",\"prom\":\"/prom\"}",
             App.get_name().c_str(), WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), t, h, WiFi.RSSI(), t_off,
             h_off, api_key.c_str());
    request->send(200, "application/json", buf);
  }

  void handle_ui_(AsyncWebServerRequest *request) {
    static const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="ru"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TH Sensor</title>
<style>
:root{--bg:#0b1220;--card:#121a2b;--line:#243049;--text:#e8eef8;--muted:#93a0b8;--acc:#3aa0ff}
*{box-sizing:border-box}body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:radial-gradient(1200px 600px at 10% -10%,#1a2b4d,transparent),var(--bg);color:var(--text)}
main{max-width:900px;margin:0 auto;padding:24px}
h1{margin:0;font-size:1.35rem}.sub{color:var(--muted);margin:6px 0 20px;font-size:.92rem}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
@media(max-width:560px){.grid{grid-template-columns:1fr}}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px}
.label{color:var(--muted);font-size:.78rem;text-transform:uppercase;letter-spacing:.06em}
.value{font-size:2.1rem;font-weight:650;margin-top:8px}.unit{font-size:1rem;color:var(--muted);margin-left:4px}
.meta{display:grid;gap:8px;margin-top:12px;font-size:.92rem}
.meta div{display:flex;justify-content:space-between;gap:12px;border-bottom:1px dashed var(--line);padding:6px 0}
.actions{display:flex;flex-wrap:wrap;gap:10px;margin-top:16px}
a,button{border:1px solid var(--line);background:#172238;color:var(--text);border-radius:10px;padding:10px 14px;text-decoration:none;font:inherit;cursor:pointer}
a.primary,button.primary{background:var(--acc);border-color:transparent;color:#041018;font-weight:600}
.calib{display:grid;grid-template-columns:1fr 1fr auto;gap:10px;align-items:end;margin-top:12px}
@media(max-width:560px){.calib{grid-template-columns:1fr}}
.calib label{display:grid;gap:6px;font-size:.78rem;color:var(--muted);text-transform:uppercase;letter-spacing:.06em}
.calib input{width:100%;padding:10px 12px;border-radius:10px;border:1px solid var(--line);background:#0a1020;color:var(--text);font:inherit}
.note{margin-top:14px;color:var(--muted);font-size:.85rem;line-height:1.45}code{color:#c9e4ff}
#logs{margin-top:14px;height:280px;overflow:auto;font:12px/1.45 Consolas,monospace;white-space:pre-wrap;background:#0a1020;border:1px solid var(--line);border-radius:12px;padding:12px}
#logs .e{color:#ff8e8e}#logs .w{color:#ffd166}#logs .i{color:#9ad1ff}#logs .d{color:#b7c3d9}
</style></head><body><main>
<h1 id="title">TH Sensor</h1>
<p class="sub">Расширенный интерфейс · логи в реальном времени</p>
<div class="actions" style="margin-top:0;margin-bottom:16px">
  <a href="/esp">ESPHome / HA</a>
  <a href="/prom">Prometheus /prom</a>
</div>
<div class="grid">
  <div class="card"><div class="label">Температура</div><div class="value" id="t">—<span class="unit">°C</span></div></div>
  <div class="card"><div class="label">Влажность</div><div class="value" id="h">—<span class="unit">%</span></div></div>
</div>
<div class="card meta" style="margin-top:12px">
  <div><span>Hostname</span><strong id="host">—</strong></div>
  <div><span>IP</span><strong id="ip">—</strong></div>
  <div><span>Wi‑Fi</span><strong id="ssid">—</strong></div>
  <div><span>RSSI</span><strong id="rssi">—</strong></div>
</div>
<div class="card" style="margin-top:12px">
  <div class="label">Home Assistant — Encryption key</div>
  <p class="note" style="margin:8px 0 10px">Вставьте этот ключ при добавлении интеграции ESPHome в HA.</p>
  <code id="apikey" style="display:block;word-break:break-all;background:#0a1020;border:1px solid var(--line);border-radius:10px;padding:12px;font-size:.85rem">—</code>
  <div class="actions" style="margin-top:10px">
    <button class="primary" id="copyKey" type="button">Копировать ключ</button>
  </div>
</div>
<div class="card" style="margin-top:12px">
  <div class="label">Калибровка (сохраняется во flash)</div>
  <div class="calib">
    <label>Temp offset °C<input id="toff" type="number" step="0.1" min="-10" max="10"></label>
    <label>Humidity offset %<input id="hoff" type="number" step="0.1" min="-20" max="20"></label>
    <button class="primary" id="saveCal" type="button">Сохранить</button>
  </div>
  <p class="note" style="margin-top:10px">Дефолт: Temp −2.0 °C, Humidity +4.0 % (под эталон ~25°C/42%). Подстройка под ifs-std002 на каждой плате.</p>
</div>
<div class="card" style="margin-top:12px"><div class="label">Лог</div><div id="logs">подключение к /events…</div></div>
<p class="note">AHT10: температура и влажность. Старый UI: <code>/esp</code>, метрики: <code>/prom</code>.</p>
<script>
const logs=document.getElementById('logs');
function strip(s){return s.replace(/\x1b\[[0-9;]*m/g,'').replace(/[\r\n]+$/,'')}
function addLog(msg){
  const m=strip(msg);
  let cls='d';
  if(m.includes('[E]')) cls='e'; else if(m.includes('[W]')) cls='w'; else if(m.includes('[I]')||m.includes('[C]')) cls='i';
  const line=document.createElement('div'); line.className=cls; line.textContent=m;
  if(logs.dataset.ready!=='1'){logs.textContent=''; logs.dataset.ready='1'}
  logs.appendChild(line); while(logs.childNodes.length>200) logs.removeChild(logs.firstChild);
  logs.scrollTop=logs.scrollHeight;
}
try{
  const es=new EventSource('/events');
  es.addEventListener('log', e=>addLog(e.data));
  es.addEventListener('ping', ()=>{ if(logs.dataset.ready!=='1'){ logs.textContent='SSE OK, ждём логи…'; }});
  es.onerror=()=>{ if(logs.dataset.ready!=='1') logs.textContent='нет SSE /events'; };
}catch(e){ logs.textContent='EventSource не поддерживается'; }
async function setNumber(name, value){
  const url='/number/'+encodeURIComponent(name)+'/set?value='+encodeURIComponent(value);
  const r=await fetch(url,{method:'POST'});
  if(!r.ok) throw new Error('HTTP '+r.status+' '+name);
}
document.getElementById('saveCal').onclick=async ()=>{
  const btn=document.getElementById('saveCal');
  const toff=document.getElementById('toff').value;
  const hoff=document.getElementById('hoff').value;
  btn.disabled=true;
  try{
    await setNumber('Temp Offset', toff);
    await setNumber('Humidity Offset', hoff);
    addLog('[I] calibration saved T_off='+toff+' RH_off='+hoff);
    setTimeout(tick, 500);
  }catch(e){ addLog('[E] calibration save failed: '+(e&&e.message?e.message:e)); }
  finally{ btn.disabled=false; }
};
document.getElementById('copyKey').onclick=async ()=>{
  const key=document.getElementById('apikey').textContent||'';
  if(!key || key==='—'){ addLog('[W] encryption key empty'); return; }
  try{
    await navigator.clipboard.writeText(key);
    addLog('[I] encryption key copied');
  }catch(e){
    try{
      const ta=document.createElement('textarea'); ta.value=key; document.body.appendChild(ta);
      ta.select(); document.execCommand('copy'); document.body.removeChild(ta);
      addLog('[I] encryption key copied');
    }catch(e2){ addLog('[E] copy failed'); }
  }
};
async function tick(){
  try{
    const j=await (await fetch('/ui/api')).json();
    document.getElementById('title').textContent=j.hostname||'TH Sensor';
    document.getElementById('host').textContent=j.hostname||'—';
    document.getElementById('ip').textContent=j.ip||'—';
    document.getElementById('ssid').textContent=j.ssid||'—';
    document.getElementById('rssi').textContent=(j.rssi??'—')+' dBm';
    if(j.api_encryption_key) document.getElementById('apikey').textContent=j.api_encryption_key;
    if(j.ok && Number.isFinite(j.t)) document.getElementById('t').innerHTML=j.t.toFixed(1)+'<span class="unit">°C</span>';
    if(j.ok && Number.isFinite(j.rh)) document.getElementById('h').innerHTML=j.rh.toFixed(1)+'<span class="unit">%</span>';
    if(Number.isFinite(j.temp_offset) && document.activeElement!==document.getElementById('toff'))
      document.getElementById('toff').value=j.temp_offset.toFixed(1);
    if(Number.isFinite(j.humidity_offset) && document.activeElement!==document.getElementById('hoff'))
      document.getElementById('hoff').value=j.humidity_offset.toFixed(1);
  }catch(e){}
}
tick(); setInterval(tick,2000);
</script>
</main></body></html>
)HTML";
    request->send_P(200, "text/html; charset=utf-8", PAGE);
  }

  // Тот же индекс ESPHome v2, но с патчем pathname:
  // www.js делает EventSource(pathname + "/events"), поэтому на /esp без патча
  // UI пустой (виден только переключатель Scheme).
  void handle_esp_(AsyncWebServerRequest *request) {
    static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset=UTF-8><link rel=icon href=data:>
<title>ESPHome</title>
</head><body>
<script>history.replaceState(null,"","/");</script>
<esp-app></esp-app>
<script src="https://oi.esphome.io/v2/www.js"></script>
<script>history.replaceState(null,"","/esp");</script>
</body></html>)HTML";
    request->send_P(200, "text/html; charset=utf-8", PAGE);
  }

  web_server_base::WebServerBase *base_;
};

}  // namespace custom_ui
}  // namespace esphome
