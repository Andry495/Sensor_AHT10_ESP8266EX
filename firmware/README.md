# Готовая прошивка

Файл **`th-sens.bin`** — образ для ESP8266EX / NodeMCU (`nodemcuv2`).

**В бинарник не зашита домашняя Wi‑Fi сеть.** После флеша:

1. Подключитесь к AP `TH-Sens-Setup` (пароль `thsens5011`).
2. Откройте http://192.168.4.1/ и укажите SSID/пароль своей сети.
3. Устройство появится в LAN как `th-sens-<mac>.local`.

## Заливка (Windows, пример COM8)

```bash
python -m esptool --port COM8 --baud 460800 write-flash --flash-mode dout --flash-size detect 0x0 th-sens.bin
```

Из корня репозитория:

```bash
python -m esptool --port COM8 --baud 460800 write-flash --flash-mode dout --flash-size detect 0x0 firmware/th-sens.bin
```

## Ключи публичного бинарника

Этот `th-sens.bin` собран с ключами ниже (нужны для Home Assistant / OTA).  
Для продакшена лучше **пересобрать** из `esphome/` со своими секретами.

| Секрет | Значение |
|--------|----------|
| `api_encryption_key` | `2vPQjhoXA7O4xUGLkt6Dc2abu238DQfxGI+tAEM5+y4=` |
| `ota_password` | `thsens-public-ota` |

Сборка своей версии:

```bash
cd esphome
cp secrets.yaml.example secrets.yaml   # заполните свои ключи
esphome compile th-sens.yaml
copy .esphome\build\th-sens\.pio\build\th-sens\firmware.bin ..\firmware\th-sens.bin
```
