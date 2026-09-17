#ifndef DEVICE_IDENTITY_H
#define DEVICE_IDENTITY_H

#include <Arduino.h>

// Fallback si no está definido en platformio.ini
#ifndef DEVICE_NAME
#define DEVICE_NAME "Central"
#endif

class DeviceIdentity {
public:
  // Si no se proporciona prefix, usa DEVICE_NAME del ini
  DeviceIdentity(String prefix = DEVICE_NAME);

  void begin();

  String getDeviceId();
  String getPrefix();
  void setPrefix(String prefix);

  int getLoRaChannel();
  float getLoRaFrequency();
  long getLoRaFrequencyHz();

private:
  String _prefix;
  String _deviceId;
  int _loRaChannel = -1;

  String _generateDeviceId();
  String _getMacHashHex();
  int _calculateLoRaChannel();
};

#endif // DEVICE_IDENTITY_H