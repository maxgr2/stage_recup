#include <Arduino.h>
#include "Temperature.h"
#include "I2C.h"
#include <Wire.h>
#include <Wifi_perso.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

//variable globale

WiFiUDP udp;




//Fonctions envoie bluetooth


void setup() {
    
    
    //config I2C
    Wire.begin(); //I2C init
    Serial.begin(115200);
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    

    //Configuration INA237
    inaWrite16(0x00, 0x0000); // Configuration du INA237 Peut modifier le gain bit 4 et mettre un temps avant de faire la mesure bit bit 6 pour 2ms d'attente
    inaWrite16(0x01, 0xF6BB); // Configuration de l'ADC pour dire quelle mesure on veut faire (continuous, moyenne sur 64 échantillons et toutes les mesure sur 540µs)
    inaWrite16(0x02, 562); // 0,15/2¹⁵*0,15*819,2*10⁶ =562 0,15=Ampères max attendu,0,15 resistance shunt, 2¹⁵= résolution du convertisseur

    delay(1000); // Attente pour que le capteur soit prêt
    connecterWifi();
    Serial.println("IP ESP32 : " + WiFi.localIP().toString());
    udp.begin(4210);//SERVEUR_PORT
    
    
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

   envoyerDonnees(data, udp);
    Serial.print("Rx = ");
    Serial.print(resistance);
    Serial.println(" Ω");
    delay(2000);

}
