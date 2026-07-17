#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>

#include "config.h"

// NodeMCU ESP8266 I2C:
//   D1 = GPIO5 = SCL
//   D2 = GPIO4 = SDA
static constexpr int PIN_SDA = 4;  // D2
static constexpr int PIN_SCL = 5;  // D1

static constexpr uint8_t AHT10_ADDR = 0x38;
static constexpr uint32_t READ_INTERVAL_MS = 2000;
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

static char g_hostname[24];
static ESP8266WebServer g_server(80);

static float g_temperatureC = NAN;
static float g_humidityRh = NAN;
static bool g_readingOk = false;
static uint32_t g_lastReadMs = 0;

static void buildHostname() {
  snprintf(g_hostname, sizeof(g_hostname), "th_sens_%u", ESP.getChipId());
}

static bool aht10Begin() {
  Wire.beginTransmission(AHT10_ADDR);
  Wire.write(0xBA);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  delay(20);

  Wire.beginTransmission(AHT10_ADDR);
  Wire.write(0xE1);
  Wire.write(0x08);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  delay(10);
  return true;
}

static bool aht10Read(float &temperatureC, float &humidityRh) {
  Wire.beginTransmission(AHT10_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(80);

  if (Wire.requestFrom(static_cast<uint8_t>(AHT10_ADDR), static_cast<uint8_t>(6)) != 6) {
    return false;
  }

  uint8_t data[6];
  for (uint8_t i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }

  if (data[0] & 0x80) {
    return false;
  }

  const uint32_t rawHumidity =
      (static_cast<uint32_t>(data[1]) << 12) |
      (static_cast<uint32_t>(data[2]) << 4) |
      (static_cast<uint32_t>(data[3]) >> 4);

  const uint32_t rawTemperature =
      ((static_cast<uint32_t>(data[3]) & 0x0F) << 16) |
      (static_cast<uint32_t>(data[4]) << 8) |
      static_cast<uint32_t>(data[5]);

  humidityRh = (rawHumidity * 100.0f) / 1048576.0f;
  temperatureC = (rawTemperature * 200.0f) / 1048576.0f - 50.0f;
  return true;
}

static void handleRoot() {
  String html;
  html.reserve(1800);
  html += F(
      "<!DOCTYPE html><html lang=\"ru\"><head>"
      "<meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>");
  html += g_hostname;
  html += F(
      "</title>"
      "<style>"
      "*{box-sizing:border-box}"
      "body{margin:0;min-height:100vh;font-family:Segoe UI,system-ui,sans-serif;"
      "background:linear-gradient(160deg,#0f2744,#163a5f 45%,#1d4d6e);"
      "color:#e8f1f8;display:flex;align-items:center;justify-content:center;padding:24px}"
      "main{width:min(420px,100%)}"
      "h1{margin:0 0 4px;font-size:1.15rem;font-weight:600;letter-spacing:.02em}"
      ".sub{margin:0 0 28px;opacity:.65;font-size:.85rem}"
      ".grid{display:grid;gap:14px}"
      ".metric{padding:22px 20px;background:rgba(255,255,255,.08);"
      "border:1px solid rgba(255,255,255,.12);border-radius:14px}"
      ".label{font-size:.8rem;opacity:.7;text-transform:uppercase;letter-spacing:.08em}"
      ".value{margin-top:8px;font-size:2.6rem;font-weight:600;line-height:1}"
      ".unit{font-size:1.1rem;opacity:.7;margin-left:4px}"
      ".status{margin-top:18px;font-size:.8rem;opacity:.55}"
      "</style></head><body><main>"
      "<h1>");
  html += g_hostname;
  html += F(
      "</h1>"
      "<p class=\"sub\">Температура и влажность</p>"
      "<div class=\"grid\">"
      "<div class=\"metric\"><div class=\"label\">Температура</div>"
      "<div class=\"value\" id=\"t\">—<span class=\"unit\">°C</span></div></div>"
      "<div class=\"metric\"><div class=\"label\">Влажность</div>"
      "<div class=\"value\" id=\"h\">—<span class=\"unit\">%</span></div></div>"
      "</div>"
      "<p class=\"status\" id=\"st\">загрузка…</p>"
      "<script>"
      "async function tick(){"
      "try{"
      "const r=await fetch('/api');"
      "const j=await r.json();"
      "if(j.ok){"
      "document.getElementById('t').innerHTML=j.t.toFixed(1)+'<span class=\"unit\">°C</span>';"
      "document.getElementById('h').innerHTML=j.rh.toFixed(1)+'<span class=\"unit\">%</span>';"
      "document.getElementById('st').textContent='обновлено';"
      "}else{"
      "document.getElementById('st').textContent='ошибка чтения датчика';"
      "}"
      "}catch(e){document.getElementById('st').textContent='нет связи';}"
      "}"
      "tick();setInterval(tick,2000);"
      "</script></main></body></html>");

  g_server.send(200, "text/html; charset=utf-8", html);
}

static void handleApi() {
  char buf[160];
  if (g_readingOk) {
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"hostname\":\"%s\",\"t\":%.2f,\"rh\":%.2f}",
             g_hostname, g_temperatureC, g_humidityRh);
  } else {
    snprintf(buf, sizeof(buf),
             "{\"ok\":false,\"hostname\":\"%s\"}",
             g_hostname);
  }
  g_server.send(200, "application/json", buf);
}

static void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(g_hostname);

  Serial.printf("Hostname: %s\n", g_hostname);
  Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
  Serial.printf("ChipId: %u\n", ESP.getChipId());

  if (WIFI_SSID[0] == '\0') {
    Serial.println(F("WiFi SSID not set — skip connect"));
    return;
  }

  Serial.printf("Connecting to WiFi \"%s\"...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - started) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK  IP=%s  hostname=%s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.hostname().c_str());
  } else {
    Serial.println(F("WiFi connect failed"));
  }
}

static void setupWeb() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (MDNS.begin(g_hostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local/\n", g_hostname);
  }

  g_server.on("/", HTTP_GET, handleRoot);
  g_server.on("/api", HTTP_GET, handleApi);
  g_server.onNotFound([]() {
    g_server.send(404, "text/plain", "Not found");
  });
  g_server.begin();
  Serial.printf("Web UI: http://%s/\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(500);

  buildHostname();

  Serial.println();
  Serial.println(F("ESP8266 + AHT10"));
  Serial.printf("SDA=GPIO%d (D2), SCL=GPIO%d (D1)\n", PIN_SDA, PIN_SCL);

  setupWifi();

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!aht10Begin()) {
    Serial.println(F("AHT10 not found. Halt."));
    while (true) {
      delay(1000);
    }
  }
  Serial.println(F("AHT10 ready"));

  setupWeb();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    MDNS.update();
    g_server.handleClient();
  }

  const uint32_t now = millis();
  if (now - g_lastReadMs < READ_INTERVAL_MS) {
    return;
  }
  g_lastReadMs = now;

  float temperatureC = NAN;
  float humidityRh = NAN;
  g_readingOk = aht10Read(temperatureC, humidityRh);
  if (!g_readingOk) {
    Serial.println(F("Read failed"));
    return;
  }

  g_temperatureC = temperatureC;
  g_humidityRh = humidityRh;
  Serial.printf("[%s] T=%.2f C  RH=%.2f %%\n", g_hostname, temperatureC, humidityRh);
}
