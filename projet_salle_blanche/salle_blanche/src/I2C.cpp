#include <Wire.h>
#include <Arduino.h>


#define SPS30_ADDR 0x69

void lecturePM(){
    Wire.beginTransmission(SPS30_ADDR);
    Wire.write(0x00); //faire une mesure
    delay(50); // Attendre que le capteur fasse la mesure
    Wire.write(0x03); // Commande de lecture des données
    Wire.requestFrom(SPS30_ADDR, (uint8_t)24);
    if (Wire.available() == 24) {
        uint16_t pm1_0 = Wire.read() << 8 | Wire.read();
        uint16_t pm2_5 = Wire.read() << 8 | Wire.read();
        uint16_t pm10 = Wire.read() << 8 | Wire.read();
        // Ignorer les autres données pour l'instant
        Serial.printf("PM1.0: %d µg/m³, PM2.5: %d µg/m³, PM10: %d µg/m³\n", pm1_0, pm2_5, pm10);
    } else {
        Serial.println("Erreur de lecture du SPS30");
    }

    Wire.endTransmission();
}