#include <Arduino.h>
#include "Temperature.h"
#include "I2C.h"
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include "bluetooth.h"
#include "esp_sleep.h"
#include "Impedance.h"



#define LED_PIN_1 4
#define LED_PIN2 15
#define SLEEP_TIME 5
#define nb_cartefille 0

int led_state=HIGH;
//#define FB_HZ            384.0       // fréquence de bit


// ---------- Planification long terme ----------
#define MEASURE_INTERVAL_MS  (60UL * 60UL * 1000UL)  // une mesure toutes les heures

// ---------- Buffers résultats ----------
float currentBuf[MAX_SAMPLES];
float voltageBuf[MAX_SAMPLES];
uint8_t lfsr=0x7F;

static const int BIN_LIST[] = {5, 6, 7, 8, 9};
static const int NUM_BINS = sizeof(BIN_LIST) / sizeof(BIN_LIST[0]);

uint8_t adresses_i2c_filles[nb_cartefille][2] ;//= {{0x24, 0x35}};


//variable globale
BLEAdvertising *pAdvertising;
uint8_t batAlimActuelle=0;
//Fonctions envoie bluetooth

// Définir un seuil de tension minimum pour considérer qu'une batterie est présente
// (Ajuste cette valeur selon ton type de batterie, ex: 1.0V)
const float SEUIL_PRESENCE_BATTERIE = 1.0; 



// f_target = 20 Hz, demi-largeur = 6 Hz -> bins k = 5..9 (résolution Fb/SEQ_LEN ≈ 3.02 Hz)

#define COH_MIN 0.9f

// Calcule X[k] (complexe, non normalisé) pour un segment de N échantillons
/*static void goertzel(const float *x, uint16_t offset, uint16_t N, int k,
                      float &re, float &im) {
  float w = 2.0f * PI * (float)k / (float)N;
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
}*/
/*ImpedanceResult computeImpedance() {
  ImpedanceResult result = {false, 0, 0, 0};
 
  // moyenne DC à retirer (montage unipolaire 0/Imax)
  float meanI = 0, meanV = 0;
  for (uint16_t n = 0; n < MAX_SAMPLES; n++) { meanI += currentBuf[n]; meanV += voltageBuf[n]; }
  meanI /= MAX_SAMPLES; meanV /= MAX_SAMPLES;
 
  float SuvRe[NUM_BINS] = {0}, SuvIm[NUM_BINS] = {0};
  float Suu[NUM_BINS] = {0}, Svv[NUM_BINS] = {0};
 
  static float iSeg[SEQ_LEN], vSeg[SEQ_LEN];
 
  for (uint16_t rep = 0; rep < REPEATS; rep++) {
    uint16_t offset = rep * SEQ_LEN;
    for (uint16_t n = 0; n < SEQ_LEN; n++) {
      iSeg[n] = currentBuf[offset + n] - meanI;
      vSeg[n] = voltageBuf[offset + n] - meanV;
    }
 
    for (int b = 0; b < NUM_BINS; b++) {
      float iRe, iIm, vRe, vIm;
      goertzel(iSeg, 0, SEQ_LEN, BIN_LIST[b], iRe, iIm);
      goertzel(vSeg, 0, SEQ_LEN, BIN_LIST[b], vRe, vIm);
 
      // Suv += conj(I) * V ; Suu += |I|^2 ; Svv += |V|^2
      SuvRe[b] += iRe * vRe + iIm * vIm;
      SuvIm[b] += iRe * vIm - iIm * vRe;
      Suu[b]   += iRe * iRe + iIm * iIm;
      Svv[b]   += vRe * vRe + vIm * vIm;
    }
  }
 
  // moyenne pondérée par la cohérence sur les bins retenus
  float wSumRe = 0, wSumIm = 0, wSum = 0, cohSum = 0;
  int nOk = 0;
 
  for (int b = 0; b < NUM_BINS; b++) {
    float suvRe = SuvRe[b] / REPEATS, suvIm = SuvIm[b] / REPEATS;
    float suu = Suu[b] / REPEATS, svv = Svv[b] / REPEATS;
 
    float suvMagSq = suvRe * suvRe + suvIm * suvIm;
    float coh = (suu > 1e-9f && svv > 1e-9f) ? (suvMagSq / (suu * svv)) : 0.0f;
 
    if (coh < COH_MIN) continue;
 
    // Z = Suv / Suu (division complexe, Suu réel)
    float zRe = suvRe / suu, zIm = suvIm / suu;
    float w = coh * coh;
 
    wSumRe += zRe * w;
    wSumIm += zIm * w;
    wSum   += w;
    cohSum += coh;
    nOk++;
  }

  if (nOk == 0 || wSum < 1e-9f) return result;  // valid = false -> mesure à rejeter
 
  float zRe = wSumRe / wSum, zIm = wSumIm / wSum;
 
  result.valid = true;
  result.magnitude_ohm = sqrtf(zRe * zRe + zIm * zIm);
  result.phase_deg = atan2f(zIm, zRe) * 180.0f / PI;
  result.confidence = cohSum / nOk;
 
  return result;
}*/
ImpedanceResult mesureImpedance() {
  mesure_impedance();
  /*for (int i = 0; i < MAX_SAMPLES; i++) {
    Serial.printf("Échantillon %d : Courant = %.3f A, Tension = %.3f V\n", i, currentBuf[i], voltageBuf[i]);
  }*/

  return computeImpedance();
}


