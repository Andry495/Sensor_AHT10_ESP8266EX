# Sensor AHT10 ESP8266EX

Прошивка Wi‑Fi датчика **температуры и влажности** на **ESP8266EX** (NodeMCU) + **AHT10** с полной совместимостью с [Home Assistant](https://www.home-assistant.io/) через нативный [ESPHome API](https://esphome.io/components/api.html).

Репозиторий: [Andry495/Sensor_AHT10_ESP8266EX](https://github.com/Andry495/Sensor_AHT10_ESP8266EX)

| | |
|---|---|
| Платформа | ESP8266EX / NodeMCU v2 |
| Датчик | AHT10 (только температура и влажность) |
| Фреймворк | [ESPHome](https://esphome.io/) ≥ 2026.7 |
| HA API | порт **6053**, шифрование Noise |
| Web | порт **80** |
| OTA | порт **8266** + Web OTA |

**Wi‑Fi STA‑сеть в прошивку не зашивается.** После USB‑флеша устройство поднимает AP и captive portal — SSID/пароль вводятся один раз и сохраняются во flash.

---

## Возможности

- Нативный ESPHome API для Home Assistant (автообнаружение по mDNS)
- Шифрование API (Noise PSK)
- Веб‑интерфейс:
  - `/` — свой UI (показания, калибровка, live‑лог)
  - `/esp` — стандартный UI ESPHome (как в HA)
  - `/prom` — метрики Prometheus
  - `/events` — SSE (состояния сущностей + логи)
- **Captive portal** для ввода Wi‑Fi (без встроенных сетей в бинарнике)
- OTA (ESPHome + Web)
- Калибровка температуры и влажности (сохранение во flash)
- Factory reset (кнопка в UI / удержание BOOT ≥ 5 с)

---

## Железо

| Элемент | Подключение |
|---------|-------------|
| Плата | NodeMCU ESP8266 (`nodemcuv2`) |
| Датчик | **AHT10** — только T и RH (CO₂/TVOC нет) |
| I2C SDA | **GPIO4** (D2) |
| I2C SCL | **GPIO5** (D1) |
| Адрес AHT10 | `0x38` |
| Опрос AHT10 | **10 с** (даташит: не чаще **1 / 2 с**) |
| BOOT/FLASH | **GPIO0**, удержание **5–30 с** → factory reset |
| Питание | USB 5V или БП **5V → VIN** + GND |
| Serial | 115200 8N1 |

### AHT10: что измеряет и как часто

AHT10 измеряет **только температуру и относительную влажность** (CO₂, TVOC, давление — нет).

По [даташиту ASAIR AHT10](https://cdn.compacttool.ru/downloads/AHT10_en_datasheet.pdf):

- при слишком частом опросе растёт собственный нагрев сенсора и падает точность;
- чтобы рост температуры сенсора был **&lt; 0.1 °C**, время активной работы не должно превышать ~10% цикла;
- **рекомендация производителя: измерение раз в 2 секунды**;
- в этой прошивке выбран интервал **10 с** — с запасом по нагреву и стабильнее на внешнем БП.

### Питание от внешнего БП

- Подавайте **5 V на контакт VIN** (или USB), общий **GND**
- Не подавайте 5 V на пин `3V3`
- БП желательно стабильный, ≥ **500 mA**
- При одновременном USB + БП — общий GND, избегайте конфликта двух источников 5 V

### Схема (логика)

```text
         ESP8266 NodeMCU
    ┌─────────────────────┐
    │  3V3 ───────────────┼─── VCC  AHT10
    │  GND ───────────────┼─── GND  AHT10
    │  D2 (GPIO4) ────────┼─── SDA  AHT10
    │  D1 (GPIO5) ────────┼─── SCL  AHT10
    │  D3 (GPIO0)  BOOT   │     (удержание ≥5 с = factory reset)
    │  VIN / USB ── 5V    │
    └─────────────────────┘
```

---

## Ввод в эксплуатацию

Порядок всегда такой:

1. **Прошить** по USB (готовый бинарник из `firmware/` или сборка из `esphome/`).
2. **Подключиться** к точке доступа контроллера.
3. **Указать** SSID и пароль своей сети в captive portal.
4. Открыть веб‑UI → скопировать **Encryption key** → добавить устройство в Home Assistant.

| | |
|---|---|
| SSID AP | `TH-Sens-Setup` |
| Пароль AP | `password` (одинаковый на **всех** платах) |
| Портал | http://192.168.4.1/ |

После успешного подключения к вашей сети AP гаснет; hostname: `th-sens-<mac>.local`.

### Сохранение Wi‑Fi после настройки

SSID и пароль вашей сети **сохраняются во flash**. Они переживают:

- обычное отключение питания;
- перезагрузку / Restart;
- повторную прошивку **без** erase flash (если credentials уже были сохранены).

Сбрасываются только при:

- **Factory Reset** (кнопка в UI или BOOT ≥ 5 с);
- полной очистке flash при прошивке (`erase_flash` и т.п.).

После factory reset снова поднимается AP `TH-Sens-Setup` / `password`.

### Телефон не открывает 192.168.4.1

Captive portal часто **не** всплывает сам:

1. Отключите мобильный интернет (LTE/5G).
2. В уведомлении Wi‑Fi выберите «Оставить в этой сети» / «без интернета».
3. В браузере вручную откройте **http://192.168.4.1/**.

### Factory reset (смена сети)

- Кнопка **Factory Reset** в `/` или `/esp`
- Удержание **BOOT/FLASH (GPIO0) ≥ 5 с**

Сбрасываются сохранённые Wi‑Fi credentials и preference (в т.ч. калибровка).

---

## Ключи и пароли: что для чего

| Что | Нужно для HA? | Где взять | Примечание |
|-----|---------------|-----------|------------|
| **Encryption key** (`api_encryption_key`) | **Да, обязательно** | Веб‑UI `/` → «Home Assistant — Encryption key» → Копировать; также `/ui/api` → `api_encryption_key` | Зашивается при сборке. После смены ключа в secrets и перепрошивки UI покажет новый |
| **OTA password** (`ota_password`) | **Нет** | `firmware/README.md` (публичный бинарник) или свой `secrets.yaml` | Только для ESPHome OTA (`esphome upload`, порт 8266). В HA **не** вводится |
| Пароль AP `password` | Нет | Документация / этот README | Только для первичной настройки Wi‑Fi |
| SSID/пароль домашней Wi‑Fi | Нет в прошивке | Вводятся на устройстве через портал | Не коммитить в git |

**Итог для Home Assistant:** при добавлении интеграции ESPHome нужен только **Encryption key**. OTA password в HA не используется и в UI для HA не показывается.

Публичный `firmware/th-sens.bin` собран с ключами из [`firmware/README.md`](firmware/README.md). Своя сборка — свои ключи в `esphome/secrets.yaml`.

---

## Совместимость с Home Assistant

Проект рассчитан на **полную** интеграцию через нативный ESPHome API.

| Требование HA / ESPHome | В проекте |
|-------------------------|-----------|
| Компонент `api:` | ✅ порт **6053** |
| Шифрование Noise | ✅ `encryption.key` из `secrets.yaml` |
| mDNS / discovery | ✅ `th-sens-<mac>.local` |
| `device_class` | ✅ temperature, humidity, signal_strength, connectivity |
| `state_class` | ✅ `measurement` для Temp/Lucht |
| `entity_category` | ✅ diagnostic / config где уместно |
| OTA | ✅ ESPHome `:8266` + Web OTA |
| Web UI / диагностика | ✅ `/` и `/esp` |
| `reboot_timeout` | ✅ **`0s`** — устройство **не** перезагружается, если HA не подключён (удобно для Prometheus-only) |

Документация API: [Native API Component](https://esphome.io/components/api.html)  
Онбординг: [Getting Started with ESPHome and Home Assistant](https://esphome.io/guides/getting_started_hassio.html)

### Добавление в Home Assistant

1. Убедитесь, что устройство уже в вашей Wi‑Fi (шаг портала выше) и в той же сети, что HA.
2. Откройте веб‑UI датчика `http://th-sens-XXXXXX.local/` → блок **Home Assistant — Encryption key** → **Копировать ключ**.  
   Для публичного бинарника тот же ключ продублирован в `firmware/README.md`.
3. **Настройки → Устройства и службы → Добавить интеграцию → ESPHome**.
4. Хост: `th-sens-XXXXXX.local` или IP.
5. Вставьте **только Encryption key** (поле Noise / encryption).  
   **OTA password сюда не нужен.**
6. После подключения `Status` станет **ON**.

Автообнаружение обычно появляется в течение нескольких минут после выхода устройства в Wi‑Fi.

### Ожидаемые сущности в HA

Имена зависят от MAC‑суффикса устройства (`name_add_mac_suffix: true`). Пример для `th-sens-587242`:

| Тип | Пример entity_id | Описание |
|-----|------------------|----------|
| `sensor` | `sensor.th_sens_587242_temp` | Температура °C |
| `sensor` | `sensor.th_sens_587242_lucht` | Влажность % |
| `sensor` | `sensor.th_sens_587242_wifi` | RSSI dBm |
| `binary_sensor` | `binary_sensor.th_sens_587242_status` | Связь с API |
| `number` | `number.th_sens_587242_temp_offset` | Калибровка T |
| `number` | `number.th_sens_587242_humidity_offset` | Калибровка RH |
| `button` | `button.th_sens_587242_restart` | Перезагрузка |
| `button` | `button.th_sens_587242_factory_reset` | Сброс |
| `text_sensor` | `…_ip`, `…_ssid`, `…_mac` | Диагностика сети |

Точные `entity_id` зависят от версии HA; смотрите карточку устройства после добавления интеграции.

### Важные нюансы HA

- Для интеграции HA нужен **только Encryption key**. OTA password — отдельно, только для прошивки по воздуху.
- Ключ в UI читается из работающей прошивки: после пересборки с новым ключом и перепрошивки UI покажет новый.
- **`reboot_timeout: 0s`** — устройство **не** перезагружается, если клиент API (HA) долго не подключён (удобно для Prometheus-only).
- Нативный API: порт **6053/tcp** с хоста HA.
- ESPHome OTA: порт **8266/tcp**, пароль `ota_password` (не для HA).

---

## Быстрый старт

### Требования

- Python 3.11+ и [ESPHome](https://esphome.io/guides/installing_esphome.html) ≥ 2026.7  
  `pip install esphome`
- [PlatformIO Core](https://platformio.org/install/cli) (подтянется/используется при сборке)
- USB‑UART (CP210x и т.п.), порт вида `COM8` (Windows)

### 1. Секреты (только API / OTA)

```bash
cd esphome
cp secrets.yaml.example secrets.yaml
```

Заполните `secrets.yaml`:

| Ключ | Назначение |
|------|------------|
| `api_encryption_key` | `openssl rand -base64 32` или генератор на [странице API](https://esphome.io/components/api.html) |
| `ota_password` | Пароль OTA |

> Wi‑Fi SSID/пароль в `secrets.yaml` **не** указываются — они задаются на устройстве через портал.  
> `secrets.yaml` в git не коммитится (см. `.gitignore`).

### 2. Первая прошивка по USB

Готовый бинарник (если не собираете сами):

```bash
python -m esptool --port COM8 --baud 460800 write-flash --flash-mode dout --flash-size detect 0x0 firmware/th-sens.bin
```

Сборка из исходников:

```bash
cd esphome
esphome compile th-sens.yaml
# или: esphome run th-sens.yaml
```

На Windows путь с пробелами удобнее обходить так: запускать команды **из каталога `esphome/`** с относительным `th-sens.yaml`.

Ручная заливка после своей сборки:

```bash
python -m esptool --port COM8 --baud 460800 write-flash --flash-mode dout --flash-size detect 0x0 .esphome/build/th-sens/.pio/build/th-sens/firmware.bin
```

### 3. Настройка Wi‑Fi (обязательно)

1. Подключите телефон/ПК к Wi‑Fi **`TH-Sens-Setup`** (пароль **`password`**).
2. Откройте в браузере вручную **http://192.168.4.1/**  
   (на телефоне портал часто **не** всплывает сам: отключите мобильный интернет, в уведомлении Wi‑Fi выберите «Оставить в этой сети» / «без интернета»).
3. Выберите свою сеть, введите пароль, сохраните.
4. Устройство перезапустится и появится в вашей LAN.

### 4. OTA (после выхода в сеть)

OTA password **не** нужен для Home Assistant. Он нужен только для обновления прошивки:

```bash
cd esphome
esphome upload th-sens.yaml --device th-sens-XXXXXX.local
# либо Web OTA в интерфейсе устройства (/esp)
```

Для публичного бинарника пароль OTA: см. `firmware/README.md` (`thsens-public-ota`).

> При `name_add_mac_suffix: true` у каждой платы свой hostname. Целевой адрес OTA указывайте явно.

---

## Hostname

| Параметр | Значение |
|----------|----------|
| Базовое имя | `th-sens` |
| Суффикс | последние **6** hex‑символов MAC (`name_add_mac_suffix: true`) |
| Пример | MAC `CC:50:E3:58:72:42` → **`th-sens-587242`** |
| mDNS | `th-sens-587242.local` |

Friendly name в UI/HA: `TH Sensor XXXXXX`.

---

## HTTP‑эндпоинты

| URL | Описание |
|-----|----------|
| `/` | Свой UI: показания, калибровка, live‑лог |
| `/ui`, `/ui/` | То же, что `/` |
| `/ui/api` | JSON API |
| `/esp` | Стандартный веб‑UI ESPHome (как в HA) |
| `/prom` | Метрики Prometheus (текст) |
| `/events` | SSE: `state`, `log`, `ping` |
| `/sensor/<Name>` | REST состояния сенсора |
| `/number/<Name>/set?value=` | Установка калибровки (**имя с пробелами**, URL‑encode) |
| `/button/<Name>/press` | Restart / Factory Reset |

### Пример `/ui/api`

```json
{
  "ok": true,
  "hostname": "th-sens-587242",
  "ip": "192.168.1.50",
  "ssid": "YourHomeWiFi",
  "t": 24.60,
  "rh": 42.02,
  "rssi": -59,
  "temp_offset": -1.0,
  "humidity_offset": 0.4,
  "prom": "/prom"
}
```

### Пример калибровки через HTTP

```http
POST /number/Temp%20Offset/set?value=-1.5
POST /number/Humidity%20Offset/set?value=3.5
```

### Пример `/prom`

```text
#Prometey TempDataEthernet
tde_temperature 24.60
tde_humidity 42.02
tde_temp_offset -1.00
tde_humidity_offset 0.40
tde_wifi_rssi -59
...
```

Совместимые алиасы scrapers: `tde_temperature`, `tde_humidity`. Плюс системные метрики контроллера (`tde_heap_*`, `tde_wifi_*`, IP/BSSID октеты и т.д.).

---

## Калибровка

Дефолты прошивки (если во flash ещё ничего не сохранено):

| Параметр | Default |
|----------|---------|
| Temp Offset | **−2.0 °C** |
| Humidity Offset | **+4.0 %** |

Настройка:

1. Откройте `http://<устройство>/`
2. Блок **Калибровка** → введите смещения → **Сохранить**
3. Либо `/esp` → сущности **Temp Offset** / **Humidity Offset**
4. Либо HA → `number.th_sens_<mac>_temp_offset` / `…_humidity_offset`

Значения переживают перезагрузку (`restore_value` + `restore_from_flash`).

---

## Структура репозитория

```text
.
├── README.md                 ← этот файл
├── .gitignore
├── firmware/
│   ├── th-sens.bin           ← готовая прошивка (без встроенного Wi‑Fi STA)
│   └── README.md             ← заливка esptool + ключи сборки
├── esphome/                  ← исходники прошивки (ESPHome)
│   ├── th-sens.yaml
│   ├── secrets.yaml.example
│   ├── secrets.yaml          ← локально, в git не входит
│   └── extra_components/
│       └── custom_ui/        ← /, /esp, /prom, /ui/api
├── aht10_sensor/             ← УСТАРЕВШИЙ Arduino‑скетч (без HA API)
├── src/, include/            ← наследие Arduino
├── tools/                    ← вспомогательные HTTP/SSE‑probe скрипты
└── platformio.ini            ← наследие; актуальная сборка — через ESPHome
```

**Актуальная прошивка — каталог `esphome/` (+ бинарник `firmware/th-sens.bin`).**  
Arduino‑дерево оставлено для истории и не обеспечивает нативный HA API / captive portal.

---

## Сущности (firmware)

| Тип | Имя | YAML id | Категория |
|-----|-----|---------|-----------|
| sensor | Temp | `sens_temp` | |
| sensor | Lucht | `sens_lucht` | |
| sensor | WiFi | `wifi_signal_sensor` | diagnostic |
| binary_sensor | Status | — | diagnostic |
| binary_sensor | Boot Button | — | internal |
| number | Temp Offset | `temp_offset` | config |
| number | Humidity Offset | `humidity_offset` | config |
| button | Restart | — | config |
| button | Factory Reset | `factory_reset_btn` | diagnostic |
| text_sensor | IP / SSID / MAC | — | diagnostic |

---

## Чеклист совместимости с Home Assistant

- [x] `api:` на порту 6053  
- [x] Noise encryption key  
- [x] mDNS (`*.local`)  
- [x] `device_class` / `state_class`  
- [x] Diagnostic/config entity categories  
- [x] OTA  
- [x] `reboot_timeout: 0s` (нет циклических ребутов без HA)  
- [x] Веб‑диагностика и кнопки Restart / Factory Reset  
- [x] Калибровка как `number` (доступна в HA)  
- [x] Wi‑Fi только через captive portal (STA не в бинарнике)

Руководства:

- [Native API](https://esphome.io/components/api.html)
- [Getting started with HA](https://esphome.io/guides/getting_started_hassio.html)

---

## Безопасность

- Не коммитьте `esphome/secrets.yaml`
- STA‑сеть и пароль **не** хранятся в исходниках и не попадают в публичный бинарник (только во flash устройства после портала)
- Пароль AP по умолчанию: `password` (один на все платы). При необходимости смените в `th-sens.yaml` и пересоберите
- Encryption key отображается в веб‑UI (нужен для HA). OTA password в UI для HA не показывается
- Для своей сборки сгенерируйте свой `api_encryption_key` и `ota_password`
- Ограничьте доступ к порту 80/6053/8266 по VLAN/firewall при необходимости

---

## Устранение неполадок

| Симптом | Что проверить |
|---------|----------------|
| HA: Status OFF | Encryption key (не OTA password), порт 6053, одна сеть с HA, mDNS |
| HA просит OTA password | Не нужен — вводите только Encryption key |
| Пустой `/esp` | Нужен доступ в интернет к `oi.esphome.io` (JS). Жёсткое обновление кэша |
| `nan` у Temp/RH | Распиновка I2C (D2/D1), питание AHT10 **3.3V**, интервал ≥2 с; после бута подождать первый опрос |
| Нет в Wi‑Fi | AP `TH-Sens-Setup` / `password` → http://192.168.4.1/ вручную; отключить LTE |
| Портал не открывается на телефоне | LTE выкл., «оставить сеть без интернета», браузер → http://192.168.4.1/ |
| После отключения питания «забыл» Wi‑Fi | Не должно: credentials во flash. Если так — был factory reset или erase flash |
| Не стабилен на БП / «нет данных» | 5V на **VIN**, ток ≥500 mA, общий GND; не подавать 5V на `3V3`; пинг IP/`*.local` |
| Калибровка не сохраняется | POST на `/number/Temp%20Offset/set` и `/number/Humidity%20Offset/set` (имя **с пробелами**); Ctrl+F5 |
| Ребут каждые ~15 мин | В этой прошивке `api.reboot_timeout: 0s` — отключено |

---

## Лицензия и ссылки

- Репозиторий: [Andry495/Sensor_AHT10_ESP8266EX](https://github.com/Andry495/Sensor_AHT10_ESP8266EX)
- [ESPHome](https://esphome.io/)
- [Home Assistant](https://www.home-assistant.io/)
- Датчик: ASAIR AHT10 (температура + влажность)
