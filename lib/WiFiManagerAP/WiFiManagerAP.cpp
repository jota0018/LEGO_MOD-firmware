#include "WiFiManagerAP.h"
#include "DeviceIdentity.h"

WiFiManagerAP::WiFiManagerAP(int ledStatusPin, int buttonPin, DeviceIdentity* deviceIdentity)
    : _ledStatusPin(ledStatusPin), _buttonPin(buttonPin), _deviceIdentity(deviceIdentity) {}

void WiFiManagerAP::begin(String deviceAlias) {
  // Inicializar pines
  pinMode(_ledStatusPin, OUTPUT);
  pinMode(_buttonPin, INPUT_PULLUP);
  digitalWrite(_ledStatusPin, LOW);

  // Usar Device ID desde DeviceIdentity (fuente única de verdad)
  String deviceId = (_deviceIdentity) ? _deviceIdentity->getDeviceId() : "Central_UNKNOWN";
  _alias = deviceAlias.isEmpty() ? deviceId : deviceAlias;

  // Inicializar SPIFFS y cargar configuración
  _initializeFilesystem();
  _loadConfiguration();

  Serial.println("\n=================================");
  Serial.println("🚀 WiFiManager AP INICIALIZADO");
  Serial.println("=================================");
  Serial.printf("   ► Device ID: %s\n", deviceId.c_str());
  Serial.printf("   ► Alias:     %s\n", _alias.c_str());
  Serial.println("=================================\n");
}

void WiFiManagerAP::handle() {
  if (_isAPActive) {
    _server.handleClient();
    _blinkLED();
    _checkAPTimeout();
  } else {
    digitalWrite(_ledStatusPin, HIGH);
  }
}

bool WiFiManagerAP::isActive() {
  return _isAPActive;
}

void WiFiManagerAP::activate() {
  if (_isAPActive) return; // Ya está activo

  _isAPActive = true;
  _apStartTime = millis();

  // Limpiar stack WiFi
  WiFi.disconnect(true);
  delay(100);

  // Configurar modo AP
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(_apIP, _apIP, _netMask);

  // Iniciar AP con canal 6 y máx 4 conexiones
  String ssidAP = "Setup-" + getDeviceId();
  WiFi.softAP(ssidAP.c_str(), NULL, 6, 0, 4);

  // Iniciar servidor web
  _startWebServer();

  Serial.println("\n🌐 [MODO AP ACTIVADO]");
  Serial.printf("   ► SSID: %s\n", ssidAP.c_str());
  Serial.printf("   ► URL:  http://192.168.4.1\n\n");
}

void WiFiManagerAP::deactivate() {
  if (!_isAPActive) return;

  _isAPActive = false;
  _server.stop();
  WiFi.mode(WIFI_OFF);
  digitalWrite(_ledStatusPin, HIGH);

  Serial.println("🔌 [MODO AP DESACTIVADO]\n");
}

String WiFiManagerAP::getDeviceId() {
  // Retornar desde DeviceIdentity (fuente única de verdad)
  return (_deviceIdentity) ? _deviceIdentity->getDeviceId() : "Central_UNKNOWN";
}

String WiFiManagerAP::getAlias() {
  return _alias;
}

String WiFiManagerAP::getSSID() {
  return String(_ssid);
}

String WiFiManagerAP::getPass() {
  return String(_pass);
}

// ============================================================================
// MÉTODOS PRIVADOS - Inicialización
// ============================================================================

void WiFiManagerAP::_initializeFilesystem() {
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ [FS] Error al montar SPIFFS.");
  } else {
    Serial.println("📁 [FS] Sistema de archivos SPIFFS listo.");
  }
}

void WiFiManagerAP::_loadConfiguration() {
  if (SPIFFS.exists("/config.json")) {
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, configFile);
      if (!error) {
        strlcpy(_ssid, doc["ssid"] | "", sizeof(_ssid));
        strlcpy(_pass, doc["pass"] | "", sizeof(_pass));
        _alias = doc["alias"] | getDeviceId();

        Serial.println("✅ [CONFIG] Configuración cargada:");
        Serial.printf("   ► Device ID: %s\n", getDeviceId().c_str());
        Serial.printf("   ► Alias:     %s\n", _alias.c_str());
        Serial.printf("   ► WiFi SSID: %s\n", _ssid);
        Serial.printf("   ► WiFi Pass: %s\n", _pass);
        configFile.close();
        return;
      }
      configFile.close();
    }
  }
  Serial.println("⚠️ [CONFIG] Sin archivo previo. Usando valores por defecto.");
  _alias = getDeviceId();
}

