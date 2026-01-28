// Definimos el pin GPIO 12
const int ledPin = 12;

void setup() {
  // Configuramos el pin 12 como salida
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // Enviamos una señal ALTA (3.3V) para encender el LED
  digitalWrite(ledPin, HIGH);
  delay(200);
   digitalWrite(ledPin, LOW);
   delay(200);
  
}
