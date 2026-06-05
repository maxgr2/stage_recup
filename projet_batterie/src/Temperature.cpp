#include <Arduino.h>
const float R = 20000.0; 
const float VREF_PIN_SUPPLY = 3.3;  // Tension d'alimentation (5V ou 3.3V)
const int   ADC_RESOLUTION  = 4095; // 10 bits pour Arduino Uno

// Coefficients de Steinhart-Hart (Valeurs typiques à ajuster selon votre capteur)
const float C1 = 0.0011268740732306604;
const float C2 = 0.00023452183442732656;
const float C3 = 8.590172470421073e-08;

float lireVoltage(int pin) {
  int raw = analogRead(pin);
  return (raw / (float)ADC_RESOLUTION) * VREF_PIN_SUPPLY;
}
const float R_FIXE = 20000.0;   // résistance fixe du pont (10K)
const float R25    = 20000.0;   // résistance thermistor à 25°C
const float BETA   = 4500;    // sur votre datasheet ou l'emballage
const float T25_K  = 298.15;    // 25°C en Kelvi


float temperature(int cap) {
    float Vout;
    switch (cap) {
        case 1: Vout = lireVoltage(12); break;
        case 2: Vout = lireVoltage(35); break;
        case 3: Vout = lireVoltage(32); break;
        case 4: Vout = lireVoltage(25); break;
        default: return -1.0;
    }

    float Vref = lireVoltage(26);

    // Thermistance en BAS du diviseur (ajuste si inversé)
    float Rx = R_FIXE * Vout / (Vref - Vout);

    float tempK = 1.0f / ((1.0f / T25_K) + (1.0f / BETA) * log(Rx / R25));
    float tempC = tempK - 273.15f;

    // Sanity check résultat
    if (tempC < -40.0f || tempC > 125.0f) {
        Serial.print("Température hors plage: ");
        Serial.println(tempC);
        return -1.0f;
    }

    return tempC;
}