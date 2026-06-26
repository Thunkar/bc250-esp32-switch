#include <Arduino.h>
#include "board.h"

// The ESP32-C3 is permanently powered from the ATX connector (5VSB), so it runs
// independently of the PSU's main rails. It controls the SFX PSU through PS_ON#
// and watches the BC250's TPMS1 line to know whether the board itself is up.
//
// Control model
// -------------
//   * Asserting PS_ON# turns the PSU on; the BC250 is expected to power up from
//     applied power (BIOS "restore on AC power"), so PSU on == board boots.
//   * We have no wire to the board's power button, so "turning the board off"
//     means cutting PSU power via PS_ON#. This is a hard power-off, not a
//     graceful OS shutdown.
//   * If the board shuts itself down (e.g. OS shutdown), TPMS1 drops to 0 while
//     the PSU is still energized. We detect that and release PS_ON# so the PSU
//     follows the board down.

enum PowerState {
  STATE_OFF,      // PSU released, board down
  STATE_BOOTING,  // PSU asserted, waiting for TPMS1 to go HIGH
  STATE_ON,       // PSU asserted, board up (TPMS1 HIGH)
};

static PowerState state = STATE_OFF;

// --- Button debounce / press tracking ---
static bool          buttonStable      = false;  // debounced "pressed" level
static bool          buttonLastRaw     = false;
static unsigned long buttonLastChange  = 0;
static unsigned long pressStart        = 0;
static bool          longPressFired    = false;

// --- Board-sense debounce ---
static bool          boardSenseStable  = false;  // debounced TPMS1 HIGH
static bool          boardSenseLastRaw = false;
static unsigned long boardSenseChange  = 0;

// --- Misc timers ---
static unsigned long bootStart    = 0;
static unsigned long lastHeartbeat = 0;

static const char *stateName(PowerState s) {
  switch (s) {
    case STATE_OFF:     return "OFF";
    case STATE_BOOTING: return "BOOTING";
    case STATE_ON:      return "ON";
  }
  return "?";
}

static void setState(PowerState next) {
  if (next == state) return;
  Serial.printf("[STATE] %s -> %s\n", stateName(state), stateName(next));
  state = next;
}

// Raw (pre-time-debounce) board-up level, derived from the TPMS1 voltage with
// hysteresis so a signal hovering near the logic threshold doesn't chatter.
static bool senseLevel = false;

static bool readBoardSense(uint32_t *outMv = nullptr) {
  uint32_t mv = analogReadMilliVolts(BOARD_SENSE);
  if (outMv) *outMv = mv;
  if (senseLevel) {
    if (mv < SENSE_LOW_MV) senseLevel = false;
  } else {
    if (mv > SENSE_HIGH_MV) senseLevel = true;
  }
  return senseLevel;
}

static void psuOn() {
  digitalWrite(PS_ON_PIN, PS_ON_ASSERT);
  Serial.println("[PSU ] PS_ON# asserted (LOW) -> PSU ON");
}

static void psuOff() {
  digitalWrite(PS_ON_PIN, PS_ON_RELEASE);
  Serial.println("[PSU ] PS_ON# released (high-Z) -> PSU OFF");
}

