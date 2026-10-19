# RFID access controller

ESP32 + MFRC522 RFID reader. Tap a card: it's granted or denied, logged,
and shown on an LED and a GPIO output. A web page shows the access log and
lets you add or remove cards.

There's no physical lock yet. Every decision goes through one function,
`access_event()`, so a lock, buzzer or MQTT message can be connected there
later without touching the rest.

## Quick start

```sh
idf.py menuconfig        # WiFi station -> SSID, password; RFID web UI -> admin password
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Open `http://<ip from the "got ip" log line>/`. Tap a card, press **Allow**
next to it in the log, give it a name. The next tap is granted.

Wiring is in `main/pincfg.h`. The ESP32 only sees 2.4 GHz WiFi networks.

## Layout

Each folder is one ESP-IDF component with one job. Dependencies point one
way, and the build enforces it (`REQUIRES` in each `CMakeLists.txt`):

```
main ──► web ──► access ──► rc522
  └────► wifi
```

| Path | Job | Knows about |
|---|---|---|
| `main/main.c` | Boot sequence and the card-polling loop. The only file that wires everything together. | everything |
| `main/pincfg.h` | Which GPIO goes where. | — |
| `components/rc522/` | MFRC522 driver: SPI, registers, REQA/ANTICOLL, 4-byte UID. No app logic. | — |
| `components/wifi/` | Join the network from menuconfig, reconnect forever. | — |
| `components/access/` | The app core: `cards` (who's allowed, saved in NVS), `accesslog` (what happened), `access_event()` (what a decision does), `uid` (hex ↔ bytes). | rc522 (UID length) |
| `components/web/` | HTTP server: the page (`index.html`) and the JSON API. | access |

Inside a component, `include/` is its public API. Everything else is private.

## How a tap flows

```
rfid_loop (main.c, every 250 ms)
  rc522_read_uid()
  same card within 3 s? ── yes ──► ignore
  cards_allowed(uid)                       decide
  access_event(uid, granted)               react
    ├─ accesslog_add()   ring buffer, 64 entries
    ├─ ESP_LOGI
    └─ LED + output pin  (esp_timer turns them off; never blocks)
```

The RFID loop and the HTTP task touch `cards` and `accesslog` at the same
time, so both keep their data behind a mutex. Other code only gets copies
(`*_snapshot()`).

## HTTP API

| Method | Path | Body | Auth |
|---|---|---|---|
| `GET` | `/` | | |
| `GET` | `/api/log` | | |
| `GET` | `/api/cards` | | |
| `POST` | `/api/cards` | form: `uid=DEADBEEF&name=...` (adds, or renames) | `X-Password` |
| `DELETE` | `/api/cards?uid=DEADBEEF` | | `X-Password` |

The password is sent over plain HTTP. That's fine on a home network, not on
an open one.

## Configuration

| Where | What |
|---|---|
| `idf.py menuconfig` | WiFi SSID and password, admin password. Saved in `sdkconfig`, which is **not committed**. |
| `sdkconfig.defaults` | Project settings that differ from ESP-IDF defaults. Committed. |
| `main/pincfg.h` | Pins. |

## Extending

- **New output on grant/deny** (lock, buzzer, MQTT): add it inside `access_event()` in `components/access/access.c`.
- **New page or endpoint**: a handler and a route in `components/web/web.c`.
- **New hardware driver**: a new folder in `components/`, with its API in `include/`, and wire it up in `main.c`.
