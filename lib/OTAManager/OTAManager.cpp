#include "OTAManager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "esp_ota_ops.h"
#include "mbedtls/sha256.h"

#define OTA_WIFI_TIMEOUT_MS   15000   // Espera máxima para conectar WiFi
#define OTA_HTTP_TIMEOUT_MS   30000   // Timeout HTTP
#define OTA_STALL_TIMEOUT_MS  15000   // Sin datos durante la descarga → abortar
#define OTA_CONFIRM_AFTER_MS  20000   // Firmware nuevo se confirma tras 20 s estable

// ============================================================================
// PUBLIC
// ============================================================================

OTAManager::OTAManager(int ledStatusPin) : _ledPin(ledStatusPin) {}

void OTAManager::begin(const char* githubOwner, const char* githubRepo, const char* currentVersion) {
  _owner   = githubOwner;
  _repo    = githubRepo;
  _version = currentVersion;

  String base   = "https://github.com/" + _owner + "/" + _repo + "/releases/latest/download/";
  _manifestUrl  = base + "version.json";
  _firmwareUrl  = base + "firmware.bin";

  // ¿Este firmware acaba de instalarse por OTA y está "en prueba"?
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  _pendingVerify = (esp_ota_get_state_partition(running, &state) == ESP_OK &&
                    state == ESP_OTA_IMG_PENDING_VERIFY);

  Serial.println("📦 OTAManager listo");
  Serial.printf("   ► Firmware:  v%s\n", _version.c_str());
  Serial.printf("   ► Partición: %s\n", running->label);
  Serial.printf("   ► Repo:      %s/%s\n", _owner.c_str(), _repo.c_str());
  if (_pendingVerify) {
    Serial.println("   ► ⚠️  Firmware NUEVO en prueba: se confirma en 20 s si corre estable");
  }
}

void OTAManager::handle() {
  if (_pendingVerify && millis() > OTA_CONFIRM_AFTER_MS) {
    confirmBoot();
  }
}

void OTAManager::confirmBoot() {
  if (!_pendingVerify) return;
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
    Serial.printf("✅ [OTA] Firmware v%s confirmado como válido (rollback cancelado)\n", _version.c_str());
  }
  _pendingVerify = false;
}

String OTAManager::getVersion() {
  return _version;
}

bool OTAManager::checkAndUpdate(const String& ssid, const String& pass) {
  Serial.println("\n🔄 [OTA] Buscando actualización...");

  if (ssid.isEmpty()) {
    Serial.println("❌ [OTA] Sin credenciales WiFi. Configura primero por el portal AP.");
    return false;
  }

  if (!_connectWiFi(ssid, pass)) {
    _wifiOff();
    return false;
  }

  String remoteVersion, sha256;
  size_t size = 0;
  if (!_fetchManifest(remoteVersion, sha256, size)) {
    _wifiOff();
    return false;
  }

  Serial.printf("   ► Actual: v%s | Disponible: v%s\n", _version.c_str(), remoteVersion.c_str());

  if (_compareSemver(remoteVersion, _version) <= 0) {
    Serial.println("✅ [OTA] Ya tienes la última versión.\n");
    _wifiOff();
    return false;
  }

  Serial.printf("⬇️  [OTA] Descargando v%s (%u bytes)...\n", remoteVersion.c_str(), (unsigned)size);

  if (!_downloadAndFlash(sha256, size)) {
    Serial.println("❌ [OTA] Actualización cancelada. Firmware actual intacto.\n");
    _wifiOff();
    return false;
  }

  Serial.printf("✅ [OTA] v%s instalada. Reiniciando en 2 s...\n", remoteVersion.c_str());
  delay(2000);
  ESP.restart();
  return true;
}

// ============================================================================
// PRIVATE - WiFi
// ============================================================================

