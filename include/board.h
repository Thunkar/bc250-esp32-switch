#pragma once

//*******  Pin definitions  ***************
//
// BC250 PSU controller wiring:
//
//   ESP32-C3                      External
//   --------                      --------
//   GPIO5  (BUTTON_SENSE)  <-----> momentary switch terminal A
//   GPIO6  (BUTTON_GND)    <-----> momentary switch terminal B
//   GPIO4  (PS_ON_PIN)     <-----> ATX PS_ON# (green wire, active LOW)
//   GPIO3  (BOARD_SENSE)   <-----> BC250 TPMS1 pin 9 (3.3V = board on)
//
// The switch bridges GPIO5 and GPIO6. GPIO6 is driven LOW to act as a local
// ground, and GPIO5 is read with an internal pull-up: pressed reads LOW.
const int BUTTON_SENSE = 5;
const int BUTTON_GND   = 6;

// ATX PS_ON# is active LOW and idles at ~5V (pulled up inside the PSU).
// Driven as OPEN-DRAIN so we never push 3.3V against the PSU's 5V pull-up:
//   LOW  -> sink to GND -> PSU on
//   HIGH -> high-impedance -> PSU pull-up wins -> PSU off
const int PS_ON_PIN = 4;

// BC250 TPMS1 (pin 9): reads ~3.3V while the board is powered/booted, 0 when
// off. In practice it's a higher-impedance source that settles near ~2.9V and
// hovers close to the ESP's digital logic threshold, so digitalRead() flickers.
// We read it as an ADC voltage with hysteresis instead (see thresholds below).
const int BOARD_SENSE = 3;

// Hysteresis thresholds for the analog board-sense reading. The gap between
// them keeps a noisy signal sitting near the threshold from chattering:
//   reading rises above HIGH -> treat as "board up"
//   reading falls below LOW  -> treat as "board down"
//   in between               -> hold previous state
const int SENSE_HIGH_MV = 2000;
const int SENSE_LOW_MV  = 800;

//*******  Logic levels  ***************

const int PS_ON_ASSERT  = LOW;   // PSU on
const int PS_ON_RELEASE = HIGH;  // PSU off (open-drain -> high-Z)

//*******  Timing (milliseconds)  ***************

// Switch debounce window.
const unsigned long DEBOUNCE_MS = 30;

// Hold the button this long while the board is ON to force it off.
const unsigned long LONG_PRESS_MS = 5000;

// TPMS1 must stay LOW continuously for this long before we treat the board as
// having shut itself down. Filters out brief dips/transients during boot/reset.
const unsigned long BOARD_OFF_DEBOUNCE_MS = 1500;

// How long to wait for TPMS1 to go HIGH after asserting PS_ON#. If the board
// hasn't signalled UP by then we assume the boot failed, release the PSU and
// return to idle (OFF).
const unsigned long BOOT_TIMEOUT_MS = 10000;

// Periodic heartbeat log interval.
const unsigned long HEARTBEAT_MS = 1000;
