# 🔧 Sistema IoT: Telemetría Dual DS18B20 + Control LEDs vía MQTT

Este proyecto implementa un sistema IoT en un microcontrolador **ESP32** utilizando el framework de Arduino mediante **PlatformIO**. 

El sistema realiza dos funciones principales y simultáneas de manera no bloqueante:
1. **Telemetría**: Lee la temperatura de dos sensores DS18B20 (mediante el bus OneWire) y publica los datos en formato JSON a un broker MQTT.
2. **Control Remoto**: Se suscribe a tópicos MQTT específicos para recibir comandos y encender/apagar dos LEDs de forma remota.

Desarrollado por **Luis Agreda** | UMSS | [xonar.site](https://xonar.site)

---

## 🚀 Arquitectura y Componentes

- **Placa**: ESP32 (esp32doit-devkit-v1)
- **Sensores**: 2x DS18B20 conectados en paralelo a un único pin (bus OneWire).
- **Actuadores**: 2x LEDs estándar.
- **Comunicación**: Conexión a Internet vía WiFi y mensajería a través de un broker **Mosquitto MQTT** alojado en **Oracle Cloud** (sin TLS por el puerto 1883).

## ⚙️ Conexiones de Hardware (Pines)

| Componente | Pin del ESP32 | Descripción |
| :--- | :---: | :--- |
| Bus OneWire (DS18B20) | `GPIO 21` | Bus de datos para ambos sensores de temperatura. Requiere resistencia Pull-Up de 4.7kΩ. |
| LED 1 | `GPIO 25` | LED indicador número 1. |
| LED 2 | `GPIO 26` | LED indicador número 2. |

## 📡 Tópicos MQTT Utilizados

El ESP32 interactúa con el broker utilizando los siguientes tópicos (por defecto):

- **Publicación (Telemetría)**: 
  - Tópico: `test/temperaturas`
  - Payload (JSON): `{"sensor1": 24.5, "sensor2": 25.1}`
- **Suscripción (Control de LED 1)**: 
  - Tópico: `test/led1`
  - Payload esperado: `on` o `off`
- **Suscripción (Control de LED 2)**: 
  - Tópico: `test/led2`
  - Payload esperado: `on` o `off`

## 🔐 Seguridad y Credenciales

Las credenciales de acceso a WiFi y MQTT no están incluidas en el código principal por motivos de seguridad.

Para compilar tu propio proyecto, debes:
1. Renombrar o copiar el archivo `src/secrets.h.example` a `src/secrets.h`.
2. Abrir `src/secrets.h` y llenar tus propias credenciales:
   - `WIFI_SSID` y `WIFI_PASSWORD`
   - `MQTT_BROKER`, `MQTT_PORT`, `MQTT_CLIENT_ID`, `MQTT_USER`, `MQTT_PASSWORD`

El archivo `secrets.h` está configurado en el `.gitignore` para no ser subido nunca al repositorio.

## 🛠️ Instalación y Uso

1. Clona este repositorio:
   ```bash
   git clone https://github.com/xonar7/IOT_MOSQUITTO_ORACLE.git
   ```
2. Abre la carpeta del proyecto con **Visual Studio Code** y la extensión **PlatformIO**.
3. Configura tus credenciales copiando el archivo `secrets.h.example` a `secrets.h` en la carpeta `src`.
4. Compila y sube el código a tu placa usando PlatformIO (`Upload`).
5. Abre el **Serial Monitor** a `115200 baudios` para ver el estado de la conexión y los diagnósticos.

## 📦 Dependencias (Librerías)

El proyecto utiliza las siguientes librerías gestionadas automáticamente por PlatformIO (`platformio.ini`):
- `paulstoffregen/OneWire`
- `milesburton/DallasTemperature`
- `knolleary/PubSubClient`
