//programa oficial para el control del sistema de monitoreo de IEM-UMSA
//este programa ha sido reescrito del original umsa-iem_controlador
//se ha realizado algunos cambios, mejoras y optimizaciones
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

//===================================================
//CONFIGURACION DE PINES GPIO
//configuracion para el sensor DHT22
#define DHTTYPE DHT22
#define DHTPIN 32
DHT dht(DHTPIN, DHTTYPE, 32);
//pines para el ventilador (Salida)
#define VENT 22 //pin ventilador PWM
#define FRECUENCIA 3000 //frecuencia PWM 3kHz
#define RESOLUCION 8 //8 bits
//Otros pines (Salida)
#define PILOTO_SERVER 26
#define PILOTO_WIFI 25
//orden: calentador[0], humidificador[1], ventRelay[2]
const int pinesActuadores[3] = {4, 2, 18};
//Otros pines (Entradas)
const int pinesSensores[1] = {34};

//===================================================
//VARIABLES
volatile bool conexion_server = false;
volatile bool estadoActuadores[3] = {false}; //calent[0], humi[1], vent[2]
volatile bool estadoSensoresIn[1] = {false}; //nivelAgua[0]
volatile bool estadoTH[4] = {false}; //tempMax[0], humMax[1], tempMin[2], humMin[3]

//tolerancias globales para el modo automatico
//tolTempMax[0], tolHumMax[1], tolTempMin[2], tolHumMin[3]
volatile float tolAuto[4] = {0.9f, 3.5f, -0.9f, -2.0f};
volatile float lecturasTH[2] = {0.0f};
volatile float configTH[2] = {0.0f};

//variables para el feedback
const char feedbackCalent[] = "feedbackCalent";
const char feedbackHumi[] = "feedbackHumi";
const char feedbackVent[] = "feedbackVent";
const char* feedbacksToServer[3] = {feedbackCalent, feedbackHumi, feedbackVent};
//feedback para los sensores digitales
const char feedbackNivel[] = "feedbackNivel";
const char* feedbacksInToServer[1] = {feedbackNivel};


//===================================================
//IDENTIFICADOR UNICO DEL DISPOSITIVO CONTROLADOR
const String DEVICE_ID = "esp_amb2";

//===================================================
//PARAMETROS DE CONEXION DE RED
//WiFi
const char* SSID = "hola";
const char* PASSWORD = "hola";
//WebSocket
const char* WS_HOST = "127.0.0.1";
const int WS_PORT = 8080;
WebSocketsClient webSocket;

//===================================================
//PROTOTIPO DE LAS FUNCIONES
void procesoConectarWiFi();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void envioAutenticacion();
void envioDatosTH();
void envioFeedbacks(const char* action, int estadoActual);
void controlActuadores(int pinActuador, int indice_estado, int valor);
void toggleVent(int indice_estado, int valor);
void intensidadVent(int indice_estado, int valor);
void activarAutomatico();
void controlAutomatico(volatile bool estadoTH[], int n);
void detenerTodo();

//===================================================
//DECLARACION DEL HANDLE DE LAS TAREAS	
TaskHandle_t WebSocketHandle = NULL;
TaskHandle_t EnvioDatosTHHandle = NULL;
TaskHandle_t PilotoServerHandle = NULL;
TaskHandle_t EnvioFeedbacksHandle = NULL;
TaskHandle_t ControlAutomaticoHandle = NULL;

//===================================================
//DECLARACION DE LOS SEMAFOROS/MUTEX
//.SemaphoreHandle_t 


//===================================================
//LAS TAREAS DE FREERTOS
//tarea 1. Mantiene la conexion con el sevidor WS
void WebSocketTask(void *parameter){
	for (;;){
		webSocket.loop();
		vTaskDelay(5 / portTICK_PERIOD_MS);
		//Serial.printf("WebSocketTask Free Stack: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
	}	
}