bool WiFiManagerAP::_saveConfiguration(const char* alias, const char* ssid, const char* pass) {
  DynamicJsonDocument doc(1024);
  doc["deviceId"] = getDeviceId();
  doc["alias"]    = alias;
  doc["ssid"]     = ssid;
  doc["pass"]     = pass;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("❌ [FS] Error al abrir config.json para escritura.");
    return false;
  }

  if (serializeJson(doc, configFile) == 0) {
    Serial.println("❌ [FS] Error al escribir JSON.");
    configFile.close();
    return false;
  }

  configFile.close();
  Serial.println("💾 [FS] Datos guardados exitosamente.");
  return true;
}

// ============================================================================
// MÉTODOS PRIVADOS - WebServer
// ============================================================================

void WiFiManagerAP::_startWebServer() {
  _server.on("/", HTTP_GET, [this]() { _renderPortalHTML(); });
  _server.on("/scan", HTTP_GET, [this]() { _handleScanNetworks(); });
  _server.on("/guardar", HTTP_GET, [this]() { _handleSaveConfig(); });
  _server.begin();
}

void WiFiManagerAP::_renderPortalHTML() {
  String randomSuffixPass  = String(random(100000, 999999));
  String randomSuffixAlias = String(random(100000, 999999));
  String idPassField  = "field_pwd_" + randomSuffixPass;
  String idAliasField = "field_als_" + randomSuffixAlias;

  String html = "<!DOCTYPE html><html lang='es'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>";
  html += "<title>Ajustes de Central</title>";

  html += "<style>";
  html += "* { box-sizing: border-box; }";
  html += "body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; background-color: #0f172a; color: #f8fafc; margin:0; padding:16px; display:flex; justify-content:center; align-items:flex-start; min-height:100vh; padding-top:60px; }";
  html += ".card { background: #1e293b; padding: 24px 20px; border-radius: 16px; width: 100%; max-width: 400px; border: 1px solid #334155; text-align: center; }";
  html += "h2 { color: #38bdf8; margin-top: 0; font-size: 20px; }";
  html += ".device-info { background: #0f172a; padding: 8px; border-radius: 8px; margin-bottom: 16px; font-family: monospace; color: #94a3b8; font-size: 12px; }";
  html += "label { font-size: 13px; color: #94a3b8; display: block; margin-top: 14px; margin-bottom: 6px; text-align: left; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 12px; border-radius: 8px; border: 1px solid #334155; background: #0f172a; color: #f8fafc; font-size: 16px; outline: none; }";
  html += "input:focus { border-color: #38bdf8; box-shadow: 0 0 0 2px rgba(56, 189, 248, 0.2); }";
  html += ".btn-save { width: 100%; background: #0284c7; color: #fff; font-weight: 600; padding: 14px; border: none; border-radius: 8px; margin-top: 24px; cursor: pointer; font-size: 15px; }";
  html += ".custom-dropdown { position: relative; width: 100%; text-align: left; }";
  html += ".dropdown-trigger { width: 100%; padding: 12px; border-radius: 8px; border: 1px solid #334155; background: #0f172a; color: #f8fafc; font-size: 14px; display: flex; justify-content: space-between; align-items: center; cursor: pointer; }";
  html += ".dropdown-menu { position: absolute; top: calc(100% + 4px); left: 0; right: 0; background: #0f172a; border: 1px solid #334155; border-radius: 8px; max-height: 180px; overflow-y: auto; display: none; z-index: 10; }";
  html += ".dropdown-menu.show { display: block; }";
  html += ".dropdown-item { padding: 12px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #1e293b; cursor: pointer; }";
  html += ".wifi-signal { display: inline-flex; gap: 3px; align-items: flex-end; }";
  html += ".wifi-bar { width: 4px; background: #38bdf8; border-radius: 2px; }";
  html += ".wifi-bar.off { opacity: 0.2; }";
  html += ".wifi-bar.h1 { height: 8px; }";
  html += ".wifi-bar.h2 { height: 12px; }";
  html += ".wifi-bar.h3 { height: 16px; }";
  html += "#form-section, #done-section, #manual_ssid_div { display: none; }";
  html += ".loader-container { padding: 20px 0; }";
  html += ".spinner { width: 48px; height: 48px; border: 4px solid #334155; border-top-color: #38bdf8; border-radius: 50%; animation: spin 1s linear infinite; margin: 0 auto 16px auto; }";
  html += "@keyframes spin { to { transform: rotate(360deg); } }";
  html += "</style>";

  html += "<script>";
  html += "var ssidSeleccionado = '';";
  html += "var passFieldId = '" + idPassField + "';";
  html += "var aliasFieldId = '" + idAliasField + "';";
  html += "function toggleDropdown() { document.getElementById('wifi_menu').classList.toggle('show'); }";
  html += "function seleccionarOpcion(nombreSSID, htmlIcono) {";
  html += "  ssidSeleccionado = nombreSSID;";
  html += "  var selectedText = document.getElementById('selected_text');";
  html += "  var campoManual = document.getElementById('manual_ssid_div');";
  html += "  if(nombreSSID === '__MANUAL__') { selectedText.innerHTML = '➕ Red Oculta'; campoManual.style.display = 'block'; }";
  html += "  else { selectedText.innerHTML = '<span style=\"margin-right:8px;\">' + htmlIcono + '</span>' + nombreSSID; campoManual.style.display = 'none'; }";
  html += "  document.getElementById('wifi_menu').classList.remove('show');";
  html += "}";
  html += "function habilitarCampo(el) { el.removeAttribute('readonly'); }";
  html += "function activarModoClave(el) { el.removeAttribute('readonly'); el.type = 'password'; }";
  html += "function cargarRedes() {";
  html += "  fetch('/scan').then(r => r.json()).then(redes => {";
  html += "    var menu = document.getElementById('wifi_menu'); menu.innerHTML = '';";
  html += "    if(redes.length === 0) { seleccionarOpcion('__MANUAL__', ''); }";
  html += "    else {";
  html += "      redes.forEach(r => {";
  html += "        var bars = (r.rssi >= -65) ? 3 : (r.rssi >= -75) ? 2 : 1;";
  html += "        var iconHtml = '<div class=\"wifi-signal\">';";
  html += "        iconHtml += '<div class=\"wifi-bar h1\"></div>';";
  html += "        if(bars >= 2) iconHtml += '<div class=\"wifi-bar h2\"></div>';";
  html += "        else iconHtml += '<div class=\"wifi-bar h2 off\"></div>';";
  html += "        if(bars >= 3) iconHtml += '<div class=\"wifi-bar h3\"></div>';";
  html += "        else iconHtml += '<div class=\"wifi-bar h3 off\"></div>';";
  html += "        iconHtml += '</div>';";
  html += "        var item = document.createElement('div'); item.className = 'dropdown-item';";
  html += "        item.onclick = function() { seleccionarOpcion(r.ssid, iconHtml); };";
  html += "        item.innerHTML = '<span>' + r.ssid + '</span>' + iconHtml;";
  html += "        menu.appendChild(item);";
  html += "      });";
  html += "      var itemM = document.createElement('div'); itemM.className = 'dropdown-item';";
  html += "      itemM.onclick = function() { seleccionarOpcion('__MANUAL__', ''); };";
  html += "      itemM.innerHTML = '<span>➕ Red Oculta</span>';";
  html += "      menu.appendChild(itemM);";
  html += "    }";
  html += "    document.getElementById('welcome-section').style.display = 'none';";
  html += "    document.getElementById('form-section').style.display = 'block';";
  html += "  }).catch(() => { seleccionarOpcion('__MANUAL__', ''); document.getElementById('welcome-section').style.display = 'none'; document.getElementById('form-section').style.display = 'block'; });";
  html += "}";
  html += "function enviarDatos() {";
  html += "  var alias = document.getElementById(aliasFieldId).value.trim();";
  html += "  var pass = document.getElementById(passFieldId).value.trim();";
  html += "  var ssidFinal = (ssidSeleccionado === '__MANUAL__') ? document.getElementById('inp_manual').value.trim() : ssidSeleccionado;";
  html += "  if(!ssidFinal) { alert('Selecciona una red Wi-Fi.'); return; }";
  html += "  document.getElementById('form-section').style.display = 'none';";
  html += "  document.getElementById('done-section').style.display = 'block';";
  html += "  var url = '/guardar?alias=' + encodeURIComponent(alias) + '&ssid=' + encodeURIComponent(ssidFinal) + '&pass=' + encodeURIComponent(pass);";
  html += "  fetch(url).catch(e => console.log(e));";
  html += "}";
  html += "window.onload = cargarRedes;";
  html += "</script></head><body>";

  html += "<div class='card'>";
  html += "<div id='welcome-section' class='loader-container'>";
  html += "<h2>¡Bienvenido!</h2><div class='spinner'></div>";
  html += "<div style='color:#94a3b8; font-size:14px;'>Escaneando redes cercanas...</div>";
  html += "</div>";

  html += "<div id='form-section'>";
  html += "<h2>⚙️ Ajustes de Central</h2>";
  html += "<div class='device-info'>ID: <b>" + getDeviceId() + "</b></div>";
  html += "<form onsubmit='return false;' autocomplete='off'>";
  html += "<label>Alias (Ubicación):</label>";
  html += "<input type='text' id='" + idAliasField + "' value='" + _alias + "' readonly onfocus='habilitarCampo(this)' autocomplete='off'>";
  html += "<label>Selecciona tu Red Wi-Fi:</label>";
  html += "<div class='custom-dropdown'>";
  html += "  <div class='dropdown-trigger' onclick='toggleDropdown()'>";
  html += "    <span id='selected_text'>-- Selecciona una red --</span>";
  html += "  </div>";
  html += "  <div class='dropdown-menu' id='wifi_menu'></div>";
  html += "</div>";
  html += "<div id='manual_ssid_div'>";
  html += "<label>Nombre de Red (SSID):</label>";
  html += "<input type='text' id='inp_manual' autocomplete='off'>";
  html += "</div>";
  html += "<label>Contraseña Wi-Fi:</label>";
  html += "<input type='text' id='" + idPassField + "' value='" + String(_pass) + "' readonly onfocus='activarModoClave(this)' oninput='this.type=\"password\"' autocomplete='off'>";
  html += "<button type='button' class='btn-save' onclick='enviarDatos()'>Guardar y Reiniciar</button>";
  html += "</form>";
  html += "</div>";

  html += "<div id='done-section' class='loader-container'>";
  html += "<div style='font-size:48px;'>🎉</div>";
  html += "<h3 style='color:#4ade80;'>¡Configuración Guardada!</h3>";
  html += "<div style='color:#94a3b8; font-size:14px;'>La central se está reiniciando.</div>";
  html += "</div>";
  html += "</div></body></html>";

  _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  _server.send(200, "text/html", html);
}

