# 🧩 Componentes LEGO

Librería modular de componentes reutilizables para proyectos ESP32.

## Componentes disponibles

### DeviceIdentity
Generador determinístico de identificadores únicos desde la MAC del dispositivo.
- Device ID: `Central_XXXXXX`
- LoRa Channel: `0-127`
- Device Address: `0-255`

**Uso:**
```cpp
DeviceIdentity identity;
identity.begin();
String id = identity.getDeviceId();
```

---

### WiFiManagerAP (próximamente integrado)
Portal web de configuración WiFi con interfaz elegante.

---