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
        case 1: Vout = lireVoltage(34); break;
        case 2: Vout = lireVoltage(35); break;
        case 3: Vout = lireVoltage(32); break; 
        case 4: Vout = lireVoltage(25); break; 
    }
       
    float Vref = lireVoltage(26);
    float denominateur = Vref - Vout;
    if (abs(denominateur) < 0.001) {
    return -1.0;  // Erreur : Vout ≈ VCC (court-circuit)
    }
    float Rx = R_FIXE * Vout / denominateur;

    /*Serial.print("Vout: ");    Serial.println(Vout, 4);
    Serial.print("Vref: ");    Serial.println(Vref, 4);
    Serial.print("Rx: ");      Serial.println(Rx, 1);*/
    float tempK = 1.0 / ( (1.0 / T25_K) + (1.0 / BETA) * log(Rx / R25) );
    return abs(tempK - 273.15);
    //float temperature = 1/(C1+C2*log(Rx)+C3*pow(log(Rx),3))-273.15; // 20000.0 est la résistance à 25°C, 3950.0 est le coefficient de température du thermistor
    //return temperature;
}