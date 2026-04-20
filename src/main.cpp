#include <Arduino.h>
#include <driver/dac.h>

#define DAC_CHAN DAC_CHANNEL_1 // GPIO 25 sur WROOM / GPIO 17 sur S2
#define SINE_STEPS 25           // On réduit à 25 points pour laisser du souffle (40µs par point)
#define ADC_PIN_Impedance 32             // Pin ADC pour la mesure

// Table de 25 points pour générer le sinus
const uint8_t valuesinus[SINE_STEPS] = {
    128, 160, 190, 216, 236, 248, 254, 252, 243, 227, 204, 175, 144, 
    112, 81, 52, 29, 13, 4, 2, 8, 20, 40, 66, 96
};

volatile int i = 0;
hw_timer_t *timer = NULL;

volatile int v_max_raw = 0;
volatile int v_min_raw = 4095;
volatile int final_max = 0;
volatile int final_min = 0;
volatile bool data_ready = false;
volatile int idx_max_adc = 0;

void IRAM_ATTR onTimer() {
    // 1. Sortie DAC (toujours à chaque fois pour garder un beau sinus)
    dac_output_voltage(DAC_CHAN, valuesinus[i]);

    
    if (i % 2 == 0) {
        int val = analogRead(ADC_PIN_Impedance); 
        
        if (val > v_max_raw) {
            v_max_raw = val;
            idx_max_adc = i;
        }
        if (val < v_min_raw) v_min_raw = val;
    }

    i++;
    if (i >= SINE_STEPS) {
        i = 0;
        final_max = v_max_raw;
        final_min = v_min_raw;
        v_max_raw = 0;
        v_min_raw = 4095;
        data_ready = true;
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Test DAC lancé...");

    dac_output_enable(DAC_CHAN);

    // Pour 1kHz avec 25 points : 1ms / 25 = 40µs par interruption
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 40, true); 
    timerAlarmEnable(timer);
}

void loop() {
    if (data_ready) {
        data_ready = false;
        
        // Calcul de la tension Crête-à-Crête
        float v_pp = ((final_max - final_min) * 3.3) / 4095.0;

        Serial.print("Vpp mesure: ");
        Serial.print(v_pp);
        Serial.println(" V");
    }
    delay(100); // On affiche 10 fois par seconde
}