// ============================================================
// 🔧 SISTEMA IOT — TELEMETRÍA DUAL DS18B20 + CONTROL LEDS
// Autor: Luis Agreda | UMSS | xonar.site
// Broker: Mosquitto en Oracle Cloud (xonarmax.site:1883)
// Descripción:
//   ESP32 que lee dos sensores DS18B20 en bus OneWire (GPIO 21),
//   publica las temperaturas vía MQTT (sin TLS) al broker propio
//   Mosquitto y controla dos LEDs (GPIO 25, GPIO 26) por
//   suscripción remota.
//
// 🔐 Credenciales separadas en src/secrets.h (excluido de Git)
// ============================================================

// 📦 LIBRERÍAS
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// 🔐 Credenciales (WIFI_SSID, WIFI_PASSWORD, MQTT_*)
#include "secrets.h"

// ============================================================
// ⚙️ CONFIGURACIÓN DE PINES (no son secretos, van aquí)
// ============================================================
#define ONE_WIRE_BUS    21   // 🌡️ Bus OneWire para 2 sensores DS18B20
#define LED1_PIN        25   // 💡 LED 1 controlado por test/led1
#define LED2_PIN        26   // 💡 LED 2 controlado por test/led2

// ============================================================
// 📡 TOPICS MQTT (no son secretos)
// ============================================================
const char* TOPIC_PUB_TEMP  = "test/temperaturas";
const char* TOPIC_SUB_LED1  = "test/led1";
const char* TOPIC_SUB_LED2  = "test/led2";

// ============================================================
// ⏱️ PARÁMETROS DE TIEMPO (todo no bloqueante con millis)
// ============================================================
const unsigned long INTERVALO_LECTURA      = 1000;   // 1 s entre lecturas
const unsigned long INTERVALO_RECONEXION   = 5000;   // 5 s entre reintentos MQTT
const unsigned long TIMEOUT_WIFI           = 20000;  // 20 s máx esperando WiFi
const uint8_t       RESOLUCION_SENSOR      = 11;     // 11 bits ≈ 0.125°C, ~375 ms

// ============================================================
// 🌐 OBJETOS GLOBALES
// ============================================================
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WiFiClient espClient;                // 🔓 Cliente sin TLS
PubSubClient mqttClient(espClient);

DeviceAddress sensor1Address;
DeviceAddress sensor2Address;
bool sensor1Encontrado = false;
bool sensor2Encontrado = false;

// ⏱️ Variables de control de tiempo (no bloqueante)
unsigned long ultimaLectura     = 0;
unsigned long ultimaReconexion  = 0;

// ============================================================
// 📡 PROTOTIPOS
// ============================================================
void conectarWiFi();
bool conectarMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void leerYPublicarTemperaturas();
void imprimirDireccionSensor(const char* etiqueta, DeviceAddress addr);
void controlarLED(uint8_t pin, const char* estado, const char* nombreLED);

// ============================================================
// 🔧 SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("============================================================");
  Serial.println("🔧 SISTEMA IOT — DS18B20 + MQTT (Mosquitto Oracle Cloud)");
  Serial.println("============================================================");

  // 💡 Configurar pines de LEDs
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);

  // 🌡️ Inicializar sensores OneWire
  sensors.begin();
  Serial.print("📡 Sensores OneWire detectados en GPIO ");
  Serial.print(ONE_WIRE_BUS);
  Serial.print(": ");
  Serial.println(sensors.getDeviceCount());

  // 🔍 Localizar dirección única de cada sensor
  sensor1Encontrado = sensors.getAddress(sensor1Address, 0);
  sensor2Encontrado = sensors.getAddress(sensor2Address, 1);

  if (sensor1Encontrado) {
    imprimirDireccionSensor("✅ Sensor 1", sensor1Address);
    sensors.setResolution(sensor1Address, RESOLUCION_SENSOR);
  } else {
    Serial.println("⚠️  Error: no se localizó el Sensor 1");
  }

  if (sensor2Encontrado) {
    imprimirDireccionSensor("✅ Sensor 2", sensor2Address);
    sensors.setResolution(sensor2Address, RESOLUCION_SENSOR);
  } else {
    Serial.println("⚠️  Error: no se localizó el Sensor 2");
  }

  // ⏱️ Modo no bloqueante
  sensors.setWaitForConversion(false);

  // 🌐 Conectar WiFi
  conectarWiFi();

  // 📡 Configurar MQTT (sin TLS)
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);

  Serial.print("📡 Broker MQTT: ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  Serial.println("============================================================");
}

// ============================================================
// 🔁 LOOP PRINCIPAL — totalmente no bloqueante
// ============================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  if (!mqttClient.connected()) {
    unsigned long ahora = millis();
    if (ahora - ultimaReconexion >= INTERVALO_RECONEXION) {
      ultimaReconexion = ahora;
      conectarMQTT();
    }
  } else {
    mqttClient.loop();
  }

  if (millis() - ultimaLectura >= INTERVALO_LECTURA) {
    ultimaLectura = millis();
    leerYPublicarTemperaturas();
  }
}

