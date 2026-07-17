# Готовая прошивка

Файл **`th-sens.bin`** — образ для ESP8266EX / NodeMCU (`nodemcuv2`), версия проекта **1.3.2**.

**В бинарник не зашита домашняя Wi‑Fi сеть.** Сеть задаётся один раз через captive portal и сохраняется во flash (переживает отключение питания).

## Быстрый старт

1. Прошейте `th-sens.bin` по USB (команда ниже).
2. Подключитесь к AP **`TH-Sens-Setup`**, пароль **`password`** (одинаковый на всех платах).
3. В браузере откройте **http://192.168.4.1/**  
   На телефоне портал часто не всплывает сам: отключите LTE, выберите «оставить сеть без интернета», откройте адрес вручную.
4. Укажите SSID/пароль своей сети → устройство появится в LAN как `th-sens-<mac>.local`.
5. Откройте `http://th-sens-XXXXXX.local/` → скопируйте **Encryption key** → добавьте в Home Assistant (интеграция ESPHome).

## Заливка (Windows, пример COM8)

```bash
python -m esptool --port COM8 --baud 460800 write-flash --flash-mode dout --flash-size detect 0x0 th-sens.bin
```

Из корня репозитория:

```bash
python -m esptool --port COM8 --baud 460800 write-flash --flash-mode dout --flash-size detect 0x0 firmware/th-sens.bin
```

## Ключи публичного бинарника

| Секрет | Значение | Для Home Assistant? |
|--------|----------|---------------------|
| `api_encryption_key` | `2vPQjhoXA7O4xUGLkt6Dc2abu238DQfxGI+tAEM5+y4=` | **Да** — вводится при добавлении ESPHome |
| `ota_password` | `thsens-public-ota` | **Нет** — только для ESPHome OTA (порт 8266) |

Encryption key также отображается в веб‑UI устройства (блок «Home Assistant — Encryption key») и в JSON `/ui/api` (`api_encryption_key`).

OTA password в UI для HA не показывается — он не нужен интеграции Home Assistant.

Для продакшена лучше **пересобрать** из `esphome/` со своими секретами.

## Сохранение настроек

| Что | После отключения питания |
|-----|--------------------------|
| Wi‑Fi SSID/пароль (заданные через портал) | Сохраняются |
| Калибровка Temp/Humidity Offset | Сохраняется |
| Encryption key / OTA password | В прошивке (меняются только пересборкой) |

Сброс Wi‑Fi и калибровки: Factory Reset в UI или удержание BOOT ≥ 5 с.

## Сборка своей версии

```bash
cd esphome
cp secrets.yaml.example secrets.yaml   # заполните свои api_encryption_key и ota_password
esphome compile th-sens.yaml
copy .esphome\build\th-sens\.pio\build\th-sens\firmware.bin ..\firmware\th-sens.bin
```

Полная документация: [README.md](../README.md) в корне репозитория.
