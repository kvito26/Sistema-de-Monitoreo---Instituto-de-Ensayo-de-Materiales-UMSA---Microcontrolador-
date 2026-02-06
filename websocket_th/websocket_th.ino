//programa para el envio de datos de temperatura y humedad hacia el servidor websocket
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

//definiendo los pines para el sensor de temp y hum
#define DHTTYPE DHT22
#define DHTPIN 32

DHT dht(DHTPIN, DHTTYPE, 32);
const int vent = 22; //ventilador
const int vent_relay = 18;
const int calent = 4; //calentador
const int humi = 2; //humidificador
//const int nivel_agua = 34; //sensor de nivel de agua
//luces piloto para indicadores/debug
const int piloto_wifi = 25;
const int piloto_server = 26;
//const int piloto_vent = 27;
//const int piloto_calent = 14;
//const int piloto_humi = 12;
/*INSTRUCCIONES PILOTO
	* Para el piloto_wifi:
		* Parpadeante cuando esta intentando conectarse a red wifi
		* Encedido constante cuando se ha logrado conectar
		* Sa apaga cuando no se conecta y si se ha desconectado
	* Para el piloto_server:
		* Parpadeante cuando se esta intentando conectar con el servidor
		* Encendido constante cuando ya esta conectado, parpadeo inverso al enviar datos.
		* Parpadeo cuando ha perdido la conexion y se esta intentado conectar
	* Para el piloto_vent:
		* El mismo comportamiento del pwm del pin vent.
	* Para los pilotos calent y humi:
		* El mismo comportamiento de los pines digitales calent y humi
*/

//estados de los pines
bool estado_vent = 0;
bool estado_calent = 0;
bool estado_humi = 0;

//configurando la conexion a wifi
//const char* ssid = "kevito_sam";
//const char* password = "12345kev";
//const char* ssid = "vw-03826";
//const char* password = "ZTERRTHJ8902852";
//const char* ssid = "BARATRONICS";
//const char* password = "inicio2021";
const char* ssid = "DaniJenny";
const char* password = "DaxJe022";
//const char* ssid = "FLIA_CRUZ_2.4G";
//const char* password = "14378556";
//const char* ssid = "IEM-MONITOREO";
//const char* password = "iem-umsa-2026";

//configuracion para la direccion del servidor websocket
//const char* ws_host = "10.245.59.12";
const char* ws_host = "192.168.1.149";
const int ws_port = 8080;

//creando al objeto websocket que guarda la conexion entre el servidor WebSocket
WebSocketsClient webSocket;
bool conexion_server = false;

bool ejecutarIncremento = false;
bool ejecutarDecremento = false;
bool estado_temp = false;
bool estado_hum = false;
bool automatico = false;

//identificador unico para este dispostivo esp32
const String DEVICE_ID = "esp_amb1";

float temp = 0.0f; //lecutura de temperatura del sensor
float hum = 0.0f; //lectura de humedad del sensor
float temp_rec = 0.0f; //set point de temperatura en modo automatico
float hum_rec = 0.0f; //set point de humedad en modo automatico
float tol_temp = 1.5f; //tolerancia de temp para el modo automatico
float tol_hum = 2.0f; //tolerancia de temp para el modo automatico


//prototipo de funciones generales
void conectar_wifi();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void pilotoServer(bool conexion_server);
void envioDatosTH();
void envioFeedbackVent(int nuevoEstado); 
void envioFeedbackCalent(int nuevoEstado); 
void envioFeedbackHumi(int nuevoEstado); 
void incrementarVel();
void decrementarVel();
void controlAuto(bool estado_temp, bool estado_hum);

//******************************************************
//DECLARACION DE LOS HANDLES
TaskHandle_t automaticoTempHumHandle = NULL;

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
				envioFeedbackVent(estado_vent);
			}
			envioFeedbackCalent(estado_calent);
			envioFeedbackHumi(estado_humi);
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

