# ESP8266-WIFI-CAPTIVE-CONFIG-EEPROM — provision wifi and mqtt settings through a captive portal and persist them in littlefs

IoT example by [Fluidwire](https://fluidwire.com/).

The ESP8266 starts its own access point with a captive portal when it has no saved settings (or can't join the saved WiFi). You enter WiFi and MQTT details from your phone, they are stored as JSON in LittleFS flash, and the board reboots into normal operation. No hard-coded credentials, no recompiling per site.

## What it does

- On boot, loads `/config.json` from LittleFS and tries to join the saved WiFi
- If there is no config, or the join fails after 15 s, starts the `Fluidwire-Setup` access point with a DNS catch-all so phones open the config page automatically
- Serves a form for WiFi SSID/password, MQTT host, port and topic; saves them as JSON and reboots
- Hold the **FLASH** button (GPIO0) for 3 seconds at boot to wipe the saved config and return to portal mode

## Required hardware

- 1x ESP8266 board (NodeMCU or Wemos D1 mini)
- Nothing else — uses the on-board FLASH button and flash storage

## Required libraries

Install from **Library Manager**:

- `ArduinoJson` by Benoit Blanchon

Built into the **ESP8266 Arduino core** (nothing to install): `ESP8266WiFi`, `ESP8266WebServer`, `DNSServer`, `LittleFS`. Install the core via **Boards Manager** (search `esp8266` by ESP8266 Community).

## Setup

1. Flash this sketch. There is **no `config.h`** — settings are entered in the portal. Change `AP_PASSWORD` (default `fluidwire123`) before deploying.
2. Select **Tools → Board → NodeMCU 1.0 (ESP-12E)** (or your board) and, under **Flash Size**, pick an option with a filesystem (e.g. `4MB (FS:2MB)`).
3. Click **Upload**.

## Expected output

Serial Monitor at **115200** baud, first boot:

```
LittleFS mount failed, formatting...
No saved config - starting portal.
Portal mode: join "Fluidwire-Setup" and open http://192.168.4.1/
```

After you save settings and the board reboots:

```
Config loaded from flash.
Joining "MyWiFi"....
Connected. IP 192.168.1.50, MQTT broker.local:1883, topic fluidwire/esp8266/data
```

## Troubleshooting

- **Portal page doesn't pop up** — browse to `http://192.168.4.1/` manually; some phones suppress captive pop-ups on networks without internet.
- **Config never saves** — make sure the board's Flash Size setting includes a filesystem partition.
- **Stuck in portal mode** — the saved password may be wrong; re-enter it (leaving the password blank keeps the old one) or hold FLASH for 3 s at boot to wipe.
- **Can't join the AP** — `AP_PASSWORD` must be at least 8 characters.

## Security note

The config is stored in plain text in flash and the portal has no login, so treat it as on-site provisioning only. Change the default AP password, and don't leave a device sitting in portal mode in a public space.

## License

MIT — see [LICENSE](LICENSE).

## Got stuck?

Drop by [fluidwire.com](https://fluidwire.com/) — we ship IoT building blocks and are happy to help.
