#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

const char* ssid = "vw-03826";       // Pon tu red real
const char* password = "ZTERRTHJ8902852"; 
const char* host = "192.168.1.224";   // IP de tu servidor Linux
const int port = 8080;

WebSocketsClient webSocket;
const int LED_PIN = 12; // Pin del LED integrado (o el que uses, ej: 12)

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Desconectado!");
            break;
        case WStype_CONNECTED:
            Serial.println("[WS] Conectado a Swoole!");
            // Enviar saludo inicial
            webSocket.sendTXT("{\"tipo\":\"info\",\"valor\":\"Hola soy el amo del ambiente 1\"}");
            break;
        case WStype_TEXT:
            Serial.printf("[WS] Recibido: %s\n", payload);

            JsonDocument doc; // ArduinoJson 7
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                // Verificamos si es un comando para el LED
                const char* tipo = doc["tipo"]; 
                if (strcmp(tipo, "led") == 0) {
                    int valor = doc["valor"];
                    digitalWrite(LED_PIN, (valor == 1) ? HIGH : LOW);
                    Serial.println((valor == 1) ? "Luz ENCENDIDA" : "Luz APAGADA");
                    
                    // (Opcional) Confirmar a la web que se hizo
                    webSocket.sendTXT("{\"tipo\":\"info\",\"valor\":\"Accion ejecutada\"}");
                }
            }
            break;
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Conectado");

    // Configuración WebSocket
    webSocket.begin(host, port, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000); // Reintentar cada 5s si se cae
    
    // Ping/Pong para mantener viva la conexión
    webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
    webSocket.loop();
}
