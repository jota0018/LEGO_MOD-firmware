#include "DeviceIdentity.h"
#include <WiFi.h>
#include <SPIFFS.h>

#define LORA_FREQ_MIN       433.0
#define LORA_FREQ_MAX       434.75
#define LORA_CHANNELS       50
#define LORA_CHANNEL_WIDTH  0.015

DeviceIdentity::DeviceIdentity(String prefix) : _prefix(prefix) {}

void DeviceIdentity::begin() {
  Serial.println("📱 Inicializando DeviceIdentity...");

  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Error inicializando SPIFFS");
    return;
  }

  _deviceId = _generateDeviceId();
  _loRaChannel = _calculateLoRaChannel();

  Serial.printf("✅ Device ID Generado: %s\n", _deviceId.c_str());
  Serial.printf("   MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf("   LoRa Channel: %d (%.3f MHz)\n",
                _loRaChannel, getLoRaFrequency());
}

String DeviceIdentity::getDeviceId() {
  return _deviceId;
}

String DeviceIdentity::getPrefix() {
  return _prefix;
}

void DeviceIdentity::setPrefix(String prefix) {
  _prefix = prefix;
  _deviceId = _generateDeviceId();
}

int DeviceIdentity::getLoRaChannel() {
  if (_loRaChannel < 0) {
    _loRaChannel = _calculateLoRaChannel();
  }
  return _loRaChannel;
}

float DeviceIdentity::getLoRaFrequency() {
  int channel = getLoRaChannel();
  return LORA_FREQ_MIN + (channel * LORA_CHANNEL_WIDTH);
}

long DeviceIdentity::getLoRaFrequencyHz() {
  return (long)(getLoRaFrequency() * 1000000);
}

String DeviceIdentity::_generateDeviceId() {
  String hash = _getMacHashHex();
  return _prefix + "_" + hash;
}

String DeviceIdentity::_getMacHashHex() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  uint32_t hash = (mac[3] << 16) | (mac[4] << 8) | mac[5];

  char hexBuffer[7];
  sprintf(hexBuffer, "%06X", hash);

  return String(hexBuffer);
}

int DeviceIdentity::_calculateLoRaChannel() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  uint16_t macPart = (mac[4] << 8) | mac[5];
  int channel = macPart % LORA_CHANNELS;

  return channel;
}