//tarea 2. Envio de datos de temp y hum al servidor
void EnvioDatosTHTask(void *parameter){
	for (;;){
		if (conexion_server == true){
			envioDatosTH();
		}
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		Serial.printf("EnvioDatosTHTask Free Stack: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
	}
}

//tarea 3. piloto de la conexion con el servidor WS
void PilotoServerTask(void *parameter){
	for (;;){
		if (conexion_server == true){
			digitalWrite(PILOTO_SERVER, HIGH);
			vTaskDelay(300 / portTICK_PERIOD_MS);
			digitalWrite(PILOTO_SERVER, LOW);
			vTaskDelay(300 / portTICK_PERIOD_MS);
		}
		vTaskDelay(5 / portTICK_PERIOD_MS);
		Serial.printf("PilotoServerTask Free Stack: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
	}
}

//tarea 4. envio de feedbacks hacia el servidor 
void EnvioFeedbacksTask(void *parameter){
	for (;;){
		if (conexion_server == true){
			//leer el nivel de agua
			estadoSensoresIn[0] = (bool) digitalRead(pinesSensores[0]);
			if (estadoSensoresIn[0] == false){
				//esperar y detener todo si el tanque esta vacio
				vTaskDelay(60000 / portTICK_PERIOD_MS);
				vTaskSuspend(ControlAutomaticoHandle);
				detenerTodo();
			}

			//enviar los feedbacks (estados) de entradas y salidas
			for (short int i = 0; i < (short int) (sizeof(feedbacksInToServer) / sizeof(feedbacksInToServer[0])); i++){
				envioFeedbacks(feedbacksInToServer[i], estadoSensoresIn[i]);
			}
			for (short int i = 0; i < (short int) (sizeof(feedbacksToServer) / sizeof(feedbacksToServer[0])); i++){
				envioFeedbacks(feedbacksToServer[i], estadoActuadores[i]);
			}
		}
		vTaskDelay(450 / portTICK_PERIOD_MS);
		Serial.printf("EnvioFeedbacksTask Free Stack: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
	}
}

//tarea 5. control del modo automatico
void ControlAutomaticoTask(void *parameter){
	for (;;){
		//verificando el estado de la temp y hum en las lecturas
		for (short int i = 0; i < 2; i++){
			estadoTH[i] = (lecturasTH[i] > (configTH[i] + tolAuto[i])) ? true : false;
		}
		for (short int i = 0, j = 2; j < 4; i++, j++){
			estadoTH[j] = (lecturasTH[i] <= (configTH[i] + tolAuto[j])) ? true : false; 
		}
		//pasando el array estadoTH a la funcion del control automatico
		controlAutomatico(estadoTH, 4);
		vTaskDelay(100 / portTICK_PERIOD_MS);
		Serial.printf("ControlAutomaticoTask Free Stack: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
	}
}


//===================================================
void setup() {
	Serial.begin(115200);
	dht.begin();

	//**************************************************
	//configuracion de PWM para el ventilador
	ledcAttach(VENT, FRECUENCIA, RESOLUCION);
	//modo de los pines para los actuadores
	for (short int i = 0; i < (short int) (sizeof(pinesActuadores) / sizeof(pinesActuadores[0])); i++){
		pinMode(pinesActuadores[i], OUTPUT);
	}
	//mode de los pines para las entradas
	for (short int i = 0; i < (short int) (sizeof(pinesSensores) / sizeof(pinesSensores[0])); i++){
		pinMode(pinesActuadores[i], INPUT);
	}
	//modo de los pines pilotos
	pinMode(PILOTO_WIFI, OUTPUT);
	pinMode(PILOTO_SERVER, OUTPUT);

	//**************************************************
	//para el estado de los pines actuadores
	for (short int i = 0; i < (short int) (sizeof(estadoActuadores) / sizeof(estadoActuadores[0])); i++){
		digitalWrite(pinesActuadores[i], estadoActuadores[i]);
	}
	//para el estado de los pilotos
	digitalWrite(PILOTO_WIFI, LOW);
	digitalWrite(PILOTO_SERVER, LOW);

	//**************************************************
	//Conectando a la red WiFi y configurando el WebSocket Client
	procesoConectarWiFi();

	webSocket.begin(WS_HOST, WS_PORT, "/");
	webSocket.onEvent(webSocketEvent);
	webSocket.setReconnectInterval(5000);

	//**************************************************
	//Creacion de las Tareas FreeRTOS
	//tarea 1. Mantiene la conexion con el sevidor WS
	xTaskCreatePinnedToCore(
		WebSocketTask,
		"WebSocketTask",
		32768, //memoria para el stack
		NULL,
		3, //prioridad de la tarea
		&WebSocketHandle,
		0
	);

	//tarea 2. Envio de datos de temp y hum al servidor
	xTaskCreatePinnedToCore(
		EnvioDatosTHTask,
		"EnvioDatosTHTask",
		5120, //memoria para el stack
		NULL,
		2, //prioridad de la tarea
		&EnvioDatosTHHandle,
		1
	);

	//tarea 3. piloto de la conexion con el servidor WS
	xTaskCreatePinnedToCore(
		PilotoServerTask,
		"PilotoServerTask",
		1024, //memoria para el stack
		NULL,
		0, //prioridad de la tarea
		&PilotoServerHandle,
		0
	);

	//tarea 4. envio de feedbacks hacia el servidor 
	xTaskCreatePinnedToCore(
		EnvioFeedbacksTask,
		"EnvioFeedbacksTask",
		10240, //memoria para el stack
		NULL,
		2, //prioridad de la tarea
		&EnvioFeedbacksHandle,
		1
	);

	//tarea 5. control del modo automatico
	xTaskCreatePinnedToCore(
		ControlAutomaticoTask,
		"ControlAutomaticoTask",
		10240, //memoria para el stack
		NULL,
		2, //prioridad de la tarea
		&ControlAutomaticoHandle,
		0
	);

	vTaskSuspend(ControlAutomaticoHandle);
}

//===================================================
void loop() {
	static uint32_t lastCheck = 0;
	if (millis() - lastCheck > 5000){
		Serial.printf("Free Heap: %u bytes\n", xPortGetFreeHeapSize());
		lastCheck = millis();
	}
}

//===================================================
//FUNCIONES
//**************************************************
//conexion a la red wifi
void procesoConectarWiFi(){
	Serial.printf("[WiFi] Conectando a la red: ");
	Serial.println(SSID);

	//conectar a la red wifi
	WiFi.begin(SSID, PASSWORD);
	while (WiFi.status() != WL_CONNECTED){
		digitalWrite(PILOTO_WIFI, HIGH);
		Serial.print(".");
		vTaskDelay(300 / portTICK_PERIOD_MS);
		digitalWrite(PILOTO_WIFI, LOW);
		Serial.print(".");
		vTaskDelay(300 / portTICK_PERIOD_MS);
	}

	if (WiFi.status() == WL_CONNECTED){
		Serial.printf("\n[WiFi] Se ha conectado a la red WiFi\n");
		Serial.printf("[WiFi] IP Local: ");
		Serial.println(WiFi.localIP());
		digitalWrite(PILOTO_WIFI, HIGH);
	}
}

//**************************************************
//eventos para el cliente WebSocket
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length){
	switch (type){
		case WStype_DISCONNECTED:
			conexion_server = false;
			vTaskResume(PilotoServerHandle);
			vTaskSuspend(EnvioDatosTHHandle);
			vTaskSuspend(EnvioFeedbacksHandle);
			vTaskSuspend(ControlAutomaticoHandle);
			Serial.printf("[WS] Desconectado con el Servidor IEM\n");
			break;

		case WStype_CONNECTED:
			Serial.printf("[WS] Conectado con el Servidor IEM\n");
			digitalWrite(PILOTO_SERVER, HIGH);
			conexion_server = true;
			envioAutenticacion(); //enviar autenticacion apenas se haya conectado
			vTaskSuspend(PilotoServerHandle);
			vTaskResume(EnvioDatosTHHandle);
			vTaskResume(EnvioFeedbacksHandle);
			//aqui no puede entrar el de control automatico
			break;

		case WStype_TEXT:
			break;
	}
}

//**************************************************
//envio de la autenticacion al servidor
void envioAutenticacion(){
	JsonDocument auth;
	auth["action"] = "autenticar";
	auth["id_disp"] = DEVICE_ID;

	String mensaje;
	serializeJson(auth, mensaje);
	webSocket.sendTXT(mensaje);
}

//**************************************************
//envio de datos de temperatura y humedad
void envioDatosTH(){
	lecturasTH[0] = dht.readTemperature();
	lecturasTH[1] = dht.readHumidity();

	JsonDocument datos_th;
	datos_th["action"] = "datos_TH";
	datos_th["id_disp"] = DEVICE_ID;
	datos_th["temp"] = lecturasTH[0];
	datos_th["hum"] = lecturasTH[1];

	String datos_envio;
	serializeJson(datos_th, datos_envio);
	webSocket.sendTXT(datos_envio);

	Serial.printf("\n[DHT] Datos enviados: ");
	for (short int i = 0; i < 2; i++){
		Serial.printf("\t%.2f", lecturasTH[i]);
	}
	Serial.printf("\n");
}

//**************************************************
//envio de los feedbacks de los pines
void envioFeedbacks(const char* action, int estadoActual){
	JsonDocument feedback;
	feedback["action"] = action;
	feedback["id_disp"] = DEVICE_ID;
	feedback["estado_ahora"] = estadoActual;

	String mensaje;
	serializeJson(feedback, mensaje);
	webSocket.sendTXT(mensaje);
}

//**************************************************
//Control Manual: control de los actuadores calentador y humidificador
void controlActuadores(int pinActuador, int indice_estado, int valor){
	if (estadoActuadores[indice_estado] != (bool) valor){
		digitalWrite(pinActuador, (bool) valor);
		estadoActuadores[indice_estado] = (bool) valor;
	}
}

//**************************************************
//Control Manual: control del encendido y apagado del ventilador con rampa 
void toggleVent(int indice_estado, int valor){
	if (estadoActuadores[indice_estado] != (bool) valor){
		estadoActuadores[indice_estado] = (bool) valor;
		if (valor == 1){
			digitalWrite(pinesActuadores[2], HIGH); //cerrar conexion relay
			vTaskDelay(50 / portTICK_PERIOD_MS);
			for (int dutyCycle = 0; dutyCycle <= 130; dutyCycle++){
				ledcWrite(VENT, dutyCycle);
				vTaskDelay(10 / portTICK_PERIOD_MS);
			}
		}
		else {
			for (int dutyCycle = 130; dutyCycle >= 0; dutyCycle--){
				ledcWrite(VENT, dutyCycle);
				vTaskDelay(10 / portTICK_PERIOD_MS);
			}
			vTaskDelay(50 / portTICK_PERIOD_MS);
			digitalWrite(pinesActuadores[2], LOW); //abrir conexion relay
		}
	}
}


//**************************************************
//Control Manual: control de intensidad del ventilador despues del encendido
void intensidadVent(int indice_estado, int valor){
	if (estadoActuadores[indice_estado] == 1){
		ledcWrite(VENT, valor);
	}
}

//**************************************************
//Control Automatico: inciar control automaico
void activarAutomatico(){
	//activando el calentador
	estadoActuadores[0] = true;
	digitalWrite(pinesActuadores[0], estadoActuadores[0]);
	vTaskDelay(10000 / portTICK_PERIOD_MS);

	//esperar 10s y activar el ventilador
	toggleVent(2, true);

	//activar el humidificador (con interlock de seguridad)
	if (estadoSensoresIn[0] == true){
		estadoActuadores[1] = true;
		digitalWrite(pinesActuadores[1], estadoActuadores[1]);
	}
	else {
		estadoActuadores[1] = false;
	}
}

//**************************************************
//Control Automatico: control automatico
void controlAutomatico(volatile bool estadoTH[], int n){
	//para este caso solo se activa el calentador
	if (estadoTH[0] == false && estadoTH[1] == false && estadoTH[2] == true && estadoTH[3] == true){
		//verificando el nivel de agua (interlock), entonces todo activado
		estadoActuadores[0] = true;
		estadoActuadores[1] = false;
		digitalWrite(pinesActuadores[0], estadoActuadores[0]);
		for (int i = 0; i < 2; i++){
			digitalWrite(pinesActuadores[i], estadoActuadores[i]);
		}
		toggleVent(2, true);
		intensidadVent(2, 135);
	}	
	if (estadoTH[0] == false && estadoTH[1] == true && estadoTH[2] == true && estadoTH[3] == false){
		estadoActuadores[0] = true;
		estadoActuadores[1] = false;
		digitalWrite(pinesActuadores[1], estadoActuadores[1]);
		for (int i = 0; i < 2; i++){
			digitalWrite(pinesActuadores[i], estadoActuadores[i]);
		}
		toggleVent(2, true);
		intensidadVent(2, 135);
	}	
	if (estadoTH[0] == true && estadoTH[1] == false && estadoTH[2] == false && estadoTH[3] == true){
		//interlock de seguridad para el nivel del agua
		if (estadoSensoresIn[0] == true){
			estadoActuadores[0] = false;
			estadoActuadores[1] = true;
			digitalWrite(pinesActuadores[1], estadoActuadores[1]);
			for (int i = 0; i < 2; i++){
				digitalWrite(pinesActuadores[i], estadoActuadores[i]);
			}
			toggleVent(2, false);
		//	intensidadVent(2, 114);
		}
		else {
			detenerTodo();
		}
	}	
	if (estadoTH[0] == true && estadoTH[1] == true && estadoTH[2] == false && estadoTH[3] == false){
		detenerTodo();
	}	
}

//**************************************************
//Detener Todos los Procesos
void detenerTodo(){
	for (int i = 0; i < 2; i++){
		estadoActuadores[i] = false;
		digitalWrite(pinesActuadores[i], estadoActuadores[i]);
	}
	toggleVent(2, false);
}
