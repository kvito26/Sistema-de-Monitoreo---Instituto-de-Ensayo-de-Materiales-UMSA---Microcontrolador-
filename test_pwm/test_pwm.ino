const int ledPin = 22;       // Pin del LED
const int frequency = 5000; // 5KHz
const int resolution = 8;    // 8 bits (0-255)

void setup() {
  // En la versión nueva, solo necesitas UNA línea:
  // ledcAttach(pin, frecuencia, resolución);
  ledcAttach(ledPin, frequency, resolution);
  Serial.begin(115200);
}

void loop() {
  // Para escribir el valor, ahora se usa ledcWrite directamente al PIN
  // (antes se usaba al canal)
	Serial.println("INCREMENTO");
  for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle++) {
	Serial.println(dutyCycle);
    ledcWrite(ledPin, dutyCycle);
    delay(3);
  }
	Serial.println("DECREMENTO");
  for (int dutyCycle = 255; dutyCycle >= 0; dutyCycle--){
	Serial.println(dutyCycle);
    ledcWrite(ledPin, dutyCycle);
    delay(3);

	}
}
