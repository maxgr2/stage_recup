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
#define nb_cartefille 1
int led_state=HIGH;


uint8_t adresses_i2c_filles[nb_cartefille]; //Veuillez le remplir avec le nombre de batterie qui sera branché sur chaque carte fille


//variable globale
BLEAdvertising *pAdvertising;

//Fonctions envoie bluetooth

// Définir un seuil de tension minimum pour considérer qu'une batterie est présente
// (Ajuste cette valeur selon ton type de batterie, ex: 1.0V)
const float SEUIL_PRESENCE_BATTERIE = 1.0; 

void mesurer_filles() {
    for (int index_carte = 0; index_carte < nb_cartefille; index_carte++) {
        uint8_t adresse_actuelle = adresses_i2c_filles[index_carte];
        
        // On boucle sur le nombre maximum d'emplacements possibles sur une carte fille (ex: 4)
        for (int bat = 1; bat <= 4; bat++) {
            
            // 1. Allumer le SSR de cet emplacement pour relier la batterie au INA237
            ssrOn_fille(bat, adresse_actuelle); // (Attention à adapter l'index du SSR selon ton routage)
            delay(500); // Laisser le temps au relais de coller et à la tension de se stabiliser
            
            // 2. Mesurer uniquement la tension V_bus en premier
            float v_bus = inaLireTensionBus();
            
            // 3. Vérifier si la batterie est réellement là
            if (v_bus > SEUIL_PRESENCE_BATTERIE) {
                
                // La batterie est là ! On récupère le reste des données
                DonneesCapteur data;
                data.tensionBus_V = v_bus;
                data.courant_A = inaLireCourant();
                data.tensionShunt_mV = inaLireTensionShunt();
                data.temperature_C = inaLireTemperature();
                
                // ... (Ici tu peux ajouter ta logique de lecture de température batterie) ...

                // On diffuse en Bluetooth
                envoierDonnees(data, pAdvertising, bat);
                
                Serial.printf("Carte 0x%02X | Emplacement %d : Batterie détectée (%.2fV)\n", adresse_actuelle, bat, v_bus);
                
            } else {
                // Pas de batterie, on passe à la suivante en silence (ou avec un debug)
                // Serial.printf("Carte 0x%02X | Emplacement %d : Vide\n", adresse_actuelle, bat);
            }
            
            // 4. On éteint impérativement le SSR avant de passer à l'emplacement suivant
            ssrOff_fille(bat, adresse_actuelle);
            delay(100); // Petit délai de sécurité entre deux commutations
        }
    }
}

DonneesCapteur mesures(int bat) {
  // Éteindre tous les CONT_MES
  ssrOff(7); ssrOff(4); ssrOff(2); ssrOff(0);
  ssrOff(9); ssrOff(10); ssrOff(11); ssrOff(12); ssrOff(13); ssrOff(14);
  ssrOff(8);
  ssrOff(15); // 3.3V_temp
  int r;
  // Si cette batterie alimente, basculer sur une autre
  if (batAlimActuelle==bat){
    r= (esp_random()%3) +1;
    while (r==bat){
      r= (esp_random()%3) +1;
    }
    if (r==3){
      r=4;
    }
  }else {
    r=batAlimActuelle;
  }
  alimentation_on(r);
  delay(100);
  alimentation_off(bat);
  batAlimActuelle=r;



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
    default: Serial.println("Batterie inconnue"); return {};
  }

  ssrOn(15); // 3.3V_temp
  delay(500);

  DonneesCapteur m;
  m.tensionBus_V = inaLireTensionBus();

  if (m.tensionBus_V > 0.1f) {
    ssrOn(8); // MES_TOT
    delay(500);
    m.tensionBus_charge_V = inaLireTensionBus();
    m.courant_A           = inaLireCourant();
    m.tensionShunt_mV     = inaLireTensionShunt();
    m.puissance_W         = m.tensionBus_charge_V * m.courant_A;
    ssrOff(8);
  } else {
    m.tensionBus_charge_V = 0.0f;
    m.courant_A           = 0.0f;
    m.tensionShunt_mV     = 0.0f;
    m.puissance_W         = 0.0f;
  }

  m.temperature_C         = inaLireTemperature();
  m.temperaturebatterie_C = 0.0f;

  ssrOff(15); // 3.3V_temp
  return m;
}


void faittous(int bat){
    //alimentation_on(bat);

    
    
    DonneesCapteur data=mesures(bat); // Mesure pour la batterie 1
    int temp_moy=0;
    for (int i=0; i<10;i++){
        temp_moy += temperature(bat);
    }
    float resistance = temp_moy / 10.0;
    resistance = temp_moy / 10.0;
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
    delay(1000); // Attente que le système soit stable après le démarrage
    BLEDevice::init("Esp_batterie");
    delay(1000);
    //config I2C
    Wire.begin(); //I2C init
    Serial.begin(115200);
    pinMode(LED_PIN2, OUTPUT);
    digitalWrite(LED_PIN2, HIGH);
    mcpInit(); // Initialisation du MCP23017
    pinMode(LED_PIN_1, OUTPUT);
    batAlimActuelle = 1;  // ou 2 ou 4
    alimentation_on(batAlimActuelle);
    
    

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

    
    for (int i=1;i<11;i++){
        if (i!=3){
            faittous(i);
        }
    }
    digitalWrite(LED_PIN2, LOW);

    // ── Deep sleep
    Serial.print("Deep sleep");
    Serial.flush();  // vider le buffer série avant de dormir
    digitalWrite(LED_PIN2,LOW);
 
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_TIME * 1000000ULL);
    esp_deep_sleep_start();

}
int i=1;


void loop(){
    /*
    digitalWrite(LED_PIN2, led_state);
    faittous(i);
    if (i==11){
        i=1;

    }
    else{
        if (i==2){
            i=4;
        }else{
        i++;
        }
    }
    if (led_state==HIGH){
        led_state=LOW;
    }
    else{
        led_state=HIGH;
    }*/
}
