#include <Arduino.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define LDR_PIN A0
#define SEUIL 300

void setup() {
  Serial.begin(9600);      // debug USB
  Serial1.begin(9600);     // lien vers ESP32-C6 (9600 plus fiable en 5V->3.3V)
  dht.begin();
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int lum = analogRead(LDR_PIN);
  const char* etat = (lum < SEUIL) ? "SOMBRE" : "LUMINEUX";

  if (isnan(t) || isnan(h)) {
    Serial1.println("ERR=DHT");
    Serial.println("ERR=DHT");
  } else {
    Serial1.print("T=");   Serial1.print(t, 1);
    Serial1.print(";H=");  Serial1.print(h, 0);
    Serial1.print(";LUM=");Serial1.print(lum);
    Serial1.print(";ETAT=");Serial1.println(etat);

    Serial.print("T=");   Serial.print(t, 1);
    Serial.print(";H=");  Serial.print(h, 0);
    Serial.print(";LUM=");Serial.print(lum);
    Serial.print(";ETAT=");Serial.println(etat);
  }

  delay(1000);
}


