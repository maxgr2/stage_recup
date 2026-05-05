#pragma once
#include <Arduino.h>
#include "Temperature.h"
#include "I2C.h"
#include <Wire.h>

void setup() {
    
    
    Wire.begin(); //I2C init
    Serial.begin(115200);
    inaWrite16(0x00, 0x0000); // Configuration du INA237 Peut modifier le gain bit 4 et mettre un temps avant de faire la mesure bit bit 6 pour 2ms d'attente
    inaWrite16(0x01, 0xF6BB); // Configuration de l'ADC pour dire quelle mesure on veut faire (continuous, moyenne sur 64 échantillons et toutes les mesure sur 540µs)
    inaWrite16(0x02, 562); // 0,15/2¹⁵*0,15*819,2*10⁶ =562 0,15=Ampères max attendu,0,15 resistance shunt, 2¹⁵= résolution du convertisseur
}

void loop() {
    int temp_moy=0;
    for (int i=0; i<10;i++){
        temp_moy += temperature(1);
        
    }
    float resistance = temp_moy / 10.0;
    Serial.print("Rx = ");
    Serial.print(resistance);
    Serial.println(" Ω");
    delay(1000);

}
