#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "kevito_sam";
const char* password = "12345kev";
const char* mqtt_server = "10.245.59.12";

WiFiClient espClient;
PubSubClient client(espClient);
const int LED_PIN = 12;

void setup_wifi() {
    delay(10);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); }
}

// Esta función se activa cuando llega un mensaje de PHP
void callback(char* topic, byte* payload, unsigned int length) {
    char mensaje = (char)payload[0];
    if (mensaje == '1') digitalWrite(LED_PIN, HIGH);
    else digitalWrite(LED_PIN, LOW);
}

void reconnect() {
    while (!client.connected()) {
        if (client.connect("ESP32Client")) {
            client.subscribe("esp32/led"); // Suscribirse al tópico
        } else {
            delay(5000);
        }
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop(); // Mantener la conexión viva
}
