# ESP32 Marauder N16

Готовая сборка ESP32 Marauder v1.20.0 для **ESP32-C5 DevKitC-1 N16R8** с флеш-памятью 16 MB и BLE-консолью. Основа проекта — [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder), commit `511bb36`. Лицензия и уведомление об авторстве находятся в `LICENSE` и `NOTICE.md`.

## Поддерживаемые платы

| Плата | Состояние |
| --- | --- |
| ESP32-C5 DevKitC-1 N16R8 | Проверено на реальной плате |
| ESP32-S3 N16 | Только исходный шаблон; готового универсального бинарника нет |

**Нельзя прошивать бинарник C5 в ESP32-S3.** Это разные микросхемы. Для S3 нужно отдельно проверить распиновку, USB, PSRAM, антенну и загрузчик конкретной платы.

## Прошивка ESP32-C5 N16R8

### Полный образ (рекомендуется)

В `firmware/esp32-c5-n16r8/` находится `merged-v1.20.0-ble.bin` — полный образ на 16 MB, запись начинается с `0x0`.

1. Подключи плату по USB и узнай её COM-порт.
2. Установи esptool: `py -m pip install esptool`.
3. Зажми **BOOT**, нажми **RESET**, отпусти **BOOT**.
4. Замени `COM8` на свой порт и выполни:

```powershell
py -m esptool --chip esp32c5 --port COM8 erase-flash
py -m esptool --chip esp32c5 --port COM8 --baud 460800 write-flash 0x0 firmware/esp32-c5-n16r8/merged-v1.20.0-ble.bin
```

После записи нажми **RESET**. Контрольные суммы находятся в `SHA256SUMS`.

### Отдельные файлы

| Файл | Адрес |
| --- | ---: |
| `bootloader.bin` | `0x2000` |
| `partitions.bin` | `0x8000` |
| `app-v1.20.0-ble.bin` | `0x10000` |

```powershell
py -m esptool --chip esp32c5 --port COM8 --baud 460800 write-flash 0x2000 firmware/esp32-c5-n16r8/bootloader.bin 0x8000 firmware/esp32-c5-n16r8/partitions.bin 0x10000 firmware/esp32-c5-n16r8/app-v1.20.0-ble.bin
```

## Сборка исходников

Нужны Arduino-ESP32 3.3.2 и параметры `FlashSize=16M`, `PartitionScheme=custom`, `PSRAM=enabled`. Скетч — `esp32_marauder/esp32_marauder.ino`; изменения BLE-консоли — в `RemoteConsole.*` и связанных файлах.

## ESP32-S3 N16

См. `docs/S3-N16.md`. Профиль `MARAUDER_MULTIBOARD_S3` предназначен для Flipper Zero Multi Board S3 и не является универсальным профилем для ATewin или другой S3 N16.

## Безопасность

Используй прошивку только на своих устройствах и сетях либо с разрешением владельца. Сканирование и радиотесты могут повлиять на других пользователей. Подмена MAC не отменяет правила доступа к сети.

## Структура

- `esp32_marauder/` — исходники;
- `firmware/esp32-c5-n16r8/` — файлы прошивки;
- `partitions/` — таблица разделов 16 MB;
- `scripts/` — вспомогательные скрипты;
- `docs/` — заметки по S3 N16.