//tarea 4: incrementar pwm
void tareaVelocidadPWM(void *parameter){
	for(;;){
		if(conexion_server == true){
			if (estado_vent == 1 && ejecutarIncremento == true){
				incrementarVel();
				ejecutarIncremento = false;
			}
			if (estado_vent == 0 && ejecutarDecremento == true){
				decrementarVel();
				ejecutarDecremento = false;
			}
		}
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
}

//tarea 5: verificar valores de temperatura y humedad
void tareaAutoTempHum(void *parameter){
	for(;;){
		//guardando los estados de las condiciones
		estado_temp = (temp >= (temp_rec + tol_temp)) ? true : false;
		estado_hum = (hum >= (hum_rec + tol_hum)) ? true : false;

		controlAuto(estado_temp, estado_hum);
		
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}


//void tareaPilotoServer(void *parameter){
//	for(;;){
//		if (conexion_server == false){
//			digitalWrite(piloto_server, LOW);
//		}
//		else {
//			digitalWrite(piloto_server, HIGH);
//		}
//	}
//	vTaskDelay(10 / portTICK_PERIOD_MS);
//}

//******************************************************
void setup() {
	Serial.begin(115200);
	dht.begin();

	//el modo de los pines
	pinMode(vent, OUTPUT);
	pinMode(vent_relay, OUTPUT);
	pinMode(calent, OUTPUT);
	pinMode(humi, OUTPUT);
	analogWrite(vent, 0);
	digitalWrite(vent_relay, LOW);
	digitalWrite(calent, estado_calent);
	digitalWrite(humi, estado_humi);
	 
	//el modo de los pines piloto
	pinMode(piloto_wifi, OUTPUT);
	pinMode(piloto_server, OUTPUT);
	//pinMode(piloto_vent, OUTPUT);
	//pinMode(piloto_calent, OUTPUT);
	//pinMode(piloto_humi, OUTPUT);
	digitalWrite(piloto_server, conexion_server);

	//realizando la conexion wifi
	conectar_wifi();

	//configurando el websocket cliente
	webSocket.begin(ws_host, ws_port, "/");
	webSocket.onEvent(webSocketEvent); //llamando a la funcion
	webSocket.setReconnectInterval(3000);

	
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

	//tarea 5: verificar valores de temperatura y humedad
	xTaskCreatePinnedToCore(
		tareaAutoTempHum,
		"AutoTempHumTask",
		4096,
		NULL,
		2,
		&automaticoTempHumHandle,
		0
	);

//	xTaskCreatePinnedToCore(
//		tareaPilotoServer,
//		"PilotoServerTask",
//		1024,
//		NULL,
//		1, //prioridad de tarea
//		NULL,
//		0 //nucleo del procesador
//	);

	vTaskSuspend(automaticoTempHumHandle);

	Serial.println("Se han creado las tareas para FreeRTOS");

	envioFeedbackVent(estado_vent);
	envioFeedbackCalent(estado_calent);
	envioFeedbackHumi(estado_humi);
}


//*******************************************************
void loop() {
	//haciendo que el loop no trabaje a maxima frecuencia
	vTaskDelay(1000 / portTICK_PERIOD_MS);
}

//*******************************************************
//funcion para realizar la conexion a la red wifi
void conectar_wifi(){
	Serial.print("Conectando a la red: ");
	Serial.println(ssid);

	//conectando a wifi
	WiFi.begin(ssid, password);
	int contador_wifi = 0;

	while (WiFi.status() != WL_CONNECTED){
		if (contador_wifi > 300){
			digitalWrite(piloto_wifi, LOW);
			Serial.println("Error en la conexion WiFi");
			break;
		}
		digitalWrite(piloto_wifi, HIGH);
		delay(300);
		Serial.print(".");
		digitalWrite(piloto_wifi, LOW);
		delay(300);
		Serial.print(".");
		contador_wifi++;
	}

	if (WiFi.status() == WL_CONNECTED){
		Serial.println("\nSe ha conectado a la red WiFi");
		Serial.print("IP: ");
		Serial.println(WiFi.localIP());
		digitalWrite(piloto_wifi, HIGH);
	}
}

//funcion para la gestion de eventos del websocket
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length){
	switch(type){
		case WStype_DISCONNECTED:
			Serial.println("[WS] Desconectado del servidor");
			conexion_server = false;
			digitalWrite(piloto_server, LOW);
			detener();
			break;

		case WStype_CONNECTED:
			Serial.println("[WS] Conectado con el servidor");
			Serial.println("\tEnviando llave de autenticacion...");
			//aqui la funcion que envia la llave
			envioAutenticacion();
			conexion_server = true;
			digitalWrite(piloto_server, HIGH);
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
			
			int valor = 0;
			//verificando lo que nos interesa en el JSON
			if (mensaje_rec["tipo"] == "ordenVent" && automatico == false){
				valor = (bool) mensaje_rec["valor"];
				//Serial.print("Este es el valor: ");
				//Serial.println(valor);
				//digitalWrite(vent, valor);
				if (valor == 1){
					estado_vent = valor;
					ejecutarIncremento = true;
					ejecutarDecremento = false;
				}
				else if (valor == 0){
					estado_vent = valor;
					decrementarVel();
					ejecutarIncremento = false;
					ejecutarDecremento = true;
				}
				envioFeedbackVent(estado_vent);
			}
			else if (mensaje_rec["tipo"] == "ordenCalent" && automatico == false){
				valor = (bool) mensaje_rec["valor"];
				//Serial.print("Este es el valor: ");
				//Serial.println(valor);
				digitalWrite(calent, valor);
				envioFeedbackCalent(valor);
				estado_calent = valor;
			}
			else if (mensaje_rec["tipo"] == "ordenHumi" && automatico == false){
				valor = (bool) mensaje_rec["valor"];
				//Serial.print("Este es el valor: ");
				//Serial.println(valor);
				digitalWrite(humi, valor);
				envioFeedbackHumi(valor);
				estado_humi = valor;
			}
			else if (mensaje_rec["tipo"] == "ordenVent_Var" && automatico == false){
				valor = (int) mensaje_rec["valor"];
				if (estado_vent == 1){
					analogWrite(vent, valor);
				}
			}
			else if (mensaje_rec["tipo"] == "automatico" && automatico == false){
				temp_rec = (float) mensaje_rec["temp"];
				hum_rec = (float) mensaje_rec["hum"];
				automatico = true;
				activarAuto();
				vTaskResume(automaticoTempHumHandle);
			}
			else if (mensaje_rec["tipo"] == "detener_todo"){
				vTaskSuspend(automaticoTempHumHandle);
				automatico = false;
				detener();
			}
			break;
		
	}
}

//funcion de autenticacion
void envioAutenticacion(){
	JsonDocument auth;
	auth["action"] = "autenticar";
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
	temp = dht.readTemperature();
	hum = dht.readHumidity();

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
	digitalWrite(vent_relay, HIGH);
	vTaskDelay(50 / portTICK_PERIOD_MS);
	for(int dutyCycle = 0; dutyCycle <= 150; dutyCycle++){   
		// changing the LED brightness with PWM
		analogWrite(vent, dutyCycle);
		vTaskDelay(15 / portTICK_PERIOD_MS);
	}
}

//funcion para decrementar la velocidad del ventilador
void decrementarVel(){
	if (estado_vent == 1){
		for(int dutyCycle = 150; dutyCycle >= 0; dutyCycle--){
			// changing the LED brightness with PWM
			analogWrite(vent, dutyCycle);
			vTaskDelay(10 / portTICK_PERIOD_MS);
		}
		vTaskDelay(50 / portTICK_PERIOD_MS);
		digitalWrite(vent_relay, LOW);
		estado_vent == 0;
	}
}

//funcion detener todo 
void detener(){
	estado_calent = 0;
	digitalWrite(calent, estado_calent);
	estado_humi = 0;
	digitalWrite(humi, estado_humi); 
	ejecutarIncremento = false;
	ejecutarDecremento = true;
	estado_vent = 0;
}

//funcion para activar el modo automatico
void activarAuto(){
	estado_calent = 1;
	//activar los componentes
	digitalWrite(calent, estado_calent);
	envioFeedbackCalent(estado_calent);

	vTaskDelay(10000 / portTICK_PERIOD_MS);
	estado_vent = 1;
	ejecutarIncremento = true;
	ejecutarDecremento = false;
	envioFeedbackVent(estado_vent);

	estado_humi = 1;
	digitalWrite(humi, estado_humi);
	envioFeedbackHumi(estado_humi);
}

//funcion para el control automatico
void controlAuto(bool estado_temp, bool estado_hum){
	if (automatico == true){
		if (estado_temp == false && estado_hum == false){
			digitalWrite(calent, estado_calent);
			digitalWrite(humi, estado_humi);
			analogWrite(vent, 170);
			estado_vent = true;
			estado_calent = true;
			estado_humi = true;
		}
		if (estado_temp == false && estado_hum == true){
			digitalWrite(calent, estado_calent);
			digitalWrite(humi, estado_humi);
			analogWrite(vent, 170);
			estado_vent = true;
			estado_calent = true;
			estado_humi = false;
		}
		if (estado_temp == true && estado_hum == false){
			digitalWrite(calent, estado_calent);
			digitalWrite(humi, estado_humi);
			analogWrite(vent, 68);
			estado_vent = true;
			estado_calent = false;
			estado_humi = true;
		}
		if (estado_temp == true && estado_hum == true){
			digitalWrite(calent, estado_calent);
			digitalWrite(humi, estado_humi);
			ejecutarIncremento = false;
			ejecutarDecremento = true;
			decrementarVel();
			estado_vent = false;
			estado_calent = false;
			estado_humi = false;
		}
	}
}

