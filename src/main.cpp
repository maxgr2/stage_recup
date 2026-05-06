#include <Arduino.h>
#include "Temperature.h"
#include "I2C.h"
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include "bluetooth.h"


//variable globale
BLEAdvertising *pAdvertising;

//Fonctions envoie bluetooth


void setup() {
    BLEDevice::init("");
    delay(1000);
    //config I2C
    Wire.begin(); //I2C init
    Serial.begin(115200);
    
    

    //Configuration INA237
    inaWrite16(0x00, 0x0000); // Configuration du INA237 Peut modifier le gain bit 4 et mettre un temps avant de faire la mesure bit bit 6 pour 2ms d'attente
    inaWrite16(0x01, 0xF6BB); // Configuration de l'ADC pour dire quelle mesure on veut faire (continuous, moyenne sur 64 échantillons et toutes les mesure sur 540µs)
    inaWrite16(0x02, 562); // 0,15/2¹⁵*0,15*819,2*10⁶ =562 0,15=Ampères max attendu,0,15 resistance shunt, 2¹⁵= résolution du convertisseur
    
    delay(1000); // Attente que le INA237 soit prêt après la configuration
    //Config bluetooth
    
    pAdvertising = BLEDevice::getAdvertising();
    Serial.println("Bluetooth prêt, en attente de diffusion...");
}

void loop() {
    int temp_moy=0;
    for (int i=0; i<10;i++){
        temp_moy += temperature(1);
        
    }
    float resistance = temp_moy / 10.0;

    DonneesCapteur data;

    //data = inaLire_1_Batterie(); // On lit les données du capteur INA237 pour une batterie 
    data.tensionBus_V = 12.12;
    data.courant_A = 124.5;
    data.puissance_W = 12.0;
    data.tensionShunt_mV =123.12; 
    data.temperature_C = 124.12;
    data.temperaturebatterie_C = resistance; // On suppose que la résistance est proportionnelle

    envoierDonnees(data, pAdvertising); // On envoie les données via Bluetooth
    Serial.print("Rx = ");
    Serial.print(resistance);
    Serial.println(" Ω");
    delay(2000);

}