DonneesCapteur mesurer_filles(int bat, int index_carte) {
        uint8_t adresse_actuelle = adresses_i2c_filles[index_carte][0];
        ssrOn_fille(bat, adresse_actuelle); // (Attention à adapter l'index du SSR selon ton routage)
        delay(800); // Laisser le temps au relais de coller et à la tension de se stabiliser
        float v_bus = inaLireTensionBus();
        ssrOn_fille(15, adresse_actuelle); // Allumer le SSR alim_temps de la carte fille
        DonneesCapteur data;
        Serial.printf("Carte 0x%02X | Emplacement %d : Tension mesurée %.2fV\n", adresse_actuelle, bat, v_bus);

            if (v_bus > SEUIL_PRESENCE_BATTERIE) {
                ssrOn(8);
                delay(800);
                // Une batterie est réelelment branché
                data.tensionBus_V = v_bus;
                data.courant_A = inaLireCourant();
                data.temperature_C = inaLireTemperature();
                //Température à faire
                data.temperaturebatterie_C = temperature_carte_fille(0x35, bat);
                
                Serial.printf("Carte 0x%02X | Emplacement %d : Batterie détectée (%.2fV)\n", adresse_actuelle, bat, v_bus);
                
            } else {
                // Pas de batterie
            }
            
            // 4. On éteint impérativement le SSR avant de passer à l'emplacement suivant
            ssrOff_fille(bat, adresse_actuelle);
            ssrOff_fille(15, adresse_actuelle); // Éteindre le SSR TEMPS de la carte fille
            ssrOff(8),
            delay(100); // Petit délai de sécurité entre deux commutations*
            return data;
    }

DonneesCapteur mesures(int bat) {
  // Éteindre tous les CONT_MES
  ssrOff(7); ssrOff(4); ssrOff(2); ssrOff(0);
  ssrOff(9); ssrOff(10); ssrOff(11); ssrOff(12); ssrOff(13); ssrOff(14);
  ssrOff(8);
  int r;
  delay(200);
  // Si cette batterie alimente, basculer sur une autre
  if (batAlimActuelle==bat){
    r= (esp_random()%3) +1;
    while (r==bat){
      r= (esp_random()%3) +1;
      if (r==3){
      r=4;
    }
    }
    
  }else {
    r=batAlimActuelle;
  }
  alimentation_on(r);
  delay(100);
  if (bat<4){
    alimentation_off(bat);
  }
  batAlimActuelle=r;
  DonneesCapteur m;
  if (bat > 10) {
    //Serial.printf("Mesure sur carte fille pour batterie %d\n", bat);
    int offset       = bat - 10 - 1;        // 0-indexé
    int index_carte  = offset / 15;     // quelle carte fille
    int bat_locale   = (offset % 15) + 1; // emplacement 1-15
 
    if (index_carte >= nb_cartefille) {
      Serial.printf("Carte fille %d inexistante (bat %d)\n", index_carte, bat);
      return {};
    }
 
    return mesurer_filles(bat_locale, index_carte);
  }



  // Allumer CONT_MES de la batterie voulue
  switch (bat) {
    case 1:  ssrOn(7);  break;
    case 2:  ssrOn(4);  break;
    case 3:  ssrOn(2);  break;
    case 4:  ssrOn(0);  break;
    case 5:  ssrOn(9);  break;
    case 6:  ssrOn(10); break;
    case 7:  ssrOn(11); break;
    case 8:  ssrOn(12); break;
    case 9:  ssrOn(13); break;
    case 10: ssrOn(14); break;
    default:
     Serial.println("Batterie inconnue"); return {};
  }

  delay(800);

  m.tensionBus_V = inaLireTensionBus();

  if (m.tensionBus_V > SEUIL_PRESENCE_BATTERIE) {
    ssrOn(8); // MES_TOT
    delay(500);
    m.tensionBus_charge_V = inaLireTensionBus();
    m.courant_A           = inaLireCourant();

    ssrOff(8);
  } else {
    m.tensionBus_charge_V = 0.0f;
    m.courant_A           = 0.0f;
    m.impedance_deg         = 0.0f;
    m.impedance_ohm         = 0.0f;
  }

  m.temperature_C         = inaLireTemperature();
  m.temperaturebatterie_C = 0.0f;

  return m;
}


