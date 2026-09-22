# BC250 ESP32 Power Switch

An ESP32-C3 power controller for an AMD **BC250** board running as a desktop. The
BC250 is fed from a PCI-E connector and has no ATX power button, so this firmware
drives the SFX PSU's `PS_ON#` line, senses board power, and can pulse the
motherboard's power-button pads for graceful ACPI shutdown — plus optional
"turn on when I pick up my controller" via Bluetooth.

## Features

- **Push-button power**: tap to turn on; tap while running for graceful ACPI shutdown;
  hold 5 s to force off.
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
| GPIO7 | BC250 power-button solder pads | **Open-drain**, 200 ms LOW pulse = ACPI power button press |
| 5VSB / GND | PSU standby + common ground | Permanent power for the ESP |

`PS_ON#` idles at ~5 V (pulled up inside the PSU). GPIO4 is driven open-drain so the
3.3 V part never fights the 5 V rail — it only ever sinks to ground to switch the PSU on.

`TPMS1` is a higher-impedance signal that hovers near the logic threshold, so it's read
as an analog voltage with hysteresis rather than a digital pin.

### Connector pinouts

**ATX 24-pin main connector** — tap three pins:

```
               +3.3V ─┤  1 │ 13 ├─ +3.3V
               +3.3V ─┤  2 │ 14 ├─ −12V
                 GND ─┤  3 │ 15 ├─ GND
                 +5V ─┤  4 │ 16 ├─ PS_ON#   ◄── GPIO4  (green, open-drain, active LOW)
                 GND ─┤  5 │ 17 ├─ GND      ◄── ESP GND (any GND pin works)
                 +5V ─┤  6 │ 18 ├─ GND
                 GND ─┤  7 │ 19 ├─ GND
              PWR_OK ─┤  8 │ 20 ├─ (RSVD)
ESP 5V/VIN ◄── +5VSB ─┤  9 │ 21 ├─ +5V
                +12V ─┤ 10 │ 22 ├─ +5V
                +12V ─┤ 11 │ 23 ├─ +5V
               +3.3V ─┤ 12 │ 24 ├─ GND
```

**TPMS1 header** — single pin for board-power sense:

```
   PCICLK ─┤  1   2 ├─ GND
    FRAME ─┤  3   4 ├─ SMB_CLK_MAIN
  PCIRST# ─┤  5   6 ├─ SMB_DATA_MAIN
     LAD3 ─┤  7   8 ├─ LAD2
       3V ─┤  9  10 ├─ LAD1      ◄── pin 9 (3V) = board-on sense ──► GPIO3
     LAD0 ─┤ 11  12 ├─ GND
          ─┤     14 ├─ S_PWRDWN#
     3VSB ─┤ 15  16 ├─ SERIRQ#
      GND ─┤ 17  18 ├─ GND
```

Pin 9 is the only TPMS1 pin used: it reads ~3.3 V when the board is powered and 0 V when
off. No ground wire is needed from this header — the ESP already shares ground with the
board through the ATX connector.

### Motherboard power button (GPIO7)

The BC250 has power/reset button solder pads on the **bottom side** of the board. GPIO7
connects to the power button pad pair — solder a wire to one pad and share the common
ground. The ESP pulses GPIO7 LOW for 200 ms (open-drain) to simulate pressing a physical
power button, triggering a graceful ACPI shutdown through the OS.

To locate the pads: flip the BC250 over and find the grid of small solder pads near the
edge — see the
[BC250 PSU adapter wiring diagram](https://github.com/mosfetparty/bc250-psu-adapter/blob/2e8af98586867503f02431941fe0897930b185ee/FSP500-30AS%20%E2%80%94%20Plug-n-Play%20Edition/Wiring%20Diagram/FSP500%20PnP%20-%20BC250%20PSU%20Adapter.pdf)
by mosfetparty for help identifying and soldering to these pads.

## Button controls

| Action | Result |
|--------|--------|
| Tap while **off** | Power on |
| Tap while **on** | Graceful ACPI shutdown (200 ms pulse on GPIO7) |
| Hold ≥ 5 s while **on** | Force power off (hard PSU cutoff) |
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
pio run -t upload     # firmware
pio run -t uploadfs   # web UI filesystem
```

## Notes

- **WiFi TX power**: these ESP32-C3 *mini* boards have an RF/power quirk
  ([arduino-esp32 #6551](https://github.com/espressif/arduino-esp32/issues/6551)) where
  the SoftAP is invisible at full power. The portal sets `WIFI_POWER_8_5dBm`
  (`AP_TX_POWER` in [include/board.h](include/board.h)) to work around it.
- Serial debug runs over USB-CDC at **115200** baud.
- Pin assignments and all timing constants live in [include/board.h](include/board.h).
