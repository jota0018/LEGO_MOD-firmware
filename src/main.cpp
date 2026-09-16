#include <Arduino.h>
#include "DeviceIdentity.h"
#include "WiFiManagerAP.h"

// ============================================================================
// CONFIGURACIÓN
// ============================================================================
#define LED_STATUS       12
#define BUTTON_PIN       27
#define BUTTON_TIMEOUT   3000  // 3 segundos

// ============================================================================
// VARIABLES GLOBALES - Componentes LEGO
// ============================================================================
DeviceIdentity identity;
WiFiManagerAP wifiManager(LED_STATUS, BUTTON_PIN, &identity);

unsigned long buttonPressTime = 0;
bool buttonPressed = false;

void monitorButton() {
  int buttonState = digitalRead(BUTTON_PIN);

  // Botón presionado (LOW)
  if (buttonState == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressTime = millis();
      Serial.println("🔘 Botón presionado...");
    }

    // Detectar si se mantuvo presionado 3 segundos
    if (!wifiManager.isActive() && (millis() - buttonPressTime >= BUTTON_TIMEOUT)) {
      Serial.println("✅ Botón presionado 3+ segundos → Activando AP");
      wifiManager.activate();
    }
  }
  // Botón liberado (HIGH)
  else {
    if (buttonPressed) {
      unsigned long pressDuration = millis() - buttonPressTime;
      Serial.printf("🔘 Botón liberado (duración: %lu ms)\n", pressDuration);
      buttonPressed = false;
    }
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("🚀 INICIANDO SISTEMA");
  Serial.println("=================================\n");

  // Inicializar componentes LEGO en orden
  identity.begin();          // Primero identidad
  wifiManager.begin("Central");  // Luego WiFiManager
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  // Manejar el WiFiManager (AP, LED, timeout)
  wifiManager.handle();

  // Monitorear botón para activar/desactivar AP
  monitorButton();
}

// ============================================================================
// FUNCIONES
// ============================================================================

