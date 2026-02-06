// Definimos el pin GPIO 12
const int led1 = 25;
const int led2 = 26;
const int led3 = 27;
const int led4 = 14;
const int led5 = 12;

void setup() {
  // Configuramos el pin 12 como salida
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(led4, OUTPUT);
  pinMode(led5, OUTPUT);
}

void loop() {
	// Enviamos una señal ALTA (3.3V) para encender el LED
	digitalWrite(led1, HIGH);
	digitalWrite(led2, HIGH);
	digitalWrite(led3, HIGH);
	digitalWrite(led4, HIGH);
	digitalWrite(led5, HIGH);
	delay(1000);
	digitalWrite(led1, LOW);
	digitalWrite(led2, LOW);
	digitalWrite(led3, LOW);
	digitalWrite(led4, LOW);
	digitalWrite(led5, LOW);
	delay(1000);
  
}