// Debounce a raw level into a stable level. Returns true if the stable value
// changed this call, writing the new value into *stable.
static bool debounce(bool raw, bool *stable, bool *lastRaw,
                     unsigned long *lastChange, unsigned long now,
                     unsigned long window) {
  if (raw != *lastRaw) {
    *lastRaw = raw;
    *lastChange = now;
  }
  if (raw != *stable && (now - *lastChange) >= window) {
    *stable = raw;
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  // Give USB-CDC a moment to enumerate so early logs aren't lost.
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 2000) {
    delay(10);
  }

  Serial.println();
  Serial.println("=== BC250 PSU controller ===");

  // PS_ON# open-drain, released by default so the PSU stays off at boot.
  pinMode(PS_ON_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(PS_ON_PIN, PS_ON_RELEASE);

  // Switch: GPIO6 = local ground, GPIO5 = sensed input with pull-up.
  pinMode(BUTTON_GND, OUTPUT);
  digitalWrite(BUTTON_GND, LOW);
  pinMode(BUTTON_SENSE, INPUT_PULLUP);

  // TPMS1 sense: read as ADC over the full 0-3.3V range.
  analogSetPinAttenuation(BOARD_SENSE, ADC_11db);

  // Seed debounced states from the current levels.
  buttonStable = buttonLastRaw = (digitalRead(BUTTON_SENSE) == LOW);
  boardSenseStable = boardSenseLastRaw = readBoardSense();

  // If the board is already up when the ESP (re)boots, adopt the ON state
  // rather than assuming OFF — keeps us in sync after an ESP-only reset.
  if (boardSenseStable) {
    psuOn();  // make sure PS_ON# matches reality
    setState(STATE_ON);
  }

  Serial.printf("[INIT] state=%s board=%s\n",
                stateName(state), boardSenseStable ? "UP" : "DOWN");
}

void loop() {
  unsigned long now = millis();

  // --- Sample & debounce inputs ---
  uint32_t senseMv;
  bool buttonRaw = (digitalRead(BUTTON_SENSE) == LOW);   // pressed == LOW
  bool boardRaw  = readBoardSense(&senseMv);             // board up (hysteresis)

  if (debounce(buttonRaw, &buttonStable, &buttonLastRaw,
               &buttonLastChange, now, DEBOUNCE_MS)) {
    if (buttonStable) {
      // Press began.
      pressStart = now;
      longPressFired = false;
      Serial.println("[BTN ] pressed");

      // Power on the moment the button goes down while OFF, so it responds to a
      // tap and doesn't depend on how long the button is held.
      if (state == STATE_OFF) {
        Serial.println("[ACT ] press while OFF -> powering on");
        psuOn();
        bootStart = now;
        setState(STATE_BOOTING);
      }
    } else {
      // Released.
      unsigned long held = now - pressStart;
      Serial.printf("[BTN ] released after %lu ms\n", held);
    }
  }

  bool boardChanged = debounce(boardRaw, &boardSenseStable, &boardSenseLastRaw,
                               &boardSenseChange, now,
                               state == STATE_BOOTING ? DEBOUNCE_MS
                                                      : BOARD_OFF_DEBOUNCE_MS);

  // --- Long-press-to-force-off while ON ---
  if (buttonStable && !longPressFired && state == STATE_ON &&
      (now - pressStart) >= LONG_PRESS_MS) {
    longPressFired = true;
    Serial.println("[ACT ] long press (>5s) while ON -> forcing power off");
    psuOff();
    setState(STATE_OFF);
  }

  // --- State-driven board-sense handling ---
  switch (state) {
    case STATE_BOOTING:
      if (boardSenseStable) {
        Serial.println("[ACT ] TPMS1 HIGH -> board is up");
        setState(STATE_ON);
      } else if ((now - bootStart) >= BOOT_TIMEOUT_MS) {
        Serial.println("[ACT ] boot timed out, board never signalled UP -> "
                       "releasing PSU, returning to idle");
        psuOff();
        setState(STATE_OFF);
      }
      break;

    case STATE_ON:
      // Board dropped TPMS1 on its own (OS shutdown / crash) -> follow it down.
      if (boardChanged && !boardSenseStable) {
        Serial.println("[ACT ] TPMS1 LOW while ON -> board shut down, "
                       "releasing PSU");
        psuOff();
        setState(STATE_OFF);
      }
      break;

    case STATE_OFF:
    default:
      break;
  }

  // --- Heartbeat ---
  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = now;
    Serial.printf("[HB  ] state=%s board=%s btn=%s | sense: %umV (%s)\n",
                  stateName(state),
                  boardSenseStable ? "UP" : "DOWN",
                  buttonStable ? "down" : "up",
                  senseMv, boardRaw ? "high" : "low");
  }
}
