#pragma once
// ============ Impedance.h ============
#include <Arduino.h>


#define ECH_HZ          10000.0f
#define FB_HZ           384.0f
#define COHERENCE_MIN   0.70f
#define Z_MIN_VALID     0.05f
#define Z_MAX_VALID     10.0f
#define PHASE_MAX_VALID 45.0f
#define TB_US            ((uint32_t)(1000000.0 / 384.0))   // ~2604 µs
#define SEQ_LEN          127         // PRBS7 : 2^7 - 1
#define REPEATS          16          // répétitions de la séquence par mesure (ajuste pour le SNR)
#define SETTLE_US        300         // attente après transition, > 0.2 ms de Ton/Toff du relais
#define MAX_SAMPLES       (SEQ_LEN * REPEATS)


typedef struct {
    float magnitude;   // |Z| en ohms
    float phase;       // en degrés
    float coherence;   // 0-1
    bool valid;
} ImpedanceResult;

// Fonction principale unique
void mesure_impedance();
static inline uint8_t prbs7_next_bit();
ImpedanceResult computeImpedance();

// Calcule l'impédance par corrélation PRBS directe
// Retourne le résultat avec validité, magnitude, phase, cohérence, SNR