void WiFiManagerAP::_handleScanNetworks() {
  int n = WiFi.scanNetworks(false, false);
  DynamicJsonDocument doc(2048);
  JsonArray array = doc.to<JsonArray>();

  for (int i = 0; i < n; ++i) {
    JsonObject net = array.createNestedObject();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
  }

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  WiFi.scanDelete();

  _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  _server.send(200, "application/json", jsonResponse);
}

void WiFiManagerAP::_handleSaveConfig() {
  if (_server.hasArg("alias") && _server.hasArg("ssid") && _server.hasArg("pass")) {
    String reqAlias = _server.arg("alias");
    String reqSSID  = _server.arg("ssid");
    String reqPass  = _server.arg("pass");

    if (reqSSID.length() > 0) {
      Serial.println("\n📥 [WEB] Configuración recibida:");
      Serial.printf("   ► Alias: %s\n", reqAlias.c_str());
      Serial.printf("   ► SSID:  %s\n", reqSSID.c_str());

      _saveConfiguration(reqAlias.c_str(), reqSSID.c_str(), reqPass.c_str());
      _server.send(200, "text/plain", "OK");

      Serial.println("🔄 [SISTEMA] Reiniciando en 1.5 segundos...");
      delay(1500);
      ESP.restart();
    } else {
      _server.send(400, "text/plain", "SSID inválido.");
    }
  } else {
    _server.send(400, "text/plain", "Faltan parámetros.");
  }
}

// ============================================================================
// MÉTODOS PRIVADOS - Utilidad
// ============================================================================

void WiFiManagerAP::_blinkLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;

  if (millis() - lastBlink >= 100) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(_ledStatusPin, ledState ? HIGH : LOW);
  }
}

void WiFiManagerAP::_checkAPTimeout() {
  if (millis() - _apStartTime >= AP_TIMEOUT_MS) {
    Serial.println("⏰ [TIMEOUT] Modo AP agotado. Reiniciando...");
    digitalWrite(_ledStatusPin, LOW);
    delay(100);
    ESP.restart();
  }
}