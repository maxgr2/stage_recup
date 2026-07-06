#include <Arduino.h>
#include "Temperature.h"
#include <Wire.h>

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
    if (cap < 1 || cap > 10) {return -1.0f;}

    float Vref = lireVoltage(PIN_VREF);
    //Serial.printf("Vref: %f", Vref);
    float Vout = lireVoltage(TEMP_PINS[cap-1]);
    //Serial.printf("Vmes: %f\n", Vout);
    if ((2*Vref - Vout) <= 0.0f) return -1.0f;

    // Résistance du TMP6131
    float Rx = R_FIXE * Vout / (2*Vref - Vout);

    // Formule linéaire PTC : R(T) = R25 * (1 + TCR * (T - T25))
    // → T = ((Rx / R25) - 1) / TCR + T25
    float tempC = ((Rx / R25) - 1.0f) / TCR + T25;

    if (tempC < -40.0f || tempC > 125.0f) {
        //Serial.printf("Température hors plage cap %d: %.1f°C\n", cap, tempC);
        return -1.0f;
    }

    return tempC;
}
static uint8_t build_control_byte_diff(uint8_t paire, bool inverser_polarite) {
  // paire : 0 (pour CH0/CH1), 1 (pour CH2/CH3), etc. jusqu'à 7.
  if (paire > 7) return 0;
  
  uint8_t odd = inverser_polarite ? 1 : 0;
  uint8_t a2a1a0 = paire & 0x07;
  
  // 0b10000000 = Enable (10) + Différentiel (SGL=0)
  return 0b10000000 | (odd << 4) | (a2a1a0 << 1); 
}

// 2. La lecture différentielle
float LireCanalDifferentiel(uint8_t adresse_i2c, uint8_t paire, bool inverser_polarite = false) {
  uint8_t ctrl = build_control_byte_diff(paire, inverser_polarite);

  // Étape A : Envoyer la config (ceci annule l'ancienne conversion et lance la nouvelle)
  Wire.beginTransmission(adresse_i2c);
  Wire.write(ctrl);
  Wire.endTransmission();

  // Étape B : Attendre la fin de la conversion (165ms max)
  delay(200); 

  // Étape C : Lire les 3 octets
  Wire.requestFrom((uint16_t)adresse_i2c, (uint8_t)3);
  if (Wire.available() < 3) {
    Serial.println("Erreur I2C: pas assez de donnees");
    return -1.0f;
  }

  uint32_t raw = 0;
  raw  = ((uint32_t)Wire.read()) << 16;
  raw |= ((uint32_t)Wire.read()) << 8;
  raw |=  (uint32_t)Wire.read();

  // Étape D : Décodage mathématique propre
  int32_t signed_val = (int32_t)raw - 0x800000;
  const float VREF_LTC = 3.3f;
  const float FULL_SCALE_CODE = 4194304.0f; // 2^22
  
  // Calcul de la tension
  float tension = ((float)signed_val / FULL_SCALE_CODE) * (VREF_LTC / 2.0f);

  Serial.printf("Adresse I2C: 0x%02X, Paire diff: %d, Code brut: 0x%06X, Tension: %.6f V\n",
    adresse_i2c, paire, raw, tension);

  return tension;
}
//Pour plus tard il faudra modifier le pcb, ou la fonction pour que ca marche
float temperature_carte_fille(uint8_t adresse_i2c, uint8_t bat) {
  // 1. Lire la tension différentielle (Paire 0 = CH0 et CH1)
  // On suppose que CH0 (IN+) est ta référence à VREF/2 et CH1 (IN-) est ton capteur
  float diff_tension = LireCanalDifferentiel(adresse_i2c, 0, false); 
  
  // Si la fonction a renvoyé -1 (erreur I2C ou pas de réponse)
  if (diff_tension <= -2.0f) {
      return -1.0f; 
  }
  
  // 2. Retrouver la tension absolue sur la broche du capteur (CH1)
  const float VREF_LTC = 3.3f;
  float tension_reference = VREF_LTC / 2.0f;      // La tension théorique sur CH0 (1.65V)
  float V_CH1 = tension_reference - diff_tension; // On soustrait l'écart pour trouver CH1
  
  // Sécurité : vérifier que la tension est physiquement possible (entre 0V et 3.3V)
  if (V_CH1 <= 0.0f || V_CH1 >= VREF_LTC) {
      Serial.printf("Erreur: Tension CH1 hors limites (%.2fV)\n", V_CH1);
      return -1.0f;
  }
  
  // 3. Calcul de la résistance de la thermistance (Rx)
  // Issu de la formule classique : V_CH1 = VREF_LTC * (Rx / (R_FIXE + Rx))
  // Attention : on suppose que la PTC est reliée au GND et R_FIXE au VCC. 
  // (Si c'est l'inverse sur ton PCB, la formule est : Rx = R_FIXE * (VREF_LTC - V_CH1) / V_CH1 )
  float Rx = R_FIXE * V_CH1 / (VREF_LTC - V_CH1);
  
  // 4. Calcul de la température (Formule linéaire de ta PTC TMP6131)
  float tempC = ((Rx / R25) - 1.0f) / TCR + T25;
  
  // 5. Filtre anti-valeurs absurdes
  if (tempC < -40.0f || tempC > 125.0f) {
    Serial.printf("Température hors plage carte fille : %.1f°C\n", tempC);
    return -1.0f;
  }
  
  return tempC;
}