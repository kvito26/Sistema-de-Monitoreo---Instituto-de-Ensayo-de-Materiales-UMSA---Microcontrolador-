//programa oficial para el control del sistema de monitoreo de IEM-UMSA
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

//IDENTIFICADOR UNICO DEL DISPOSITIVO
const String DEVICE_ID = "esp_amb1";

//CONEXION A LA RED WIFI ESPECIFICA
//const char* ssid = "FLIA_CRUZ_2.4G";
//const char* password = "14378556";
const char* ssid = "vw-03826";
const char* password = "ZTERRTHJ8902852";
//const char* ssid = "IEM-MONITOREO";
//const char* password = "iem-umsa-2026";
//const char* ssid = "DaniJenny";
//const char* password = "DaxJe022";

//================================================
//pines para el sensor de temperatura DHT22
#define DHTTYPE DHT22	
#define DHTPIN 32

DHT dht(DHTPIN, DHTTYPE, 32);

//configuracion del pwm para el ventilador
const int vent = 22; //vetilador (PWM)
const int frecuencia = 3000; //frecuencia: 3000Hz
const int resolucion = 8; // bits: 8

//PINES DE LOS ACTUADORES
//ORDEN: calentador[0], humidificador[1], ventRelay[2]
const int pinesActuadores[3] = {4, 2, 18}; 

//pines para las luces piloto
const int piloto_server = 26;
const int piloto_wifi = 25;


//================================================
//configuracion para la conexion con el servidor websocket
const char* ws_host = "192.168.1.229";
const int ws_port = 8080;

//creando al objeto websocket que mantiene la conexion con el servidor
WebSocketsClient webSocket;

//================================================
//declaracion de varialbles control
bool conexion_server = false;
bool estadoActuadores[3] = {false};
bool estadoTH[2] = {false}; //temp[0], hum[1]
//tolerancias globales modo automatico: tolTemp[0], tolHum[1]
float tolAuto[2] = {0.55f, 1.5f}; 

//se hace esto para guardar en la memoria ram y evitar que se lea constantement la memoria flash
const char feedbackCalent[] = "feedbackCalent";
const char feedbackHumi[] = "feedbackHumi";
const char feedbackVent[] = "feedbackVent";
const char* feedbacksToServer[3] = {feedbackCalent, feedbackHumi, feedbackVent};

//variables de manejo de datos
float lecturasTH[2] = {0.0f, 0.0f};
float configTH[2] = {0.0f, 0.0f};

//=================================================
//PROTOTIPO DE LAS FUNCIONES A USAR
void procesoConectarWiFi();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void envioAutenticacion();
void envioDatosTH();
void envioFeedbacks(const char* action, int estadoActual);
void controlActuadores(int pinActuador, int indice_estado, int valor);
void toggleVent(int valor);
void intensidadVent(int indice_estado, int valor);
void controlAutomatico(bool estadoTH[], int tamano);

//=================================================
//DECLARACION DE LAS TAREAS
TaskHandle_t WebSocketHandle = NULL;
TaskHandle_t EnvioDatosTHHandle = NULL;
TaskHandle_t PilotoServerHandle = NULL;
TaskHandle_t EnvioFeedbacksHandle = NULL;
TaskHandle_t ControlAutomaticoHandle = NULL;

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
//esta tarea se bloquea una vez que se haya conectado al servidor
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

