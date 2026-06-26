#pragma once

#include <Arduino.h>

// Persistent configuration, stored in NVS via the Preferences library.
//   wakeAddr   - bound controller BLE MAC, lower-case colon form ("aa:bb:..").
//   passHash   - SHA-256 hex of the portal password ("" => not set yet).
//   forceSetup - request that the next boot enters the WiFi setup portal.
struct Config {
  String wakeAddr;
  String passHash;
  bool   forceSetup;
};

extern Config config;

void loadConfig();

void setWakeAddr(const String &addr);
void setPassHash(const String &hash);
void setForceSetup(bool force);

// Fully provisioned: a password has been set and a controller is bound.
bool isConfigured();
