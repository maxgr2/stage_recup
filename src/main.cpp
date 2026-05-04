#include <Arduino.h>
#include <driver/dac.h>
#include <algorithm>

#define DAC_CHAN   DAC_CHANNEL_1 
#define ADC_MEAS   35            
const int SAMPLE_INTERVAL_US = 100;
const int NUM_SAMPLES = 50; 
int lut[NUM_SAMPLES]={960 , 236 , 10 , 230 , 883 , 1761 , 2722 , 3632 , 4095 , 4071 , 3127 , 2236 , 1266 , 473 , 53 , 80 , 533 , 1346 , 2323 , 3191 , 4049 , 4095 , 3536 , 2646 , 1681 , 806 , 186 , 14 , 271 , 929 , 1837 , 2874 , 3767 , 4095 , 3929 , 3013 , 2117 , 1140 , 389 , 27 , 111 , 625 , 1535 , 2510 , 3388 , 4095 , 4095 , 3398 , 2518 , 1555};


void setup_cwg() {
    dac_output_enable(DAC_CHAN);
   dac_cw_config_t cw_config = {
    .en_ch = DAC_CHAN,
    .scale = DAC_CW_SCALE_1, // Amplitude divisée par 2 (~1.6V au lieu de 3.3V)
    .phase = DAC_CW_PHASE_180,
    .freq = 1000, 
    .offset = 4            // ON REMONTE LE SIGNAL
    };
    dac_cw_generator_config(&cw_config);
}
float calculerPhase(int* tableauRef, int* tableauMeas, int taille, int intervalleUs) {
    // 1. Calculer combien de points font un cycle (360°)
    // Fréquence = 1000Hz -> Période = 1000µs
    // Nb de points par cycle = 1000µs / intervalle entre chaque point
    float pointsParCycle = 1000.0 / intervalleUs; 

    long max_corr = -2147483647; // Valeur minimum possible
    int meilleur_shift = 0;
    
    // On cherche le décalage sur une plage de +/- une demi-période
    int plage_recherche = (int)(pointsParCycle / 2);

    for (int shift = -plage_recherche; shift <= plage_recherche; shift++) {
        long correlation = 0;
        
        // On compare les signaux sur la zone centrale pour éviter de sortir du tableau
        for (int i = plage_recherche; i < taille - plage_recherche; i++) {
            // On soustrait 2048 pour enlever la composante continue (centrer sur 0)
            long ref_centree = (long)tableauRef[i] - 2048;
            long meas_centree = (long)tableauMeas[i + shift] - 2048;
            
            correlation += ref_centree * meas_centree;
        }

        // Si cette corrélation est la meilleure, on garde ce décalage
        if (correlation > max_corr) {
            max_corr = correlation;
            meilleur_shift = shift;
        }
    }

    // 2. Conversion du décalage de points en degrés
    // Un décalage positif veut dire que le signal de mesure est EN AVANCE
    // Un décalage négatif veut dire qu'il est EN RETARD (cas classique du condensateur)
    float decalage_deg = (meilleur_shift * 360.0) / pointsParCycle;

    return decalage_deg;
}
/*
void faireMesure() {
    int meas[NUM_SAMPLES];
    unsigned long start_time = micros();
    dac_cw_generator_enable();
    

    for (int n = 0; n < NUM_SAMPLES; n++) {    
        meas[n] = analogRead(ADC_MEAS);
    }
    dac_cw_generator_disable();


    float phase = calculerPhase(lut, meas, NUM_SAMPLES, SAMPLE_INTERVAL_US);
    Serial.printf("Phase par rapport au tableau : %.2f deg\n", phase);


}*/
    /*float sum_avg = 0;
    for(int n=0; n < NUM_SAMPLES; n++) sum_avg += meas[n];
    float avg = sum_avg / NUM_SAMPLES;
    


    float correlation_sin = 0;
    float correlation_cos = 0;

    for (int n = 0; n < NUM_SAMPLES; n++) {
        float s_reel = (meas[n] - avg) / 2048.0;
        // On compare avec notre tableau (lut est un sinus, on crée le cosinus par déphasage)
        correlation_sin += s_reel * lut[n];
        // Pour le cosinus, on décale l'index du tableau de 90° (1/4 de période)
        // À 1kHz/25µs, une période = 40 points, donc 90° = 10 points
        int cos_idx = (n + 10) % 40; 
        correlation_cos += s_reel * lut[cos_idx];
        }
    float phase_rad = atan2(correlation_sin, correlation_cos);
    float phase_deg = phase_rad * 180.0 / PI;
    Serial.printf("Phase par rapport au tableau : %.2f deg\n", phase_deg);*/
void setup() {
    Serial.begin(115200);
    setup_cwg();
    analogReadResolution(12);
    //Pré calcul du tableau de sinus parfait pour 1kHz à 25µs d'intervalle (40 points par période)
}

float faireMesure() {
    int meas[NUM_SAMPLES];
    dac_cw_generator_enable();
    unsigned long start_t = micros();
    
    for (int n = 0; n < NUM_SAMPLES; n++) {
        meas[n] = analogRead(ADC_MEAS); // Lecture à vitesse max (~10-15µs)
    }
    
    unsigned long end_t = micros();
    dac_cw_generator_disable();

   return calculerPhase(lut, meas, NUM_SAMPLES, (end_t - start_t) / NUM_SAMPLES);
}

void loop() {
    float moyenne_phase = 0.0;
    for (int i=0; i<10; i++) {
    moyenne_phase=faireMesure() + moyenne_phase;
    delay(10);
    }
    Serial.printf("Phase moyenne sur 10 mesures : %.2f deg\n", moyenne_phase/10.0);
    delay(1000);
}