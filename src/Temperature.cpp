#include <Arduino.h>
const float R = 10000.0; 
const float VREF_PIN_SUPPLY = 3.3;  // Tension d'alimentation (5V ou 3.3V)
const int   ADC_RESOLUTION  = 4095; // 10 bits pour Arduino Uno

float lireVoltage(int pin) {
  int raw = analogRead(pin);
  return (raw / (float)ADC_RESOLUTION) * VREF_PIN_SUPPLY;
}



float temperature(int cap) {
    float Vout;
    if (cap < 0 || cap > 4) {
        return -1.0; // On a que 4 capteur de température
    }
    if (cap == 1) {
        Vout = lireVoltage(26); 
    } 
    if (cap == 2) {
        Vout = lireVoltage(27);
    }
    if (cap == 3) {
        Vout = lireVoltage(14); // Capteur de référence
    }
    if (cap == 4) {
        Vout = lireVoltage(12); // Capteur de mesure
    }
       
    float Vref = lireVoltage(33);

    float Vcc_calc = Vref * (R + R) / R;
    float denominateur = Vcc_calc - Vout;
    if (abs(denominateur) < 0.001) {
    return -1.0;  // Erreur : Vout ≈ VCC (court-circuit)
    }

    float Rx = R * Vout / denominateur;
    float temperature = 1/(1/298.15 + (1.0/3950.0) * log(Rx/10000.0))-272.15; 
    return temperature;
}