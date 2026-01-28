//programa para el envio de datos de temperatura y humedad hacia el servidor websocket
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

//definiendo los pines para el sensor de temp y hum
#define DHTTYPE DHT22
#define DHTPIN 32

DHT dht(DHTPIN, DHTTYPE, 32);
int led = 22; //ventilador
int led2 = 4; //calentador
int led3 = 2; //humidificador
//estados de los pines
bool estado_led = 0;
bool estado_led2 = 0;
bool estado_led3 = 0;

//configurando la conexion a wifi
//const char* ssid = "kevito_sam";
//const char* password = "12345kev";
const char* ssid = "vw-03826";
const char* password = "ZTERRTHJ8902852";
//const char* ssid = "BARATRONICS";
//const char* password = "inicio2021";
//const char* ssid = "DaniJenny";
//const char* password = "DaxJe022";

//configuracion para la direccion del servidor websocket
//const char* ws_host = "10.245.59.12";
const char* ws_host = "192.168.1.229";
const int ws_port = 8080;

//creando al objeto websocket que guarda la conexion entre el servidor WebSocket
WebSocketsClient webSocket;
bool conexion_server = false;

bool ejecutarIncremento = false;
bool ejecutarDecremento = false;

//identificador unico para este dispostivo esp32
const String DEVICE_ID = "esp_amb1";


//prototipo de funciones generales
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void envioDatosTH();
void envioFeedbackVent(int nuevoEstado); 
void envioFeedbackCalent(int nuevoEstado); 
void envioFeedbackHumi(int nuevoEstado); 
void incrementarVel();
void decrementarVel();


//******************************************************
//A PARTIR DE AQUI LAS TAREAS DE FREERTOS
//tarea 1: mantener la conexion con el servidor websocket
void tareaWebSocket(void *parameter){
	for(;;){
		webSocket.loop();
		//agregando un delay de 5ms
		vTaskDelay(5 / portTICK_PERIOD_MS);
	}
}