void faittous(int bat){
    ssrOn(15); //alim capteur temp
    DonneesCapteur data = mesures(bat);
    if (bat<10){
        int temp_moy = 0;
        for (int i = 0; i < 10; i++) {
            temp_moy += temperature(bat);
        }
        data.temperaturebatterie_C = temp_moy / 10.0;

       
    }
    ImpedanceResult impedanceResult;
    //Mesur de la batterie 4 car le reste est sur alim
    //Mais système final devrait faire ca pour toutes les batteries
    if (bat==4 && data.tensionBus_V > SEUIL_PRESENCE_BATTERIE) {
      
      impedanceResult = mesureImpedance();
      delay(1000);
      if (impedanceResult.valid) {
        data.impedance_ohm = impedanceResult.magnitude;
        data.impedance_deg = impedanceResult.phase;
        }
      Serial.printf("batAlimActuelle au moment de la mesure : %d\n", batAlimActuelle);
      Serial.printf("Résultat de l'impédance : Magnitude = %.2f Ω, Phase = %.2f°, Validité = %s\n",
        impedanceResult.magnitude,
        impedanceResult.phase,
        impedanceResult.valid ? "Valide" : "Invalide");
        Serial.printf("Résultat : Validité = %s, confiance = %.3f\n", impedanceResult.valid ? "Valide" : "Invalide", impedanceResult.coherence);
        ssrOff(8);

    }
    
    ssrOff(15); //alim capteur temp off
    if (data.tensionBus_V > SEUIL_PRESENCE_BATTERIE) {
        envoierDonnees(data, pAdvertising, bat);
        uint64_t chipid = ESP.getEfuseMac();
    Serial.printf("Diffusion | ChipID: %04X%08X | Batterie: %d | %.2fV | %.2fA | %.2fW | %.2fmV | %.1f°C | %.1f°C\n",
        (uint16_t)(chipid >> 32),
        (uint32_t)chipid,
        bat,
        data.tensionBus_V,
        data.courant_A,
        data.temperature_C,
        data.temperaturebatterie_C
    );

    }
}

void setup() {
    delay(1000); // Attente que le système soit stable après le démarrage
    BLEDevice::init("Esp_batterie");
    delay(1000);
    uint16_t impedance;
    
    
    //config I2C
    Wire.begin(); //I2C init
    Serial.begin(115200);
    pinMode(LED_PIN2, OUTPUT);
    digitalWrite(LED_PIN2, HIGH);
    mcpInit(); // Initialisation du MCP23017
    mcpInit_fille(0x24);
    batAlimActuelle = 1;  // ou 2 ou 4
    alimentation_on(batAlimActuelle);
    pinMode(LED_PIN_1, OUTPUT);
    delay(200);
    
    

    //Configuration INA237
    inaWrite16(0x00, 0x0000); // Configuration du INA237 Peut modifier le gain bit 4 et mettre un temps avant de faire la mesure bit bit 6 pour 2ms d'attente
    inaWrite16(0x01, 0xF6BB); // Configuration de l'ADC pour dire quelle mesure on veut faire (continuous, moyenne sur 64 échantillons et toutes les mesure sur 540µs)
    inaWrite16(0x02, 562); // 0,15/2¹⁵*0,15*819,2*10⁶ =562 0,15=Ampères max attendu,0,15 resistance shunt, 2¹⁵= résolution du convertisseur
    
    delay(1000); // Attente que le INA237 soit prêt après la configuration
    //Config bluetooth
    
    pAdvertising = BLEDevice::getAdvertising();
    Serial.println("Bluetooth prêt, en attente de diffusion...");
    //ssrAllOff(); // On s'assure que toutes les alimentations sont éteintes avant de commencer les mesures
    //ssrSetAll(0xFF,0xFF);
    //alimentation_on(1); // On allume l'alimentation de la batterie 1 pour faire les mesures

    ImpedanceResult impedanceResult;
    for (int i=1;i<13;i++){//J'ai actuellement 13 batterie
        if (i!=3){
            faittous(i);
        }
    }
    digitalWrite(LED_PIN2, LOW);
    ssrOff(8);

    // ── Deep sleep
    Serial.print("Deep sleep");
    Serial.flush();  // vider le buffer série avant de dormir
    digitalWrite(LED_PIN2,LOW);
 
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_TIME * 1000000ULL);
    esp_deep_sleep_start();



}


void loop(){

}