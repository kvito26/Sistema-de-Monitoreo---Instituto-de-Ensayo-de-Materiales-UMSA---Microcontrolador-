#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// --- CONFIGURACIÓN DE RED ---
//const char* ssid     = "kevito_sam";
//const char* password = "12345kev";
const char* ssid     = "vw-03826";
const char* password = "ZTERRTHJ8902852";

// --- CONFIGURACIÓN DEL SERVIDOR SWOOLE ---
// Cambia esto por la IP de tu servidor 'vito'
//const char* ws_host = "10.245.59.12"; 
const char* ws_host = "192.168.1.224"; 
const int   ws_port = 8080;

// --- HARDWARE ---
const int LED_PIN = 12; // Pin del LED interno en la mayoría de ESP32
WebSocketsClient webSocket;

// Identificador único definido en tu entidad Dispositivo
const String DEVICE_ID = "esp_amb1"; 

// --- GESTIÓN DE EVENTOS DEL WEBSOCKET ---
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Desconectado del servidor");
      break;

    case WStype_CONNECTED:
      Serial.println("[WS] Conectado! Enviando autenticación...");
      // Al conectar, enviamos la "llave" para que Swoole nos registre
      enviarAutenticacion();
      break;

    case WStype_TEXT: {
      Serial.printf("[WS] Mensaje recibido: %s\n", payload);
      
      // Creamos un documento JSON para leer lo que mandó el servidor
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("Error al parsear JSON: ");
        Serial.println(error.c_str());
        return;
      }

      // 1. Verificamos si es una orden de encendido/apagado
      if (doc["tipo"] == "orden") {
        int valor = doc["valor"]; // 1 o 0
        digitalWrite(LED_PIN, valor);
        Serial.println(valor == 1 ? "LED Encendido" : "LED Apagado");

        // 2. ENVIAR FEEDBACK (Retroalimentación)
        // Esto le confirma al usuario que el LED realmente cambió de estado
        enviarFeedback(valor);
      }
      break;
    }
  }
}

// Función para registrar el dispositivo en la memoria de Swoole
void enviarAutenticacion() {
  JsonDocument doc;
  doc["action"] = "auth";
  doc["device_id"] = DEVICE_ID;

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

// Función para confirmar la orden recibida
void enviarFeedback(int nuevoEstado) {
  JsonDocument doc;
  doc["action"] = "feedback";
  doc["device_id"] = DEVICE_ID;
  doc["new_state"] = nuevoEstado; // Confirmamos el estado real del hardware

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

// Función para enviar datos de sensores (simulados para el ejemplo)
void enviarDatosSensores() {
  // En un caso real usarías dht.readTemperature()
  float t = random(20, 30); 
  float h = random(40, 60);

  JsonDocument doc;
  doc["action"] = "sensor_data"; // Acción que procesa tu SocketController
  doc["device_id"] = DEVICE_ID;
  doc["temp"] = t;
  doc["hum"] = h;

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
  Serial.println("📊 Datos de sensores enviados.");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Conexión WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");

  // Configuración de WebSocket
  webSocket.begin(ws_host, ws_port, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000); // Reintento automático cada 5 seg
}

unsigned long lastMillis = 0;

void loop() {
  webSocket.loop(); // Mantiene viva la conexión

  // Enviar datos de temperatura cada 10 segundos
  if (millis() - lastMillis > 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      enviarDatosSensores();
    }
    lastMillis = millis();
  }
}
