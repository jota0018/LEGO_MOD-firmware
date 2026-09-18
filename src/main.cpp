#include <Arduino.h>
#include <WiFi.h>
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
// CONEXIÓN A INTERNET (no bloqueante)
// ============================================================================
#define WIFI_CONNECT_TIMEOUT_MS  10000   // máx esperando WL_CONNECTED tras WiFi.begin()
#define WIFI_RETRY_BASE_MS       5000    // backoff base
#define WIFI_RETRY_MAX_MS        60000   // backoff tope

enum NetState { NET_IDLE, NET_CONNECTING, NET_CONNECTED, NET_DISCONNECTED };

NetState      netState           = NET_IDLE;
unsigned long netStateChangedAt  = 0;
unsigned long lastConnectAttempt = 0;
uint8_t       reconnectAttempts  = 0;

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
void checkInternetConnection();
void updateLED();
unsigned long currentBackoff();

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

  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW);   // arranca apagado; updateLED() es el único que lo toca de ahí en más

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
  checkInternetConnection();
  updateLED();   // único punto del programa que escribe LED_STATUS fuera de AP/OTA
}

// ============================================================================
// FUNCIONES
// ============================================================================

void runOTA() {
  if (wifiManager.isActive()) wifiManager.deactivate();
  ota.checkAndUpdate(wifiManager.getSSID(), wifiManager.getPass());
  // Si hubo actualización, el dispositivo ya se reinició.
  // Si no, seguimos operando normal. (checkAndUpdate() es bloqueante:
  // mientras corre, nadie más toca el LED, así que su parpadeo rápido
  // no compite con updateLED().)
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

// Backoff exponencial (5s, 10s, 20s, 40s ... tope 60s)
unsigned long currentBackoff() {
  unsigned long backoff = WIFI_RETRY_BASE_MS * (1UL << min(reconnectAttempts, (uint8_t)6));
  return min(backoff, (unsigned long)WIFI_RETRY_MAX_MS);
}

// Máquina de estados no bloqueante: nunca usa while/delay para esperar
// la conexión. Solo actualiza netState — NO toca el LED (eso es trabajo
// exclusivo de updateLED()).
void checkInternetConnection() {
  // Si el AP de configuración está activo, no tocamos el STA
  if (wifiManager.isActive()) {
    if (netState != NET_IDLE) {
      Serial.println("📡 AP activo → pausando conexión a internet");
      netState = NET_IDLE;
    }
    return;
  }

  // ---- Conectado ----
  if (WiFi.status() == WL_CONNECTED) {
    if (netState != NET_CONNECTED) {
      netState = NET_CONNECTED;
      reconnectAttempts = 0;
      Serial.println("✅ Internet conectado");
      Serial.printf("   IP: %s | RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    return;
  }

  String ssid = wifiManager.getSSID();
  String pass = wifiManager.getPass();
  if (ssid.length() == 0) {
    if (netState != NET_IDLE) {
      Serial.println("⚠️ Sin credenciales WiFi guardadas");
      netState = NET_IDLE;
    }
    return;
  }

  unsigned long now = millis();

  switch (netState) {
    case NET_CONNECTED:
      Serial.println("❌ Se perdió la conexión a internet");
      netState = NET_DISCONNECTED;
      lastConnectAttempt = 0;  // fuerza reintento inmediato
      break;

    case NET_CONNECTING:
      if (now - netStateChangedAt >= WIFI_CONNECT_TIMEOUT_MS) {
        reconnectAttempts++;
        Serial.printf("⏱️ Timeout conectando (intento #%d)\n", reconnectAttempts);
        netState = NET_DISCONNECTED;
        lastConnectAttempt = now;
      }
      break;

    case NET_IDLE:
    case NET_DISCONNECTED:
    default:
      if (lastConnectAttempt == 0 || now - lastConnectAttempt >= currentBackoff()) {
        Serial.printf("🔄 Conectando a WiFi \"%s\"...\n", ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        lastConnectAttempt = now;
        netStateChangedAt  = now;
        netState = NET_CONNECTING;
      }
      break;
  }
}

// Único punto del programa (fuera del bloqueo de OTA) que escribe LED_STATUS.
// Prioridad: AP activo (parpadeo 500/500) > estado de conexión (sólido on/off).
void updateLED() {
  if (wifiManager.isActive()) {
    static unsigned long lastToggle = 0;
    static bool ledOn = false;
    if (millis() - lastToggle >= 500) {
      lastToggle = millis();
      ledOn = !ledOn;
      digitalWrite(LED_STATUS, ledOn ? HIGH : LOW);
    }
    return;
  }

  digitalWrite(LED_STATUS, (netState == NET_CONNECTED) ? HIGH : LOW);
}