#include "conectar_wifi.h"
#include <Arduino.h>
#include <WiFi.h>

void procesoConectarWifi(const char* ssid, const char* clave){
	//definiendo el modo los pines declarados en el encabezado

	int rojo = 12;
	int azul = 14;
	int verde = 27;

	pinMode(rojo, OUTPUT);
	pinMode(azul, OUTPUT);
	pinMode(verde, OUTPUT);
	pinMode(2, OUTPUT);

	digitalWrite(rojo, LOW);
	digitalWrite(azul, LOW);
	digitalWrite(verde, LOW);

	//realizando la conexion wifi
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, clave);
	Serial.print("Conectado a la red: ");
	Serial.print(ssid);

	int contador_wifi = 0;
	while (WiFi.status() != WL_CONNECTED){
		delay(300);
		Serial.print('.');
		
		if (contador_wifi == 15){
			digitalWrite(rojo, 1);
			Serial.println("");
			Serial.println("Error en la conexion a la red");
			break;	
		}

		digitalWrite(azul, 1);
		digitalWrite(2, 1);
		delay(300);
		digitalWrite(azul, 0);
		digitalWrite(2, 0);
		delay(300);

		contador_wifi++;
	}

	if (WiFi.status() == WL_CONNECTED){
		//cuando ya se haya conectado
		digitalWrite(verde, 1);
		Serial.println("WiFi conectado");

		//mostrar mensajes de conexion
		Serial.println("Se le ha asignado la siguiente direccion IP: ");
		Serial.println(WiFi.localIP());
	}
}