//tarea 4: envio de feedback hacia el servidor
void tareaEnvioFeedbacks(void *parameter){
	for(;;){
		if (conexion_server == true){
			for (int i = 0; i < 3; i++){
				envioFeedbacks(feedbacksToServer[i], estadoActuadores[i]);
			}	
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

//tarea 5: ejecucion del modo automatico
void tareaControlAutomatico(void *parameter){
	for(;;){
		//verificando el estado de la temp y hum en las lecturas
		for (int i = 0; i < 2; i++){
			estadoTH[i] = (lecturasTH[i] >= (configTH[i] + tolAuto[i])) ? true : false;
		}	

		//pasando el array estadoTH a la funcion de control automatico
		controlAutomatico(estadoTH, 2);

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}


//=================================================
void setup() {
	Serial.begin(115200);
	dht.begin();

	//modo de los pines designados (actuadores)
	//aqui el ventRelay tambien pertenece al estado del ventilador
	ledcAttach(vent, frecuencia, resolucion);
	for (int i = 0; i < 3; i++){
		pinMode(pinesActuadores[i], OUTPUT);
	}
	pinMode(piloto_wifi, OUTPUT);
	pinMode(piloto_server, OUTPUT);

	//configurando el estado de los pines
	for (int i = 0; i < 3; i++){
		digitalWrite(pinesActuadores[i], estadoActuadores[i]);
	}
	digitalWrite(piloto_wifi, LOW);
	digitalWrite(piloto_server, LOW);

	//conectando a la red wifi (codigo modular)
	procesoConectarWiFi();

	//configuraciones para el websocket cliente
	webSocket.begin(ws_host, ws_port, "/");
	webSocket.onEvent(webSocketEvent);
	webSocket.setReconnectInterval(5000);

	//***************************************************
	//CREAR TAREAS freeRTOS
	//tarea 1: mantener la conexion con el servidor websocket
	xTaskCreatePinnedToCore(
		tareaWebSocket,
		"WebSocketTask",
		10240, //memoria para la pila
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
		2, //prioridad de la tarea
		&EnvioDatosTHHandle,
		1 //nucleo de procesador
	);

	//tarea 3: segnalizacion de la conexion con el servidor
	xTaskCreatePinnedToCore(
		tareaPilotoServer,
		"PilotoServerTask",
		1024, //memoria de la pila
		NULL,
		0, //prioridad de la tarea
		&PilotoServerHandle,
		0 //nucleo de procesador
	);

	//tarea 4: envio de feedback hacia el servidor
	xTaskCreatePinnedToCore(
		tareaEnvioFeedbacks,
		"EnvioFeedbacksTask",
		4096, //memoria de la pila
		NULL,
		2, //prioridad de la tarea
		&EnvioFeedbacksHandle,
		1 //nucleo de procesador
	);

	//tarea 5: ejecucion del modo automatico
	xTaskCreatePinnedToCore(
		tareaControlAutomatico,
		"ControlAutomaticoTask",
		8192, //memoria de la pila
		NULL,
		2, //prioridad de la tarea
		&ControlAutomaticoHandle,
		0 //nucleo de procesador
	);

	//suspender al inicio ya que no se esta usando el control automatico
	vTaskSuspend(ControlAutomaticoHandle);
	//***************************************************
	Serial.println("[FreeRTOS] Se han creado las tareas para FreeRTOS");
}

//=================================================
void loop() {
//	vTaskDelay(1000 / portTICK_PERIOD_MS); //el tiempo que puede tomar del loop principal
}

//=================================================
//FUNCIONES 
//funcion para la conexion a la red wifi
void procesoConectarWiFi(){
	Serial.print("[WiFi] Conectando a la red: ");
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
		Serial.println("\n[WiFi] Se ha conectado a la red WiFi");
		Serial.print("[WiFi] IP local: ");
		Serial.println(WiFi.localIP());
		digitalWrite(piloto_wifi, HIGH);
	}
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length){
	switch(type){
		case WStype_DISCONNECTED:
			conexion_server = false;
			vTaskResume(PilotoServerHandle);
			vTaskSuspend(EnvioFeedbacksHandle);
			Serial.println("[WS] Desconectado del Servidor IEM");
			break;
		
		case WStype_CONNECTED:
			Serial.println("[WS] Conectado con el Servidor IEM");
			vTaskResume(EnvioFeedbacksHandle);
			vTaskSuspend(PilotoServerHandle);
			digitalWrite(piloto_server, HIGH);
			conexion_server = true;
			envioAutenticacion();
			envioDatosTH();
			break;

		case WStype_TEXT:
			Serial.printf("[WS] Mensaje recibido: %s\n", payload);

			//decodificando json y verificando algun error
			JsonDocument mensaje_rec;
			DeserializationError error_rec = deserializeJson(mensaje_rec, payload);

			//error al decodificar el json
			if (error_rec){
				Serial.print("[JSON] Error al parsear JSON: ");
				Serial.println(error_rec.c_str());
			}

			//**************************************
			//seleccion de las ordenes
			if (mensaje_rec["tipo"] == "ordenCalent"){
				controlActuadores(pinesActuadores[0], 0, mensaje_rec["valor"]);
			}
			else if (mensaje_rec["tipo"] == "ordenHumi"){
				controlActuadores(pinesActuadores[1], 1, mensaje_rec["valor"]);
			}
			else if (mensaje_rec["tipo"] == "ordenVent"){
				toggleVent(2, mensaje_rec["valor"]);
			}
			else if (mensaje_rec["tipo"] == "ordenVent_Var"){
				intensidadVent(2, mensaje_rec["valor"]);
			}
			else if (mensaje_rec["tipo"] == "automatico"){
				configTH[0] = (float) mensaje_rec["temp"];
				configTH[1] = (float) mensaje_rec["hum"];
				activarAutomatico();
				vTaskResume(ControlAutomaticoHandle);
			}
			else if (mensaje_rec["tipo"] == "detener_todo"){
				vTaskSuspend(ControlAutomaticoHandle);
				detenerTodo();
			}
			break;

	}
}

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

	Serial.print("[DHT] Datos enviados: ");
	for (int i = 0; i < 2; i++){
		Serial.printf("\t%.2f", lecturasTH[i]);
	}
	Serial.printf("\n");
}

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

//***********************************************
//CONTROL MANUAL
//funciones de control para los actuadores: calentador y humidificador
void controlActuadores(int pinActuador, int indice_estado, int valor){
	if (estadoActuadores[indice_estado] != (bool) valor){
		digitalWrite(pinActuador, (bool) valor);
		estadoActuadores[indice_estado] = (bool) valor;
	}
}

//funcion para el control del encendido y apagado del ventilador (con rampa)
void toggleVent(int indice_estado, int valor){
	if (estadoActuadores[indice_estado] != (bool) valor){
		estadoActuadores[indice_estado] = (bool) valor;
		if (valor == 1){
			digitalWrite(pinesActuadores[2], HIGH); //aqui el pin del ventRelay
			vTaskDelay(50 / portTICK_PERIOD_MS);
			for (int dutyCycle = 0; dutyCycle <= 130; dutyCycle++){
				ledcWrite(vent, dutyCycle);
				vTaskDelay(10 / portTICK_PERIOD_MS);
			}
		}
		else if (valor != 1){
			for (int dutyCycle = 130; dutyCycle >= 0; dutyCycle--){
				ledcWrite(vent, dutyCycle);
				vTaskDelay(10 / portTICK_PERIOD_MS);
			}
			vTaskDelay(50 / portTICK_PERIOD_MS);
			digitalWrite(pinesActuadores[2], LOW); //aqui el pin del ventRelay
		}
	}
}

//control de la intensidad del ventilador despues del encendido
void intensidadVent(int indice_estado, int valor){
	if (estadoActuadores[indice_estado] == 1){
		ledcWrite(vent, valor);
	}
}

//***********************************************
//para el encendido 
void activarAutomatico(){
	estadoActuadores[0] = true;
	digitalWrite(pinesActuadores[0], estadoActuadores[0]);
	vTaskDelay(10000 / portTICK_PERIOD_MS);

	toggleVent(2, true);

	estadoActuadores[1] = true;
	digitalWrite(pinesActuadores[1], estadoActuadores[1]);
}

//CONTROL AUTOMATICO
void controlAutomatico(bool estadoTH[], int n){
	if (estadoTH[0] == false && estadoTH[1] == false){
		for (int i = 0; i < 2; i++){
			estadoActuadores[i] = true;
			digitalWrite(pinesActuadores[i], estadoActuadores[i]);
		}
		toggleVent(2, true);
		intensidadVent(2, 150);
	}	
	if (estadoTH[0] == false && estadoTH[1] == true){
		estadoActuadores[0] = true;
		estadoActuadores[1] = false;
		digitalWrite(pinesActuadores[1], estadoActuadores[1]);
		for (int i = 0; i < 2; i++){
			digitalWrite(pinesActuadores[i], estadoActuadores[i]);
		}
		toggleVent(2, true);
		intensidadVent(2, 150);
	}	
	if (estadoTH[0] == true && estadoTH[1] == false){
		estadoActuadores[0] = false;
		estadoActuadores[1] = true;
		digitalWrite(pinesActuadores[1], estadoActuadores[1]);
		for (int i = 0; i < 2; i++){
			digitalWrite(pinesActuadores[i], estadoActuadores[i]);
		}
		toggleVent(2, true);
		intensidadVent(2, 114);
	}	
	if (estadoTH[0] == true && estadoTH[1] == true){
		detenerTodo();
	}	
}

//***********************************************
//detener todos los procesos
void detenerTodo(){
	for (int i = 0; i < 2; i++){
		estadoActuadores[i] = false;
		digitalWrite(pinesActuadores[i], estadoActuadores[i]);
	}
	toggleVent(2, false);
}