// ============================================================
// 🌐 CONEXIÓN WIFI con timeout
// ============================================================
void conectarWiFi() {
  Serial.print("📶 Conectando a WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - inicio < TIMEOUT_WIFI)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ WiFi conectado | IP: ");
    Serial.print(WiFi.localIP());
    Serial.print(" | RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("⚠️  Timeout WiFi — reintentará en el próximo loop");
  }
}

// ============================================================
// 📡 CONEXIÓN MQTT con resuscripción automática
// ============================================================
bool conectarMQTT() {
  if (WiFi.status() != WL_CONNECTED) return false;

  Serial.print("📡 Conectando al broker Mosquitto... ");

  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("✅ conectado");
    mqttClient.subscribe(TOPIC_SUB_LED1);
    mqttClient.subscribe(TOPIC_SUB_LED2);
    Serial.print("📥 Suscrito a: ");
    Serial.print(TOPIC_SUB_LED1);
    Serial.print(" , ");
    Serial.println(TOPIC_SUB_LED2);
    return true;
  } else {
    Serial.print("❌ falló (rc=");
    Serial.print(mqttClient.state());
    Serial.println(") — reintenta en 5 s");
    return false;
  }
}

// ============================================================
// 📩 CALLBACK MQTT — usa strcmp() en lugar de String
// ============================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buffer[16];
  unsigned int len = (length < sizeof(buffer) - 1) ? length : sizeof(buffer) - 1;
  memcpy(buffer, payload, len);
  buffer[len] = '\0';

  Serial.print("📩 Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] : ");
  Serial.println(buffer);

  if (strcmp(topic, TOPIC_SUB_LED1) == 0) {
    controlarLED(LED1_PIN, buffer, "LED1");
  } else if (strcmp(topic, TOPIC_SUB_LED2) == 0) {
    controlarLED(LED2_PIN, buffer, "LED2");
  }
}

// ============================================================
// 💡 CONTROL DE LED por payload "on" / "off"
// ============================================================
void controlarLED(uint8_t pin, const char* estado, const char* nombreLED) {
  if (strcmp(estado, "on") == 0) {
    digitalWrite(pin, HIGH);
    Serial.print("💡 ");
    Serial.print(nombreLED);
    Serial.println(" → ON");
  } else if (strcmp(estado, "off") == 0) {
    digitalWrite(pin, LOW);
    Serial.print("💡 ");
    Serial.print(nombreLED);
    Serial.println(" → OFF");
  } else {
    Serial.print("⚠️  Comando desconocido para ");
    Serial.print(nombreLED);
    Serial.print(": ");
    Serial.println(estado);
  }
}

// ============================================================
// 🌡️ LECTURA Y PUBLICACIÓN DE TEMPERATURAS
// ============================================================
void leerYPublicarTemperaturas() {
  if (!sensor1Encontrado || !sensor2Encontrado) {
    Serial.println("⚠️  Saltando lectura: algún sensor no fue inicializado");
    return;
  }

  sensors.requestTemperatures();
  delay(400);

  float temp1 = sensors.getTempC(sensor1Address);
  float temp2 = sensors.getTempC(sensor2Address);

  bool s1OK = (temp1 != DEVICE_DISCONNECTED_C && temp1 > -55.0 && temp1 < 125.0);
  bool s2OK = (temp2 != DEVICE_DISCONNECTED_C && temp2 > -55.0 && temp2 < 125.0);

  if (s1OK) {
    Serial.printf("🌡️  Sensor 1: %.1f °C\n", temp1);
  } else {
    Serial.println("⚠️  Error de lectura en Sensor 1");
  }

  if (s2OK) {
    Serial.printf("🌡️  Sensor 2: %.1f °C\n", temp2);
  } else {
    Serial.println("⚠️  Error de lectura en Sensor 2");
  }

  if (s1OK && s2OK && mqttClient.connected()) {
    char jsonPayload[64];
    snprintf(jsonPayload, sizeof(jsonPayload),
             "{\"sensor1\":%.1f,\"sensor2\":%.1f}", temp1, temp2);

    if (mqttClient.publish(TOPIC_PUB_TEMP, jsonPayload)) {
      Serial.print("📤 Publicado en ");
      Serial.print(TOPIC_PUB_TEMP);
      Serial.print(" → ");
      Serial.println(jsonPayload);
    } else {
      Serial.println("⚠️  Falló la publicación MQTT");
    }
  } else if (!mqttClient.connected()) {
    Serial.println("⚠️  Sin publicar: MQTT desconectado");
  } else {
    Serial.println("⚠️  Sin publicar: error en algún sensor");
  }
}

// ============================================================
// 🔍 UTILIDAD — imprimir dirección OneWire en hex
// ============================================================
void imprimirDireccionSensor(const char* etiqueta, DeviceAddress addr) {
  Serial.print(etiqueta);
  Serial.print(" → ");
  for (uint8_t i = 0; i < 8; i++) {
    if (addr[i] < 16) Serial.print("0");
    Serial.print(addr[i], HEX);
    if (i < 7) Serial.print(":");
  }
  Serial.println();
}
