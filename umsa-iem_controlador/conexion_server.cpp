#include "conexion_server.h"
#include <WebSocketsClient.h>

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length){
	switch(type){
		case WStype_DISCONNECTED:
			conexion_server = false;
			vTaskResume(PilotoServerHandle);
			Serial.println("[WS] Desconectado del Servidor IEM");
			break;

		case WStype_CONNECTED:
			Serial.println("[WS] Conectado con el Servidor IEM");
			vTaskSuspend(PilotoServerHandle);
			digitalWrite(piloto_server, HIGH);
			conexion_server = true;
			envioAutenticacion();
			envioDatosTH();
			break;

		case WStype_TEXT:
			Serial.printf("[WS] Mensaje recibido: %s\n", payload);
			break;

	}
}

