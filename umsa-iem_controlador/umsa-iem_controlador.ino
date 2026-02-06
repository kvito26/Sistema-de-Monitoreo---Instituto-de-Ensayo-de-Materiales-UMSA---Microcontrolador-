//programa oficial para el control del sistema de monitoreo de IEM-UMSA
#include "conexion_wifi.h"
#include "conexion_server.h"
#include <ArduinoJson.h>
#include <DHT.h>

//IDENTIFICADOR UNICO DEL DISPOSITIVO
const String DEVICE_ID = "esp_amb1";

//================================================
//pines para el sensor de temperatura DHT22
#define DHTTYPE DHT22	
#define DHTPIN 32

DHT dht(DHTPIN, DHTTYPE, 32);

//configuracion del pwm para el ventilador
const int vent = 22; //vetilador (PWM)
const int frecuencia = 3000; //frecuencia: 3000Hz
const int resolucion = 8; // bits: 8

//pines de los actuadores
const int calent = 4; //calentador
const int humi = 2; //humidificador

//pines para las luces piloto
const int piloto_server = 26;

//================================================

//configuracion para la conexion con el servidor websocket
const char* ws_host = "192.168.1.8";
const int ws_port = 8080;

//creando al objeto websocket que mantiene la conexion con el servidor
WebSocketsClient webSocket;

//================================================
//declaracion de varialbles control
bool conexion_server = false;
bool estado_calent = false;
bool estado_humi = false;


//variables de manejo de datos
float temp = 0.0f;
float hum = 0.0f;

//=================================================
//DECLARACION DE LAS TAREAS
TaskHandle_t WebSocketHandle = NULL;
TaskHandle_t EnvioDatosTHHandle = NULL;
TaskHandle_t PilotoServerHandle = NULL;


//TAREAS freeRTOS
//tarea 1: mantener la conexion con el servidor websocket
void tareaWebSocket(void *parameter){
	for(;;){
		webSocket.loop();
		vTaskDelay(5 / portTICK_PERIOD_MS);
	}
}

//tarea 2: envio de los datos de temp y hum al servidor0
void tareaEnvioDatosTH(void *parameter){
	for(;;){
		if (conexion_server == true){
			envioDatosTH();
		}
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}

//tarea 3: segnalizacion de la conexion con el servidor
void tareaPilotoServer(void *parameter){
	for(;;){
		if (conexion_server == false){
			digitalWrite(piloto_server, HIGH);
			vTaskDelay(300 / portTICK_PERIOD_MS); 
			digitalWrite(piloto_server, LOW);
			vTaskDelay(300 / portTICK_PERIOD_MS); 
		}
		vTaskDelay(5 / portTICK_PERIOD_MS);
	 }
}



//=================================================
void setup() {
	Serial.begin(115200);
	dht.begin();

	//modo de los pines designados
	ledcAttach(vent, frecuencia, resolucion);
	pinMode(calent, OUTPUT);
	pinMode(humi, OUTPUT);
//	pinMode(piloto_wifi, OUTPUT);
	pinMode(piloto_server, OUTPUT);

	//configurando el estado de los pines
	digitalWrite(calent, estado_calent);
	digitalWrite(humi, estado_humi);
//	digitalWrite(piloto_wifi, LOW);
	digitalWrite(piloto_server, LOW);

	//conectando a la red wifi (codigo modular)
	procesoConectarWiFi();

	//configuraciones para el websocket cliente
	webSocket.begin(ws_host, ws_port, "/");
	webSocket.onEvent(webSocketEvent);
	webSocket.setReconnectInterval(3000);

	//***************************************************
	//CREAR TAREAS freeRTOS
	//tarea 1: mantener la conexion con el servidor websocket
	xTaskCreatePinnedToCore(
		tareaWebSocket,
		"WebSocketTask",
		8192, //memoria para la pila
		NULL, 
		3, //prioridad de la tarea
		&WebSocketHandle,
		0 //nucleo de procesador
	);

	//tarea 2: envio de los datos de temp y hum al servidor
	xTaskCreatePinnedToCore(
		tareaEnvioDatosTH,
		"EnvioDatosTHTask",
		4096, //memoria de la pila
		NULL,
		4, //prioridad de la tarea
		&EnvioDatosTHHandle,
		1 //nucleo de procesador
	);

	//tarea 3: segnalizacion de la conexion con el servidor
	xTaskCreatePinnedToCore(
		tareaPilotoServer,
		"PilotoServerTask",
		1024, //memoria de la pila
		NULL,
		1, //prioridad de la tarea
		&PilotoServerHandle,
		1 //nucleo de procesador
	);

	//***************************************************
	Serial.println("Se han creado las tareas para FreeRTOS");
}

//=================================================
void loop() {
//	vTaskDelay(1000 / portTICK_PERIOD_MS); //el tiempo que puede tomar del loop principal
}

//=================================================
//FUNCIONES 

//void webSocketEvent(WStype_t type, uint8_t * payload, size_t length){
//	switch(type){
//		case WStype_DISCONNECTED:
//			conexion_server = false;
//			vTaskResume(PilotoServerHandle);
//			Serial.println("[WS] Desconectado del Servidor IEM");
//			break;
//		
//		case WStype_CONNECTED:
//			Serial.println("[WS] Conectado con el Servidor IEM");
//			vTaskSuspend(PilotoServerHandle);
//			digitalWrite(piloto_server, HIGH);
//			conexion_server = true;
//			envioAutenticacion();
//			envioDatosTH();
//			break;
//
//		case WStype_TEXT:
//			Serial.printf("[WS] Mensaje recibido: %s\n", payload);
//			break;
//
//	}
//}

//envio de la autenticacion hacia el servidor
void envioAutenticacion(){
	JsonDocument auth;
	auth["action"] = "autenticar";
	auth["id_disp"] = DEVICE_ID;

	String mensaje;
	serializeJson(auth, mensaje);
	webSocket.sendTXT(mensaje);
}

//envio de los datos de temperatura y humedad
void envioDatosTH(){
	temp = dht.readTemperature();
	hum = dht.readHumidity();

	JsonDocument datos_th;
	datos_th["action"] = "datos_TH";
	datos_th["id_disp"] = DEVICE_ID;
	datos_th["temp"] = temp;
	datos_th["hum"] = hum; 

	String datos_envio;
	serializeJson(datos_th, datos_envio);
	webSocket.sendTXT(datos_envio);

	Serial.println("Datos enviados: ");
	Serial.print("\tTemp.: ");
	Serial.println(temp);
	Serial.print("\tHum.: ");
	Serial.println(hum);
}


