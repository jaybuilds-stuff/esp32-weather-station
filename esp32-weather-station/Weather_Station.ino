#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <math.h>

// Wi-Fi credentials. Keep these private.
// Copy secrets.example.h to secrets.h and add your Wi-Fi credentials.
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DHTPIN 4
#define DHTTYPE DHT11
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

const char* timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
const size_t HISTORY_SIZE = 60;
float tempHistory[HISTORY_SIZE];
float humHistory[HISTORY_SIZE];
size_t historyCount = 0, historyNext = 0;
float temperature = NAN, humidity = NAN;
float minTemp = NAN, maxTemp = NAN, sumTemp = 0;
float minHum = NAN, maxHum = NAN, sumHum = 0;
unsigned long sampleCount = 0;
unsigned long lastSensorRead = 0, lastWifiCheck = 0, lastScreenDraw = 0, lastPageChange = 0;
unsigned long lastHistorySample = 0;
unsigned long lastReconnectAttempt = 0;
uint8_t currentPage = 0;
bool autoPages = true, screenEnabled = true;
float tempWarning = 30.0, humWarning = 70.0;

String numberToJson(float v, int digits = 1) {
  return isnan(v) ? "null" : String(v, digits);
}
String boolToJson(bool value) {
  return value ? "true" : "false";
}
String timeString(const char* format) {
  struct tm t;
  if (!getLocalTime(&t, 5)) return "--";
  char b[32]; strftime(b, sizeof(b), format, &t);
  return String(b);
}
void recordHistory() {
  if (isnan(temperature) || isnan(humidity)) return;
  tempHistory[historyNext] = temperature;
  humHistory[historyNext] = humidity;
  historyNext = (historyNext + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) historyCount++;
}
String historyToJson(const float* values) {
  String s = "[";
  for (size_t i = 0; i < historyCount; i++) {
    size_t index = (historyNext + HISTORY_SIZE - historyCount + i) % HISTORY_SIZE;
    if (i) s += ',';
    s += numberToJson(values[index]);
  }
  s += ']'; return s;
}
String buildStatusJson() {
  String s = "{";
  s += "\"temperature\":" + numberToJson(temperature);
  s += ",\"humidity\":" + numberToJson(humidity, 0);
  s += ",\"minTemp\":" + numberToJson(minTemp);
  s += ",\"maxTemp\":" + numberToJson(maxTemp);
  s += ",\"avgTemp\":" + (sampleCount ? numberToJson(sumTemp / sampleCount) : "null");
  s += ",\"minHum\":" + numberToJson(minHum, 0);
  s += ",\"maxHum\":" + numberToJson(maxHum, 0);
  s += ",\"avgHum\":" + (sampleCount ? numberToJson(sumHum / sampleCount, 0) : "null");
  s += ",\"wifi\":" + boolToJson(WiFi.status() == WL_CONNECTED);
  s += ",\"rssi\":" + (WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) : "null");
  s += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  s += ",\"time\":\"" + timeString("%H:%M:%S") + "\"";
  s += ",\"date\":\"" + timeString("%d.%m.%Y") + "\"";
  s += ",\"uptime\":" + String(millis() / 1000);
  s += ",\"freeHeap\":" + String(ESP.getFreeHeap());
  s += ",\"warning\":" + boolToJson((!isnan(temperature) && temperature >= tempWarning) || (!isnan(humidity) && humidity >= humWarning));
  s += ",\"tempWarning\":" + numberToJson(tempWarning);
  s += ",\"humWarning\":" + numberToJson(humWarning, 0);
  s += ",\"autoPages\":" + boolToJson(autoPages);
  s += ",\"screenEnabled\":" + boolToJson(screenEnabled);
  s += ",\"page\":" + String(currentPage);
  s += ",\"temperatureHistory\":" + historyToJson(tempHistory);
  s += ",\"humidityHistory\":" + historyToJson(humHistory);
  s += "}"; return s;
}

