#include <Arduino.h>
#include "DeviceIdentity.h"
#include "WiFiManagerAP.h"
#include "OTAManager.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0"   // Se define en platformio.ini (build_flags)
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "Central"      // Se define en platformio.ini (build_flags)
#endif

#ifndef GITHUB_REPO
#define GITHUB_REPO "jota0018/LEGO_MOD-firmware"  // Se define en platformio.ini
#endif

// ============================================================================
// CONFIGURACIÓN
// ============================================================================
#define LED_STATUS       12
#define BUTTON_PIN       27
#define BUTTON_TIMEOUT   3000   // Mantener 3 s      → Activar AP
#define CLICK_MAX_MS     600    // Click corto        → menos de 600 ms
#define TRIPLE_CLICK_MS  1500   // 3 clicks en 1.5 s  → Buscar OTA
#define DEBOUNCE_MS      40

#ifndef GITHUB_OWNER
#define GITHUB_OWNER "jota0018"    // Se define en platformio.ini (build_flags)
#endif

// GITHUB_REPO viene del ini (build_flags)

// ============================================================================
// COMPONENTES LEGO
// ============================================================================
DeviceIdentity identity(DEVICE_NAME);
WiFiManagerAP  wifiManager(LED_STATUS, BUTTON_PIN, &identity);
OTAManager     ota(LED_STATUS);

// Estado del botón
unsigned long buttonPressTime = 0;
bool          buttonPressed   = false;
int           clickCount      = 0;
unsigned long firstClickTime  = 0;

// El core de Arduino confirma un firmware nuevo automáticamente al arrancar.
// Devolviendo true le decimos que NO lo haga: lo confirma OTAManager tras
// 20 s corriendo estable. Si se cuelga antes, el bootloader vuelve solo
// a la versión anterior.
bool verifyRollbackLater() { return true; }

void monitorButton();
void runOTA();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("🚀 INICIANDO SISTEMA");
  Serial.printf("   Firmware: v%s\n", FIRMWARE_VERSION);
  Serial.printf("   Dispositivo: %s\n", DEVICE_NAME);
  Serial.println("=================================\n");

  identity.begin();
  wifiManager.begin(DEVICE_NAME);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  ota.begin(GITHUB_OWNER, GITHUB_REPO, FIRMWARE_VERSION);
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  wifiManager.handle();
  ota.handle();
  monitorButton();
}

// ============================================================================
// FUNCIONES
// ============================================================================

void runOTA() {
  if (wifiManager.isActive()) wifiManager.deactivate();
  ota.checkAndUpdate(wifiManager.getSSID(), wifiManager.getPass());
  // Si hubo actualización, el dispositivo ya se reinició.
  // Si no, seguimos operando normal.
}

void monitorButton() {
  int buttonState = digitalRead(BUTTON_PIN);

  // ---- Botón presionado (LOW) ----
  if (buttonState == LOW) {
    if (!buttonPressed) {
      buttonPressed   = true;
      buttonPressTime = millis();
    }
    // Mantenido 3 s → AP
    if (!wifiManager.isActive() && (millis() - buttonPressTime >= BUTTON_TIMEOUT)) {
      Serial.println("✅ Botón 3+ segundos → Activando AP");
      wifiManager.activate();
      clickCount = 0;
    }
  }
  // ---- Botón liberado (HIGH) ----
  else if (buttonPressed) {
    buttonPressed = false;
    unsigned long pressDuration = millis() - buttonPressTime;

    // Click corto (con debounce)
    if (pressDuration >= DEBOUNCE_MS && pressDuration < CLICK_MAX_MS) {
      // Si pasó mucho tiempo desde el primer click, empezar de nuevo
      if (clickCount == 0 || millis() - firstClickTime > TRIPLE_CLICK_MS) {
        clickCount     = 0;
        firstClickTime = millis();
      }
      clickCount++;
      Serial.printf("🔘 Click %d/3\n", clickCount);

      if (clickCount >= 3) {
        clickCount = 0;
        Serial.println("✅ Triple click → Buscando actualización OTA");
        runOTA();
      }
    }
  }
}