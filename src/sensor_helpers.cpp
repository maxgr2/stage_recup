/**
 * @file sensor_helpers.cpp
 * @author Nicolas Boulanger
 * @author Maxime Goncalves
 * @author Dominique Richer
 * @author Emilie Croteau
 * @date 2025-07-28
 * @brief Contient les fonctions pour interagir le module INA228 afin
 *        d'obtenir les valeurs de tensions et d'impédance.
 * @defgroup INA228_HELPERS INA228 helper functions
 * @copyright
 */
#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "sensor_helpers.h"
#include "i2c_helpers.h"

// --- Buffer glissant sur 5 mesures ---
float buf[PRINT_PERIOD_S];
uint8_t idx=0, count=0;

void readAveraged(float &vbus_V, float &vshunt_V){
    double sV=0, sS=0;
    uint8_t nV=0, nS=0;

    for(uint8_t i=0; i<AVG_SAMPLES; i++){
        int32_t vbus_raw;
        int32_t vsh_raw;
        if (spiRead24s(REG_VBUS, vbus_raw)) { 
            sV += (double)vbus_raw * VBUS_LSB_V; 
            nV++;
        }
        if (spiRead24s(REG_VSHUNT, vsh_raw)) { 
            sS += (double)vsh_raw * VSHUNT_LSB_V; 
            nS++;
        }
        delay(SAMPLE_DELAY_MS);
    }
    vbus_V   = nV ? (float)(sV/nV) : NAN;
    vshunt_V = nS ? (float)(sS/nS) : NAN;
}

// --- Une estimation R_int (OFF -> ON) ---
// Permet de mesurer l'impédance interne de la batterie, en mesurant la chute de tension
// lors de l'application d'une charge connue (MOSFET + résistance de shunt).
bool measure_Rint_once(float &Rint_ohm, float &Vopen, float &Vload){
    // OFF
    digitalWrite(MOSFET_GATE_PIN, LOW);
    delay(LOAD_SETTLE_MS);
    float vbus_off, vsh_off;
    readAveraged(vbus_off, vsh_off);

    // ON
    digitalWrite(MOSFET_GATE_PIN, HIGH);
    delay(LOAD_SETTLE_MS);
    float vbus_on, vsh_on;
    readAveraged(vbus_on, vsh_on);

    // Retour OFF (évite échauffement continu)
    digitalWrite(MOSFET_GATE_PIN, LOW);

    float Iopen = vsh_off / R_SHUNT_OHMS;
    float Iload = vsh_on  / R_SHUNT_OHMS;
    Vopen = vbus_off;
    Vload = vbus_on;

    float dV = Vopen - Vload;
    float dI = Iload - Iopen;

    if (fabs(dI) < 1e-6f || isnan(dV) || isnan(dI)) return false;

    // R_int toujours POSITIVE
    Rint_ohm = fabs(dV / dI);
    return true;
}
