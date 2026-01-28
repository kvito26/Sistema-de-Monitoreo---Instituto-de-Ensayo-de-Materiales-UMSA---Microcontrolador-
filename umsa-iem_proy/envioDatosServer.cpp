#include "envioDatosServer.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

void envioDatosTH(float temp, float hum, const char* url_server){
	//verificando si existen datos de temperatura y hum
	if (isnan(temp) || isnan(hum)){
		Serial.println("error - datos del sensor DHT vacios");
		return;
	}

	//preparando el json
	JsonDocument datosTH;
	datosTH["temp_amb1"] = temp;
	datosTH["hum_amb1"] = hum;

	String salidaJson;
	serializeJson(datosTH, salidaJson);
	
	//enviando datos hacia el servidor
	HTTPClient http;
	http.begin(url_server);
	http.addHeader("Content-Type", "application/json");

	Serial.print("Enviando JSON: ");
	Serial.println(salidaJson);

	//el codigo de respuesta del servidor
	int responseCode = http.POST(salidaJson);
	Serial.print("Respuesta del servidor: ");
	Serial.println(responseCode);

}

