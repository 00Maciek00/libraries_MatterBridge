// =============================================================================
//  MatterBridge — Example 01: On/Off Light
//  MatterBridge — Przykład 01: Lampa On/Off
// =============================================================================
//
//  The simplest MatterBridge sketch: one virtual On/Off light. When you
//  switch it in your smart-home app, onHubChange() runs and the LED follows.
//  Najprostszy szkic MatterBridge: jedna wirtualna lampa On/Off. Po
//  przełączeniu w aplikacji smart-home wywoływane jest onHubChange(),
//  a dioda LED podąża za stanem.
//
//  Board / Płytka: ESP32-C6 or/lub ESP32-S3, Arduino-ESP32 3.x
//  Partition scheme / Schemat partycji: e.g./np. "Huge APP"
//  Commission over Bluetooth (see Serial Monitor).
//  Komisjonuj przez Bluetooth (patrz Serial Monitor).
//
//  S.M. DIY Home | https://github.com/00Maciek00/libraries_MatterBridge
// =============================================================================

#include <MatterBridge.h>

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2   // change to your LED pin / zmień na pin swojej LED
#endif

static uint8_t lightSlot = MB_INVALID_SLOT;

// Called when the hub changes the state (or you call setState()).
// Wywoływane gdy hub zmienia stan (lub gdy wołasz setState()).
static void onHubChange(uint8_t slot, bool state) {
  Serial.printf("slot %u -> %s\n", slot, state ? "ON" : "OFF");
  if (slot == lightSlot) digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
}

static void printPairingInfo() {
  if (MatterBridge.isCommissioned()) {
    Serial.println("Already commissioned / Juz skomisjonowany");
    return;
  }
  char code[16] = {0};
  char qr[128]  = {0};
  if (MatterBridge.getPairingCode(code, sizeof(code))) Serial.printf("Pairing code / Kod parowania: %s\n", code);
  if (MatterBridge.getQRCode(qr, sizeof(qr)))          Serial.printf("QR payload: %s\n", qr);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);

  // Devices are created in setup() on every boot, so do not persist them.
  // Urządzenia tworzymy w setup() przy każdym starcie, więc nie zapisujemy ich.
  MatterBridge.setRuntimeNVS(false);          // BEFORE begin() / PRZED begin()

  if (!MatterBridge.begin()) {                // node + aggregator / węzeł + agregator
    Serial.println("begin() failed");
    while (true) delay(1000);
  }

  lightSlot = MatterBridge.addOnOffLightRuntime("Lamp");
  MatterBridge.onChange(onHubChange);

  if (!MatterBridge.start()) {                // start Matter / uruchom Matter
    Serial.println("start() failed");
    while (true) delay(1000);
  }

  printPairingInfo();
  MatterBridge.printStatus();
}

void loop() {
  MatterBridge.update();                      // GPIO debounce, ticks / debounce GPIO
  MatterBridge.checkBoot();                   // hold BOOT 5 s = factory reset
                                              // przytrzymaj BOOT 5 s = factory reset
}