bool OTAManager::_connectWiFi(const String& ssid, const String& pass) {
  Serial.printf("📶 [OTA] Conectando a %s...\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > OTA_WIFI_TIMEOUT_MS) {
      Serial.println("❌ [OTA] No se pudo conectar a WiFi.");
      return false;
    }
    digitalWrite(_ledPin, !digitalRead(_ledPin));
    delay(250);
  }

  Serial.printf("📶 [OTA] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void OTAManager::_wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// ============================================================================
// PRIVATE - Manifest (version.json)
// ============================================================================

bool OTAManager::_fetchManifest(String& remoteVersion, String& sha256, size_t& size) {
  WiFiClientSecure client;
  client.setInsecure();  // TLS sin validar CA. Integridad garantizada por SHA256.
                         // Endurecer después: client.setCACert(GITHUB_ROOT_CA);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // GitHub redirige a su CDN
  http.setTimeout(OTA_HTTP_TIMEOUT_MS);

  if (!http.begin(client, _manifestUrl)) {
    Serial.println("❌ [OTA] No se pudo iniciar conexión HTTP.");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("❌ [OTA] version.json → HTTP %d (¿existe un release publicado?)\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("❌ [OTA] version.json inválido.");
    return false;
  }

  remoteVersion = doc["version"] | "";
  sha256        = doc["sha256"]  | "";
  size          = doc["size"]    | 0;

  if (remoteVersion.isEmpty() || sha256.length() != 64) {
    Serial.println("❌ [OTA] version.json incompleto (version / sha256).");
    return false;
  }
  return true;
}

// ============================================================================
// PRIVATE - Descarga + SHA256 + Flash
// ============================================================================

bool OTAManager::_downloadAndFlash(const String& sha256Expected, size_t expectedSize) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(OTA_HTTP_TIMEOUT_MS);

  if (!http.begin(client, _firmwareUrl)) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("❌ [OTA] firmware.bin → HTTP %d\n", code);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("❌ [OTA] Tamaño de firmware desconocido.");
    http.end();
    return false;
  }
  if (expectedSize > 0 && (size_t)contentLength != expectedSize) {
    Serial.printf("❌ [OTA] Tamaño no coincide (manifest %u, servidor %d).\n", (unsigned)expectedSize, contentLength);
    http.end();
    return false;
  }

  if (!Update.begin(contentLength, U_FLASH)) {
    Serial.printf("❌ [OTA] No hay espacio en la partición OTA (%s).\n", Update.errorString());
    http.end();
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA-256

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t written = 0;
  int lastPercent = -1;
  unsigned long lastData = millis();
  bool ok = true;

  while (written < (size_t)contentLength) {
    size_t avail = stream->available();
    if (avail > 0) {
      size_t toRead = (avail > sizeof(buf)) ? sizeof(buf) : avail;
      size_t n = stream->readBytes(buf, toRead);
      if (n == 0) continue;

      mbedtls_sha256_update(&ctx, buf, n);
      if (Update.write(buf, n) != n) {
        Serial.printf("\n❌ [OTA] Error escribiendo flash: %s\n", Update.errorString());
        ok = false;
        break;
      }
      written += n;
      lastData = millis();

      int percent = (written * 100) / contentLength;
      if (percent / 10 != lastPercent / 10) {
        Serial.printf("   ► %d%%\n", percent);
        lastPercent = percent;
      }
      if ((written / 4096) % 2 == 0) digitalWrite(_ledPin, !digitalRead(_ledPin));
    } else {
      if (!http.connected() && stream->available() == 0) {
        Serial.println("\n❌ [OTA] Conexión cerrada antes de terminar.");
        ok = false;
        break;
      }
      if (millis() - lastData > OTA_STALL_TIMEOUT_MS) {
        Serial.println("\n❌ [OTA] Descarga detenida (timeout).");
        ok = false;
        break;
      }
      delay(1);
    }
  }

  uint8_t hash[32];
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  http.end();

  if (!ok || written != (size_t)contentLength) {
    Update.abort();
    return false;
  }

  String sha256Got = _toHex(hash, 32);
  if (!sha256Got.equalsIgnoreCase(sha256Expected)) {
    Serial.println("❌ [OTA] SHA256 NO coincide. Firmware corrupto o manipulado.");
    Serial.printf("   esperado: %s\n   recibido: %s\n", sha256Expected.c_str(), sha256Got.c_str());
    Update.abort();
    return false;
  }
  Serial.println("🔐 [OTA] SHA256 verificado OK");

  if (!Update.end(true)) {
    Serial.printf("❌ [OTA] Error finalizando: %s\n", Update.errorString());
    return false;
  }
  return Update.isFinished();
}

// ============================================================================
// PRIVATE - Utilidades
// ============================================================================

int OTAManager::_compareSemver(const String& a, const String& b) {
  int a1 = 0, a2 = 0, a3 = 0, b1 = 0, b2 = 0, b3 = 0;
  const char* pa = a.startsWith("v") ? a.c_str() + 1 : a.c_str();
  const char* pb = b.startsWith("v") ? b.c_str() + 1 : b.c_str();
  sscanf(pa, "%d.%d.%d", &a1, &a2, &a3);
  sscanf(pb, "%d.%d.%d", &b1, &b2, &b3);
  if (a1 != b1) return a1 - b1;
  if (a2 != b2) return a2 - b2;
  return a3 - b3;
}

String OTAManager::_toHex(const uint8_t* data, size_t len) {
  String s;
  s.reserve(len * 2);
  char hex[3];
  for (size_t i = 0; i < len; i++) {
    sprintf(hex, "%02x", data[i]);
    s += hex;
  }
  return s;
}