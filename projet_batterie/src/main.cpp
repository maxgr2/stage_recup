#include <Arduino.h>
#include "Temperature.h"
#include "I2C.h"
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include "bluetooth.h"
#include "esp_sleep.h"


#define LED_PIN_1 4
#define LED_PIN2 15
#define SLEEP_TIME 15
int led_state=HIGH;

//variable globale
BLEAdvertising *pAdvertising;

//Fonctions envoie bluetooth


void faittous(int bat){
    alimentation_on(bat);

    
    
    DonneesCapteur data=mesures(bat); // Mesure pour la batterie 1
    int temp_moy=0;
    for (int i=0; i<10;i++){
        temp_moy += temperature(bat);
    }
    float resistance = temp_moy / 10.0;
    resistance = temp_moy / 10.0;
    data = inaLire_1_Batterie(); // On lit les données du capteur INA237 pour une batterie
    data.temperaturebatterie_C = resistance; // On suppose que la résistance est proportionnelle
    Serial.printf("Diffusion | ChipID: %08X | Batterie: %d | %.2fV | %.2fA | %.2fW | %.2fmV | %.1f°C | %.1f°C\n",
        data.tensionBus_V,
        data.courant_A,
        data.puissance_W,
        data.tensionShunt_mV,
        data.temperature_C,
        data.temperaturebatterie_C
    );
    envoierDonnees(data, pAdvertising, bat); // On envoie les données via Bluetooth


    
}

void setup() {
    BLEDevice::init("Esp_batterie");
    delay(1000);
    //config I2C
    Wire.begin(); //I2C init
    Serial.begin(115200);
    pinMode(LED_PIN2, OUTPUT);
    digitalWrite(LED_PIN2, HIGH);
    
    

    //Configuration INA237
    inaWrite16(0x00, 0x0000); // Configuration du INA237 Peut modifier le gain bit 4 et mettre un temps avant de faire la mesure bit bit 6 pour 2ms d'attente
    inaWrite16(0x01, 0xF6BB); // Configuration de l'ADC pour dire quelle mesure on veut faire (continuous, moyenne sur 64 échantillons et toutes les mesure sur 540µs)
    inaWrite16(0x02, 562); // 0,15/2¹⁵*0,15*819,2*10⁶ =562 0,15=Ampères max attendu,0,15 resistance shunt, 2¹⁵= résolution du convertisseur
    
    delay(1000); // Attente que le INA237 soit prêt après la configuration
    //Config bluetooth
    
    pAdvertising = BLEDevice::getAdvertising();
    Serial.println("Bluetooth prêt, en attente de diffusion...");
    ssrAllOff(); // On s'assure que toutes les alimentations sont éteintes avant de commencer les mesures

    alimentation_on(1); // On allume l'alimentation de la batterie 1 pour faire les mesures

    /*
    for (int i=1;i<5;i++){
        faittous(i);
    }
    digitalWrite(LED_PIN2, LOW);

    // ── Deep sleep
    Serial.print("Deep sleep");
    Serial.flush();  // vider le buffer série avant de dormir
    digitalWrite(LED_PIN2,LOW);
 
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_TIME * 1000000ULL);
    esp_deep_sleep_start();
    */

}

void loop(){}
/*
int i=1;
void loop() {
    faittous(i);
    digitalWrite(LED_PIN2, led_state);
    if (led_state==HIGH){
        led_state=LOW;
    }
    else {
        led_state=HIGH;
    }
    i++;
    if (i==5){
        i=1;
    }

    delay(1000);
}*/