//tarea 2: enviar los datos de temperatura cada 5 segundos
void tareaEnvioDatosTH(void *parameter){
	for(;;){
		if(conexion_server == true){
			envioDatosTH();
		}
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}

//tarea 3: enviar feedbacks de los estados
void tareaEnvioFeedbacks(void *parameter){
	for(;;){
		if(conexion_server == true){
			if (ejecutarIncremento != true || ejecutarDecremento != true){
				//envioFeedbackVent(estado_led);
			}
			//envioFeedbackCalent(estado_led2);
			//envioFeedbackHumi(estado_led3);
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

//tarea 4: incrementar pwm
void tareaVelocidadPWM(void *parameter){
	for(;;){
		if(conexion_server == true){
			if (estado_led == 1 && ejecutarIncremento == true){
				incrementarVel();
				ejecutarIncremento = false;
			}
			if (estado_led == 0 && ejecutarDecremento == true){
				decrementarVel();
				ejecutarDecremento = false;
			}
		}
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
}

//******************************************************
void setup() {
	Serial.begin(115200);
	dht.begin();

	//el modo de los pines
	pinMode(led, OUTPUT);
//	digitalWrite(led, estado_led);
	pinMode(led2, OUTPUT);
	digitalWrite(led2, estado_led2);
	pinMode(led3, OUTPUT);
	digitalWrite(led3, estado_led3);

	//conectando a wifi
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED){
		delay(500);
		Serial.print(".");
	}
	Serial.println("\nSe ha conectado a la red WiFi");

	//configurando el websocket cliente
	webSocket.begin(ws_host, ws_port, "/");
	webSocket.onEvent(webSocketEvent); //llamando a la funcion
	webSocket.setReconnectInterval(5000);

	
	//******************************************************
	//CREANDO LAS TAREAS PARA FREERTOS
	//tarea 1: mantener la conexion con el servidor websocket
	xTaskCreatePinnedToCore(
		tareaWebSocket,
		"WebSocketTask",
		4096,
		NULL,
		3, //prioridad de tarea
		NULL,
		0 //nucleo del procesador
	);
	//tarea 2: enviar los datos de temperatura cada 5 segundos
	xTaskCreatePinnedToCore(
		tareaEnvioDatosTH,
		"EnvioDatosTHTask",
		4096,
		NULL,
		1, //prioridad de tarea
		NULL,
		1 //nucleo del procesador
	);
	//tarea 3: enviar feedbacks de los estados
	xTaskCreatePinnedToCore(
		tareaEnvioFeedbacks,
		"EnvioFeedbacksTask",
		4096,
		NULL,
		2, //prioridad de tarea
		NULL,
		1 //nucleo del procesador
	);
	//tarea 4: incrementar pwm
	xTaskCreatePinnedToCore(
		tareaVelocidadPWM,
		"VelocidadPWMTask",
		4096,
		NULL,
		2,
		NULL,
		1
	);

	Serial.println("Se han creado las tareas para FreeRTOS");
}


void loop() {
	//haciendo que el loop no trabaje a maxima frecuencia
	vTaskDelay(1000 / portTICK_PERIOD_MS);
}

//funcion para la gestion de eventos del websocket
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length){
	switch(type){
		case WStype_DISCONNECTED:
			Serial.println("[WS] Desconectado del servidor");
			conexion_server = false;
			break;

		case WStype_CONNECTED:
			Serial.println("[WS] Conectado con el servidor");
			Serial.println("\tEnviando llave de autenticacion...");
			//aqui la funcion que envia la llave
			envioAutenticacion();
			conexion_server = true;
			break;

		case WStype_TEXT:
			Serial.printf("[WS] Mensaje recibido: %s\n", payload);

			//convirtiendo en json
			JsonDocument mensaje_rec;
			DeserializationError error = deserializeJson(mensaje_rec, payload);
			
			if (error){
				Serial.print("Error al parsear JSON: ");
				Serial.println(error.c_str());
				return;
			}
			
			bool valor = 0;
			//verificando lo que nos interesa en el JSON
			if (mensaje_rec["tipo"] == "ordenVent"){
				valor = (bool) mensaje_rec["valor"];
				//Serial.print("Este es el valor: ");
				//Serial.println(valor);
				//digitalWrite(led, valor);
				if (valor == 1){
					estado_led = valor;
					ejecutarIncremento = true;
					ejecutarDecremento = false;
				}
				else if (valor == 0){
					estado_led = valor;
					ejecutarIncremento = false;
					ejecutarDecremento = true;
				}
			}
			else if (mensaje_rec["tipo"] == "ordenCalent"){
				valor = (bool) mensaje_rec["valor"];
				//Serial.print("Este es el valor: ");
				//Serial.println(valor);
				digitalWrite(led2, valor);
				envioFeedbackCalent(valor);
				estado_led2 = valor;
			}
			else if (mensaje_rec["tipo"] == "ordenHumi"){
				valor = (bool) mensaje_rec["valor"];
				//Serial.print("Este es el valor: ");
				//Serial.println(valor);
				digitalWrite(led3, valor);
				envioFeedbackHumi(valor);
				estado_led3 = valor;
			}
			break;

	}
}

//funcion de autenticacion
void envioAutenticacion(){
	JsonDocument auth;
	auth["action"] = "autenticacion";
	auth["id_disp"] = DEVICE_ID;

	String mensaje;
	serializeJson(auth, mensaje);
	webSocket.sendTXT(mensaje);
}

void envioFeedbackVent(int nuevoEstado){
	JsonDocument feedback;
	feedback["action"] = "feedbackVent";
	feedback["id_disp"] = DEVICE_ID;
	feedback["estado_ahora"] = nuevoEstado;	

	String mensaje;
	serializeJson(feedback, mensaje);
	webSocket.sendTXT(mensaje);
}

void envioFeedbackCalent(int nuevoEstado){
	JsonDocument feedback;
	feedback["action"] = "feedbackCalent";
	feedback["id_disp"] = DEVICE_ID;
	feedback["estado_ahora"] = nuevoEstado;	

	String mensaje;
	serializeJson(feedback, mensaje);
	webSocket.sendTXT(mensaje);
}

void envioFeedbackHumi(int nuevoEstado){
	JsonDocument feedback;
	feedback["action"] = "feedbackHumi";
	feedback["id_disp"] = DEVICE_ID;
	feedback["estado_ahora"] = nuevoEstado;	

	String mensaje;
	serializeJson(feedback, mensaje);
	webSocket.sendTXT(mensaje);
}

void envioDatosTH(){
	float temp = dht.readTemperature();
	float hum = dht.readHumidity();

	//imprimir datos de temp y hum en la terminal arduino 
	Serial.println("\nSe envian los datos de temp y hum:");
	Serial.print("\tTemperatura: ");
	Serial.println(temp);
	Serial.print("\tHumedad: ");
	Serial.println(hum);
	
	JsonDocument datos;
	datos["action"] = "datos_TH";
	datos["id_disp"] = DEVICE_ID;
	datos["temp"] = temp;
	datos["hum"] = hum;

	String datos_out;
	serializeJson(datos, datos_out);
	webSocket.sendTXT(datos_out);
	Serial.println("Datos TH enviados exitosamente");

}

//funcion para incrementar pwm para el ventilador
void incrementarVel(){
	for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle++){   
		// changing the LED brightness with PWM
		analogWrite(led, dutyCycle);
		vTaskDelay(15 / portTICK_PERIOD_MS);
	}
}

//funcion para decrementar la velocidad del ventilador
void decrementarVel(){
	for(int dutyCycle = 255; dutyCycle >= 70; dutyCycle--){
		// changing the LED brightness with PWM
		analogWrite(led, dutyCycle);
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}