const char DASHBOARD[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Weather Station</title>
  <style>
    :root { font-family: Arial, Helvetica, sans-serif; color: #252525; background: #f7f7f5; }
    * { box-sizing: border-box; }
    body { margin: 0; }
    main { max-width: 900px; margin: 0 auto; padding: 36px 20px 60px; }
    header { display: flex; align-items: baseline; justify-content: space-between; gap: 12px; flex-wrap: wrap; border-bottom: 1px solid #d8d8d4; padding-bottom: 20px; }
    h1 { font-size: 25px; font-weight: 600; margin: 0 0 6px; }
    h2 { font-size: 16px; font-weight: 600; margin: 0 0 16px; }
    p { line-height: 1.5; }
    .secondary { color: #707070; font-size: 13px; }
    .status { font-size: 13px; }
    .metrics { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; margin-top: 22px; }
    section { background: #fff; border: 1px solid #dededb; border-radius: 6px; padding: 22px; margin-top: 16px; }
    .metrics section { margin-top: 0; }
    .metric { font-size: 38px; font-weight: 500; margin: 12px 0; letter-spacing: -1px; }
    .chart { display: block; width: 100%; height: 160px; }
    .controls { display: flex; flex-wrap: wrap; gap: 8px; margin: 12px 0; }
    button { font: inherit; font-size: 13px; padding: 9px 13px; background: #fff; border: 1px solid #bcbcb8; border-radius: 4px; cursor: pointer; }
    button:hover { background: #f0f0ed; }
    button:focus-visible, input:focus-visible { outline: 2px solid #4b6b88; outline-offset: 2px; }
    label { display: inline-flex; align-items: center; gap: 8px; margin: 8px 20px 8px 0; font-size: 14px; }
    input[type=number] { font: inherit; padding: 7px; width: 78px; border: 1px solid #bcbcb8; border-radius: 4px; }
    dl { display: grid; grid-template-columns: 150px 1fr; gap: 10px 16px; margin: 0; font-size: 14px; }
    dt { color: #707070; } dd { margin: 0; overflow-wrap: anywhere; }
    footer { margin-top: 24px; }
    @media (max-width: 550px) { .metrics { grid-template-columns: 1fr; } main { padding: 24px 14px 40px; } dl { grid-template-columns: 110px 1fr; } }
  </style>
</head>
<body>
<main>
  <header>
    <div><h1>Weather Station</h1><div class="secondary">ESP32 / Indoor conditions</div></div>
    <div id="connection" class="status">Connecting...</div>
  </header>

  <div class="metrics">
    <section><div class="secondary">Temperature</div><div class="metric" id="temp">--</div><div class="secondary" id="tempStats">Waiting for readings</div></section>
    <section><div class="secondary">Humidity</div><div class="metric" id="hum">--</div><div class="secondary" id="humStats">Waiting for readings</div></section>
  </div>

  <section><h2>Temperature history</h2><svg class="chart" id="tempChart" viewBox="0 0 700 160" preserveAspectRatio="none" aria-label="Temperature history chart"></svg><p class="secondary">One reading per minute, up to 60 readings. History resets after a restart.</p></section>
  <section><h2>Humidity history</h2><svg class="chart" id="humChart" viewBox="0 0 700 160" preserveAspectRatio="none" aria-label="Humidity history chart"></svg></section>

  <section>
    <h2>Display</h2>
    <div class="controls">
      <button type="button" onclick="changePage(0)">Overview</button>
      <button type="button" onclick="changePage(1)">History</button>
      <button type="button" onclick="changePage(2)">System</button>
      <button type="button" onclick="changePage(3)">Network</button>
    </div>
    <label><input id="autoPages" type="checkbox" onchange="saveSettings()"> Rotate pages automatically</label>
    <label><input id="screenEnabled" type="checkbox" onchange="saveSettings()"> Display enabled</label>
  </section>

  <section>
    <h2>Alert thresholds</h2>
    <label>Temperature <input id="tempWarning" type="number" min="-20" max="80" step="0.5"> °C</label>
    <label>Humidity <input id="humWarning" type="number" min="0" max="100" step="1"> %</label>
    <div class="controls"><button type="button" onclick="saveSettings()">Save settings</button></div>
    <div class="secondary" id="warning">No readings yet</div>
  </section>

  <section>
    <h2>Device information</h2>
    <dl>
      <dt>Local time</dt><dd id="localTime">--</dd>
      <dt>IP address</dt><dd id="ip">--</dd>
      <dt>Wi-Fi signal</dt><dd id="signal">--</dd>
      <dt>Free memory</dt><dd id="memory">--</dd>
      <dt>Uptime</dt><dd id="uptime">--</dd>
    </dl>
  </section>
  <footer class="secondary">Local network only. This page has no password protection. Settings reset when the ESP32 restarts.</footer>
</main>
<script>
  const element = id => document.getElementById(id);
  let settingsLoaded = false;

  function formatReading(value, decimals = 1) {
    return value == null ? '--' : Number(value).toFixed(decimals);
  }

  function drawChart(id, readings) {
    const svg = element(id);
    svg.replaceChildren();
    const values = readings.filter(value => value != null);
    if (values.length < 2) return;

    let minimum = Math.min(...values);
    let maximum = Math.max(...values);
    if (maximum - minimum < 1) { minimum -= 0.5; maximum += 0.5; }

    const ns = 'http://www.w3.org/2000/svg';
    const grid = document.createElementNS(ns, 'path');
    grid.setAttribute('d', 'M10 145 H690 M10 80 H690 M10 15 H690');
    grid.setAttribute('stroke', '#e6e6e3');
    grid.setAttribute('fill', 'none');
    svg.appendChild(grid);

    let path = '';
    readings.forEach((value, index) => {
      if (value == null) return;
      const x = 10 + index * 680 / Math.max(readings.length - 1, 1);
      const y = 145 - (value - minimum) / (maximum - minimum) * 130;
      path += (index > 0 && readings[index - 1] != null ? 'L' : 'M') + x.toFixed(1) + ',' + y.toFixed(1) + ' ';
    });

    const line = document.createElementNS(ns, 'path');
    line.setAttribute('d', path);
    line.setAttribute('stroke', '#4c697e');
    line.setAttribute('stroke-width', '2');
    line.setAttribute('fill', 'none');
    svg.appendChild(line);
  }

  async function refreshData() {
    try {
      const response = await fetch('/api/data', { cache: 'no-store' });
      if (!response.ok) throw new Error('Request failed');
      const data = await response.json();

      element('connection').textContent = data.wifi ? 'Wi-Fi connected' : 'Wi-Fi disconnected';
      element('temp').textContent = formatReading(data.temperature) + ' °C';
      element('hum').textContent = formatReading(data.humidity, 0) + ' %';
      element('tempStats').textContent = 'Min ' + formatReading(data.minTemp) + ' · Max ' + formatReading(data.maxTemp) + ' · Avg ' + formatReading(data.avgTemp);
      element('humStats').textContent = 'Min ' + formatReading(data.minHum, 0) + ' · Max ' + formatReading(data.maxHum, 0) + ' · Avg ' + formatReading(data.avgHum, 0);
      element('warning').textContent = data.warning ? 'A threshold has been reached.' : 'All readings within thresholds.';
      element('localTime').textContent = data.date + ' ' + data.time;
      element('ip').textContent = data.ip;
      element('signal').textContent = data.rssi == null ? '--' : data.rssi + ' dBm';
      element('memory').textContent = data.freeHeap + ' bytes';
      element('uptime').textContent = Math.floor(data.uptime / 3600) + 'h ' + Math.floor(data.uptime % 3600 / 60) + 'm';

      drawChart('tempChart', data.temperatureHistory);
      drawChart('humChart', data.humidityHistory);

      if (!settingsLoaded) {
        element('tempWarning').value = data.tempWarning;
        element('humWarning').value = data.humWarning;
        element('autoPages').checked = data.autoPages;
        element('screenEnabled').checked = data.screenEnabled;
        settingsLoaded = true;
      }
    } catch (error) {
      element('connection').textContent = 'Device unavailable';
    }
  }

  async function changePage(page) {
    await fetch('/api/page?n=' + page, { method: 'POST' });
    refreshData();
  }

  async function saveSettings() {
    const settings = new URLSearchParams({
      temp: element('tempWarning').value,
      hum: element('humWarning').value,
      auto: element('autoPages').checked ? '1' : '0',
      screen: element('screenEnabled').checked ? '1' : '0'
    });
    await fetch('/api/settings?' + settings, { method: 'POST' });
    refreshData();
  }

  refreshData();
  setInterval(refreshData, 2500);
</script>
</body>
</html>
)HTML";
void setupWebServer() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", DASHBOARD); });
  server.on("/api/data", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", buildStatusJson());
  });
  server.on("/api/page", HTTP_POST, []() {
    if (!server.hasArg("n")) { server.send(400, "text/plain", "Missing page"); return; }
    int p = server.arg("n").toInt();
    if (p < 0 || p > 3) { server.send(400, "text/plain", "Invalid page"); return; }
    currentPage = p; lastPageChange = millis();
    server.send(200, "text/plain", "OK");
  });
  server.on("/api/settings", HTTP_POST, []() {
    if (server.hasArg("temp")) tempWarning = constrain(server.arg("temp").toFloat(), -20.0f, 80.0f);
    if (server.hasArg("hum")) humWarning = constrain(server.arg("hum").toFloat(), 0.0f, 100.0f);
    if (server.hasArg("auto")) autoPages = server.arg("auto") == "1";
    if (server.hasArg("screen")) screenEnabled = server.arg("screen") == "1";
    server.send(200, "text/plain", "OK");
  });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void showStartupScreen() {
  for (int i = 0; i <= 110; i += 5) {
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2); display.setCursor(11, 8); display.print("WEATHER");
    display.setTextSize(1); display.setCursor(40, 33); display.print("STATION 2.0");
    display.drawRect(9, 52, 110, 8, SSD1306_WHITE);
    display.fillRect(10, 53, min(i, 108), 6, SSD1306_WHITE);
    display.display(); delay(35);
  }
}
void drawHeader(const char* name) {
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.print(name);
  display.setCursor(103, 0); display.print(WiFi.status() == WL_CONNECTED ? "WIFI" : "OFF");
  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);
}
void drawHistoryGraph(const float* values) {
  if (historyCount < 2) { display.setCursor(0, 34); display.print("Collecting data..."); return; }
  float low = INFINITY, high = -INFINITY;
  for (size_t i = 0; i < historyCount; i++) {
    size_t idx = (historyNext + HISTORY_SIZE - historyCount + i) % HISTORY_SIZE;
    low = min(low, values[idx]); high = max(high, values[idx]);
  }
  if (high - low < 0.5f) { high += 0.5f; low -= 0.5f; }
  int prevX = 0, prevY = 0;
  for (size_t i = 0; i < historyCount; i++) {
    size_t idx = (historyNext + HISTORY_SIZE - historyCount + i) % HISTORY_SIZE;
    int x = (int)(i * 127 / (historyCount - 1));
    int y = 58 - (int)((values[idx] - low) / (high - low) * 33);
    if (i) display.drawLine(prevX, prevY, x, y, SSD1306_WHITE);
    prevX = x; prevY = y;
  }
}
void updateDisplay() {
  if (!screenEnabled) { display.clearDisplay(); display.display(); return; }
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
  switch (currentPage) {
    case 0: {
      drawHeader("WEATHER");
      display.setTextSize(2); display.setCursor(0, 15);
      String t = timeString("%H:%M:%S"); display.print(t == "--" ? "--:--:--" : t);
      display.setTextSize(1); display.setCursor(0, 34); display.print(timeString("%d.%m.%Y"));
      display.drawLine(0, 45, 127, 45, SSD1306_WHITE);
      display.setCursor(0, 51); display.print("T:");
      if (isnan(temperature)) display.print("--"); else display.print(temperature, 1);
      display.print("C  H:");
      if (isnan(humidity)) display.print("--"); else display.print(humidity, 0);
      display.print("%");
      break;
    }
    case 1:
      drawHeader("TEMP HISTORY");
      display.setCursor(0, 15); display.print("Now: ");
      if (isnan(temperature)) display.print("--"); else display.print(temperature, 1);
      display.print(" C"); drawHistoryGraph(tempHistory); break;
    case 2:
      drawHeader("SYSTEM"); display.setCursor(0, 17);
      display.print("Uptime: "); display.print(millis() / 60000); display.println(" min");
      display.print("Free RAM: "); display.println(ESP.getFreeHeap());
      display.print("Samples: "); display.println(sampleCount);
      display.print("T min/max: ");
      if (isnan(minTemp)) display.print("--"); else display.print(minTemp, 0);
      display.print("/");
      if (isnan(maxTemp)) display.print("--"); else display.print(maxTemp, 0);
      break;
    case 3:
      drawHeader("NETWORK"); display.setCursor(0, 17);
      display.print("Status: "); display.println(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");
      display.print("IP: "); display.println(WiFi.localIP());
      display.print("RSSI: ");
      if (WiFi.status() == WL_CONNECTED) display.print(WiFi.RSSI()); else display.print("--");
      display.println(" dBm");
      display.print("Open IP in browser"); break;
  }
  bool warning = (!isnan(temperature) && temperature >= tempWarning) || (!isnan(humidity) && humidity >= humWarning);
  if (warning && currentPage != 0) { display.fillRect(120, 57, 7, 7, SSD1306_WHITE); }
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED initialization failed - check wiring and address");
    while (true) delay(1000);
  }
  showStartupScreen();
  dht.begin();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(100); yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWeather Station: http://"); Serial.println(WiFi.localIP());
  } else Serial.println("\nWiFi offline; will retry");
  configTzTime(timezone, "pool.ntp.org", "time.google.com");
  setupWebServer();
  lastPageChange = millis();
  lastSensorRead = millis() - 2000;
  lastHistorySample = millis() - 60000;
}
void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (now - lastSensorRead >= 2000) {
    lastSensorRead = now;
    float t = dht.readTemperature(), h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temperature = t; humidity = h;
      if (isnan(minTemp) || t < minTemp) minTemp = t;
      if (isnan(maxTemp) || t > maxTemp) maxTemp = t;
      if (isnan(minHum) || h < minHum) minHum = h;
      if (isnan(maxHum) || h > maxHum) maxHum = h;
      sumTemp += t; sumHum += h; sampleCount++;
    }
  }
  if (now - lastHistorySample >= 60000) { lastHistorySample = now; recordHistory(); }
  if (now - lastWifiCheck >= 10000) {
    lastWifiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }
  }
  if (autoPages && now - lastPageChange >= 7000) {
    lastPageChange = now; currentPage = (currentPage + 1) % 4;
  }
  if (now - lastScreenDraw >= 350) { lastScreenDraw = now; updateDisplay(); }
  delay(2);
}
