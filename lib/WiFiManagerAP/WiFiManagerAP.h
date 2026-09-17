#ifndef WIFIMANAGER_AP_H
#define WIFIMANAGER_AP_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Forward declaration
class DeviceIdentity;

class WiFiManagerAP {
public:
  // Constructor
  WiFiManagerAP(int ledStatusPin = 12, int buttonPin = 27, DeviceIdentity* deviceIdentity = nullptr);

  // Métodos públicos
  void begin(String deviceAlias = "Central");
  void handle();
  bool isActive();
  void activate();
  void deactivate();

  // Getters
  String getDeviceId();
  String getAlias();
  String getSSID();
  String getPass();

private:
  // Pines
  int _ledStatusPin;
  int _buttonPin;

  // Referencia a DeviceIdentity (fuente única de verdad)
  DeviceIdentity* _deviceIdentity;

  // Estado
  bool _isAPActive = false;
  unsigned long _apStartTime = 0;
  const unsigned long AP_TIMEOUT_MS = 180000; // 3 minutos

  // Credenciales WiFi
  String _alias;
  char _ssid[50] = "";
  char _pass[50] = "";

  // IP del AP
  IPAddress _apIP = IPAddress(192, 168, 4, 1);
  IPAddress _netMask = IPAddress(255, 255, 255, 0);

  // WebServer
  WebServer _server{80};

  // Métodos privados - Inicialización
  void _initializeFilesystem();
  void _loadConfiguration();
  bool _saveConfiguration(const char* alias, const char* ssid, const char* pass);

  // Métodos privados - WebServer
  void _startWebServer();
  void _renderPortalHTML();
  void _handleScanNetworks();
  void _handleSaveConfig();

  // Métodos privados - Utilidad
  void _blinkLED();
  void _checkAPTimeout();
};

#endif