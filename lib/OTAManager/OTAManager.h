#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

/**
 * OTAManager - Actualización de firmware desde GitHub Releases
 *
 * Flujo de checkAndUpdate():
 *   1. Conecta a WiFi (credenciales de WiFiManagerAP)
 *   2. Descarga version.json del release "latest"
 *   3. Compara semver: solo actualiza si la remota es MAYOR
 *   4. Descarga firmware.bin verificando SHA256 mientras escribe
 *   5. Reinicia con el nuevo firmware
 *
 * Rollback (nativo ESP32):
 *   El firmware nuevo arranca "en prueba". Si corre 20 s estable,
 *   handle() lo confirma. Si se cuelga o reinicia antes, el
 *   bootloader vuelve solo a la versión anterior.
 */
class OTAManager {
public:
  OTAManager(int ledStatusPin = 12);

  // owner/repo de GitHub donde se publican los releases
  // Si no se proporcionan, usa los valores del platformio.ini (GITHUB_REPO, GITHUB_HOST)
  void begin(const char* githubOwner, const char* githubRepo, const char* currentVersion);

  // Llamar en loop(): confirma el firmware nuevo tras 20 s estable
  void handle();

  // Confirmación manual del firmware (cancela rollback)
  void confirmBoot();

  // Busca y aplica actualización. Reinicia si actualiza.
  // Devuelve false si no actualizó (sin update, error, sin WiFi).
  bool checkAndUpdate(const String& ssid, const String& pass);

  String getVersion();

private:
  int _ledPin;
  String _owner, _repo, _version;
  String _manifestUrl, _firmwareUrl;
  bool _pendingVerify = false;

  bool _connectWiFi(const String& ssid, const String& pass);
  void _wifiOff();
  bool _fetchManifest(String& remoteVersion, String& sha256, size_t& size);
  bool _downloadAndFlash(const String& sha256Expected, size_t expectedSize);
  int  _compareSemver(const String& a, const String& b);  // >0 si a > b
  static String _toHex(const uint8_t* data, size_t len);
};

#endif // OTA_MANAGER_H