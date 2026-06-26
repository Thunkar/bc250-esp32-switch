#pragma once

// WiFi setup portal (setup mode only). Brings up an open SoftAP + captive
// portal that forces the user to set a password and then bind a BLE controller.
void portalBegin();
void portalLoop();
