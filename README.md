# BC250 ESP32 Power Switch

An ESP32-C3 power controller for an AMD **BC250** board running as a desktop. The
BC250 is fed from a PCI-E connector and has no ATX power button, so this firmware
drives the SFX PSU's `PS_ON#` line and senses board power, giving you a real power
button — plus optional "turn on when I pick up my controller" via Bluetooth.

## Features

- **Push-button power**: tap to turn on; hold 5 s while running to force off.
- **Follows the board**: if the OS shuts the board down, the PSU is cut automatically.
- **Boot watchdog**: if the board doesn't come up within 10 s, the PSU is released.
- **BLE controller wake** (optional): when a bound controller (e.g. an 8BitDo) powers
  on, the machine powers on with it.
- **WiFi setup portal**: configure the bound controller from a phone — no reflashing.

## Wiring

The ESP32-C3 is permanently powered from the ATX connector's **5 V standby**, so it
runs whether the machine is on or off. Share a common ground between the ESP, the PSU,
and the board.

| ESP32-C3 | Connects to | Notes |
|----------|-------------|-------|
| GPIO5 | Momentary switch, terminal A | Read with internal pull-up |
| GPIO6 | Momentary switch, terminal B | Driven LOW as the switch's ground |
| GPIO4 | ATX `PS_ON#` (green wire) | **Open-drain**, active LOW: LOW = PSU on, released = off |
| GPIO3 | BC250 `TPMS1` (pin 9) | ~3.3 V when the board is up, 0 when off |
| 5VSB / GND | PSU standby + common ground | Permanent power for the ESP |

`PS_ON#` idles at ~5 V (pulled up inside the PSU). GPIO4 is driven open-drain so the
3.3 V part never fights the 5 V rail — it only ever sinks to ground to switch the PSU on.

`TPMS1` is a higher-impedance signal that hovers near the logic threshold, so it's read
as an analog voltage with hysteresis rather than a digital pin.

## Button controls

| Action | Result |
|--------|--------|
| Tap while **off** | Power on |
| Hold ≥ 5 s while **on** | Force power off |
| Hold ≥ 8 s while **off** | Enter WiFi setup portal |

The button is the primary control and always works, even with no controller configured.

## Bluetooth controller wake

When a controller is bound (via the portal), the machine **follows the controller**:
turn the controller on and the machine powers up. After a power-off there's a short
guard window so the controller's reconnect burst can't immediately switch it back on —
turn the controller off within that window to keep the machine down.

## Setup portal

Hold the button ≥ 8 s while off (or on first use) to start the portal:

1. Connect to the open WiFi network **`BC250 Switch Setup`** and open `http://192.168.4.1`.
2. Create a password.
3. Pick your controller from the live BLE scan (or enter its MAC).
4. Finish — the device reboots into normal operation.

## Build & flash

PlatformIO (pioarduino). Two steps — firmware and the portal's web UI (a single
`app/index.html` packed into SPIFFS):

```bash
pio run -e esp32-c3-devkitm-1 -t upload     # firmware
pio run -e esp32-c3-devkitm-1 -t uploadfs   # web UI filesystem
```

A second `scanner` environment flashes a standalone BLE scanner (`src/ble_scan.cpp`)
for discovering a controller's advertisement details:

```bash
pio run -e scanner -t upload && pio device monitor
```

## Notes

- **WiFi TX power**: these ESP32-C3 *mini* boards have an RF/power quirk
  ([arduino-esp32 #6551](https://github.com/espressif/arduino-esp32/issues/6551)) where
  the SoftAP is invisible at full power. The portal sets `WIFI_POWER_8_5dBm`
  (`AP_TX_POWER` in [include/board.h](include/board.h)) to work around it.
- Serial debug runs over USB-CDC at **115200** baud.
- Pin assignments and all timing constants live in [include/board.h](include/board.h).
