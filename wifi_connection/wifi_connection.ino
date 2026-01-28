//conexion del esp32 a una red wifi
#include <WiFi.h>

int rojo = 12;
int azul = 14;
int verde = 27;

//valores constantes de la red wifi
const char* ssid = "vw-03826";
const char* clave = "ZTERRTHJ8902852";

void setup() {
	Serial.begin(115200);
	delay(10);

	//configuracion y estado de los pilotos
	pinMode(rojo, OUTPUT);
	pinMode(azul, OUTPUT);
	pinMode(verde, OUTPUT);
	pinMode(2, OUTPUT);
	digitalWrite(rojo, LOW);
	digitalWrite(azul, LOW);
	digitalWrite(verde, LOW);

	//conectado a la red wifi
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, clave);
	Serial.print("Conectado a la red: ");
	Serial.print(ssid);

	//esperando a la conexion wifi
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
void loop() {
}
