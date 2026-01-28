#include "sensor_dht.h"
#include <DHT.h>
//definiendo las variables para el sensor
#define DHT_TYPE DHT22
#define DHT_PIN 4

DHT dht(DHT_PIN, DHT_TYPE, 22);

float leerTemp(){
	float temp = dht.readTemperature();
	return temp;
}

float leerHum(){
	float hum = dht.readHumidity();
	return hum;
}
