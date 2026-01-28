#include "conectar_wifi.h"
#include "sensor_dht.h"
#include "envioDatosServer.h"
#include <ArduinoJson.h>

//credenciales de la red wifi
const char* ssid = "vw-03826";
const char* clave = "ZTERRTHJ8902852";
const char* url_server = "http://192.168.1.224/umsa-iem/public/?action=datos_amb1";

//const char* ssid = "Baratronics2";
//const char* clave = "inicio2021";
//const char* url_server = "http://192.168.0.111/umsa-iem/esp32_amb1_temp_hum.php";

//temperatura y humedad iniciales
float temp0 = 0.0f;
float hum0 = 0.0f;

void setup() {
	Serial.begin(115200);
	delay(10);

	//conectando a red wifi 
	procesoConectarWifi(ssid, clave);
}

void loop() {
	//leyendo temp y hum del sensor
	float tempDHT = leerTemp();
	float humDHT = leerHum();
	
	//imprimiendo valores de temp y humedad
	Serial.print("Temperatura: ");
	Serial.println(tempDHT);
	Serial.print("Humedad: ");
	Serial.println(humDHT);
	Serial.println("--------------------------------");


	if ((temp0 == tempDHT) && (hum0 == humDHT)){
		Serial.println("Datos iguales a los anteriores");
	}
	else {
		JsonDocument datos;
		datos["temp"] = tempDHT;
		datos["hum"] = humDHT;

		Serial.print("\n");
		serializeJsonPretty(datos, Serial);
		Serial.print("\n\n");

		envioDatosTH(tempDHT, humDHT, url_server);

		temp0 = tempDHT;
		hum0 = humDHT;
	}

	delay(5000);
}
