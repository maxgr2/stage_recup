// ============ Impedance.cpp ============

#include "Impedance.h"
#include <Arduino.h>
#include "I2C.h"
#include <math.h>


// Externes de votre main
extern float currentBuf[MAX_SAMPLES];
extern float voltageBuf[MAX_SAMPLES];
extern uint8_t lfsr;   
extern uint8_t batAlimActuelle;

static inline uint8_t prbs7_next_bit() {
  uint8_t newbit = ((lfsr >> 6) ^ (lfsr >> 5)) & 0x01;
  lfsr = ((lfsr << 1) | newbit) & 0x7F;
  return newbit; // 0 ou 1
}

void mesure_impedance(){
  inaWrite16(0x01, 0xF000);

  uint16_t idx = 0;

  for (uint16_t rep = 0; rep < REPEATS; rep++) {
    for (uint16_t b = 0; b < SEQ_LEN; b++) {
      uint8_t bit = prbs7_next_bit();
      if (bit) {
        ssrOn(8);  // Résultat = HIGH;
      } else {
        ssrOff(8);  // Résultat = LOW;
      }

      uint32_t tBit = micros();

      // attendre la fin du transitoire de commutation avant d'échantillonner
      while ((uint32_t)(micros() - tBit) < SETTLE_US) { /* attente */ }

      float i = inaLireCourant();
      float v = inaLireTensionBus();
      currentBuf[idx] = i;
      voltageBuf[idx] = v;
      idx++;

      // compléter le reste de la durée du bit
      while ((uint32_t)(micros() - tBit) < TB_US) { /* attente */ }
    }
  }

  ssrOff(8);  // s'assurer que le relais est éteint à la fin de la mesure
  inaWrite16(0x01, 0xF6BB); // Configuration de l'ADC pour dire quelle mesure on veut faire (continuous, moyenne sur 64 échantillons et toutes les mesure sur 540µs)

  return;



}
// ============================================================
// CONSTANTES ET CONFIGURATION FREQUENTIELLE
// ============================================================
static const int BIN_LIST[] = {5, 6, 7, 8, 9}; // Bins autour de f_target
static const int NUM_BINS = sizeof(BIN_LIST) / sizeof(BIN_LIST[0]);

// ============================================================
// GOERTZEL STANDARD
// ============================================================
static void goertzelStandard(const float *x, uint16_t offset, uint16_t N, int k,
                             float &re, float &im) {
    float w = TWO_PI * (float)k / (float)N;
    float cosine = cosf(w);
    float sine = sinf(w);
    float coeff = 2.0f * cosine;
    float q0 = 0, q1 = 0, q2 = 0;

    for (uint16_t n = 0; n < N; n++) {
        q0 = coeff * q1 - q2 + x[offset + n];
        q2 = q1;
        q1 = q0;
    }
    re = q1 - q2 * cosine;
    im = q2 * sine;
}

// ============================================================
// CALCUL DE LA COHÉRENCE (MSC - Magnitude Squared Coherence)
// ============================================================
static float computeCoherence(float SuvRe, float SuvIm, float Suu, float Svv) {
    float Suv2 = SuvRe * SuvRe + SuvIm * SuvIm;
    float denom = Suu * Svv;
    return (denom > 1e-18f) ? (Suv2 / denom) : 0.0f;
}

// ============================================================
// NOUVELLE IMPLÉMENTATION DE COMPUTEIMPEDANCE VIA GOERTZEL
// ============================================================
ImpedanceResult computeImpedance() {
    // 1. Soustraction de la composante continue (DC)
    float iMean = 0, vMean = 0;
    for (int i = 0; i < MAX_SAMPLES; i++) {
        iMean += currentBuf[i];
        vMean += voltageBuf[i];
    }
    iMean /= MAX_SAMPLES;
    vMean /= MAX_SAMPLES;

    // Buffers intermédiaires par répétition
    static float iSeg[SEQ_LEN];
    static float vSeg[SEQ_LEN];

    float SuvRe[NUM_BINS] = {0};
    float SuvIm[NUM_BINS] = {0};
    float Suu[NUM_BINS]   = {0};
    float Svv[NUM_BINS]   = {0};

    // 2. Traitement par segment (REPEATS)
    for (uint16_t rep = 0; rep < REPEATS; rep++) {
        uint16_t offset = rep * SEQ_LEN;

        // Extraction du segment avec retrait de la moyenne DC
        for (uint16_t n = 0; n < SEQ_LEN; n++) {
            iSeg[n] = currentBuf[offset + n] - iMean;
            vSeg[n] = voltageBuf[offset + n] - vMean;
        }

        // Application de Goertzel pour chaque bin cible
        for (int b = 0; b < NUM_BINS; b++) {
            float iRe, iIm, vRe, vIm;

            goertzelStandard(iSeg, 0, SEQ_LEN, BIN_LIST[b], iRe, iIm);
            goertzelStandard(vSeg, 0, SEQ_LEN, BIN_LIST[b], vRe, vIm);

            // Accumulation du spectre croisé (S_iv = conj(I) * V) et des auto-spectres
            SuvRe[b] += (iRe * vRe + iIm * vIm);
            SuvIm[b] += (iRe * vIm - iIm * vRe);
            Suu[b]   += (iRe * iRe + iIm * iIm);
            Svv[b]   += (vRe * vRe + vIm * vIm);
        }
    }

    // 3. Fusion des résultats sur l'ensemble des Bins
    float totalSuvRe = 0, totalSuvIm = 0;
    float totalSuu = 0, totalSvv = 0;
    float cohSum = 0;
    int validBinsCount = 0;

    for (int b = 0; b < NUM_BINS; b++) {
        float binCoh = computeCoherence(SuvRe[b], SuvIm[b], Suu[b], Svv[b]);

        // Accumulation globale
        totalSuvRe += SuvRe[b];
        totalSuvIm += SuvIm[b];
        totalSuu   += Suu[b];
        totalSvv   += Svv[b];

        cohSum += binCoh;
        if (binCoh > COHERENCE_MIN) {
            validBinsCount++;
        }
    }

    // Cohérence moyenne calculée sur tous les bins cibles
    float coherence = computeCoherence(totalSuvRe, totalSuvIm, totalSuu, totalSvv);

    // 4. Calcul de l'impédance complexe Z = S_iv / S_ii
    float zMag = 0.0f;
    float zPhase = 0.0f;

    if (totalSuu > 1e-12f) {
        float zRe = totalSuvRe / totalSuu;
        float zIm = totalSuvIm / totalSuu;

        zMag = sqrtf(zRe * zRe + zIm * zIm);
        zPhase = atan2f(zIm, zRe) * (180.0f / M_PI); // Conversion rad -> degrés
    }

    // 5. Critères de validation du résultat
    bool valid = (coherence >= COHERENCE_MIN) &&
                 (zMag >= Z_MIN_VALID && zMag <= Z_MAX_VALID) &&
                 (fabsf(zPhase) <= PHASE_MAX_VALID);

    Serial.printf("[IMP-GOERTZEL] |Z|=%.4f Ω ∠%.2f° | Cohérence: %.3f (%d/%d Bins OK) -> %s\n",
                  zMag, zPhase, coherence, validBinsCount, NUM_BINS,
                  valid ? "OK" : "FAIL");

    return (ImpedanceResult){zMag, zPhase, coherence, valid};
}