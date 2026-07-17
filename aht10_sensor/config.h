#pragma once

// Legacy Arduino‑дерево: STA‑сеть сюда НЕ зашивать.
// Актуальная прошивка — esphome/ (AP + captive portal при первом запуске).
// Оставьте пустыми: без SSID Arduino‑скетч не подключается к Wi‑Fi.
static constexpr const char *WIFI_SSID = "";
static constexpr const char *WIFI_PASSWORD = "";
