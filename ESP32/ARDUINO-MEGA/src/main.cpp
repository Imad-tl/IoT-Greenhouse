#include <Arduino.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  dht.begin();
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("ERREUR");
  } else {
    // Affichage console USB
    Serial.print("T=");
    Serial.print(t, 1);
    Serial.print(";H=");
    Serial.println(h, 0);

    // Envoi vers C6 via Serial1 (TX1 pin 18)
    Serial1.print("T=");
    Serial1.print(t, 1);
    Serial1.print(";H=");
    Serial1.println(h, 0);
  }

  delay(5000);
}


