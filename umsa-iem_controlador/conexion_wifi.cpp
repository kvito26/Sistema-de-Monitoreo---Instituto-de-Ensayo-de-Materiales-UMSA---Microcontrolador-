#include "conexion_wifi.h"
#include <Arduino.h>
#include <WiFi.h>

//funcion para la conexion a la red wifi
void procesoConectarWiFi(){
	//declarando pin para el piloto de conexion de wifi
	const int piloto_wifi = 25;

	pinMode(piloto_wifi, OUTPUT);
	digitalWrite(piloto_wifi, LOW);


	//aqui vienen los ssid y las claves usuales para la conexion
	const char* ssid = "FLIA_CRUZ_2.4G";
	const char* password = "14378556";
	//const char* ssid = "vw-03826";
	//const char* password = "ZTERRTHJ8902852";
	//const char* ssid = "IEM-MONITOREO";
	//const char* password = "iem-umsa-2026";

	Serial.print("Conectando a la red: ");
	Serial.println(ssid);

	//conectar a la red wifi
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED){
		digitalWrite(piloto_wifi, HIGH);		
		Serial.print(".");
		vTaskDelay(300 / portTICK_PERIOD_MS);
		digitalWrite(piloto_wifi, LOW);		
		Serial.print(".");
		vTaskDelay(300 / portTICK_PERIOD_MS);
	}

	if (WiFi.status() == WL_CONNECTED){
		Serial.println("\nSe ha conectado a la red WiFi");
		Serial.print("IP local: ");
		Serial.println(WiFi.localIP());
		digitalWrite(piloto_wifi, HIGH);		
	}	
}
