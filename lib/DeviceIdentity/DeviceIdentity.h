#ifndef DEVICE_IDENTITY_H
#define DEVICE_IDENTITY_H

#include <Arduino.h>

class DeviceIdentity {
public:
  DeviceIdentity(String prefix = "Central");

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