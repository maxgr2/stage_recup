#include <Arduino.h>
#include "Temperature.h"

#define VREF_ADC     3.3f
#define ADC_RES      4095
#define R_FIXE       10000.0f  // résistance fixe pont Wheatstone
#define R25          10000.0f  // résistance TMP6131 à 25°C
#define TCR          0.0064f   // 6400 ppm/°C = 0.64 %/°C
#define T25          25.0f

// GPIO39 = Vref (base pont Wheatstone)
#define PIN_VREF 39

static const int TEMP_PINS[10] = {34, 35, 32, 33, 25, 26, 27, 14, 12, 13};

static float lireVoltage(int pin) {
    return (analogRead(pin) / (float)ADC_RES) * VREF_ADC;
}

float temperature(int cap) {
    // cap : 1–10
    if (cap < 1 || cap > 10) return -1.0f;

    float Vref = lireVoltage(PIN_VREF);
    float Vout = lireVoltage(TEMP_PINS[cap - 1]);

    if ((Vref - Vout) <= 0.0f) return -1.0f;

    // Résistance du TMP6131
    float Rx = R_FIXE * Vout / (Vref - Vout);

    // Formule linéaire PTC : R(T) = R25 * (1 + TCR * (T - T25))
    // → T = ((Rx / R25) - 1) / TCR + T25
    float tempC = ((Rx / R25) - 1.0f) / TCR + T25;

    if (tempC < -40.0f || tempC > 125.0f) {
        Serial.printf("Température hors plage cap %d: %.1f°C\n", cap, tempC);
        return -1.0f;
    }

    return tempC;
}