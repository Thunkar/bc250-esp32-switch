#include "config.h"
#include <Preferences.h>

Config config{"", "", false};

static Preferences prefs;
static const char *NS = "bc250";

void loadConfig() {
  prefs.begin(NS, true);  // read-only
  config.wakeAddr   = prefs.getString("wakeAddr", "");
  config.passHash   = prefs.getString("passHash", "");
  config.forceSetup = prefs.getBool("forceSetup", false);
  prefs.end();
}

static void putString(const char *key, const String &val) {
  prefs.begin(NS, false);
  prefs.putString(key, val);
  prefs.end();
}

void setWakeAddr(const String &addr) {
  config.wakeAddr = addr;
  putString("wakeAddr", addr);
}

void setPassHash(const String &hash) {
  config.passHash = hash;
  putString("passHash", hash);
}

void setForceSetup(bool force) {
  config.forceSetup = force;
  prefs.begin(NS, false);
  prefs.putBool("forceSetup", force);
  prefs.end();
}

bool isConfigured() {
  return config.passHash.length() > 0 && config.wakeAddr.length() > 0;
}
