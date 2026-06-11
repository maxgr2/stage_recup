#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // Indispensable pour changer le canal Wi-Fi à la volée
#include <Wire.h>
#include "I2C.h"

// -- Configuration -----------------------------------------
#define ROOM_ID           1
#define SLAVE_ID          1

uint8_t MASTER_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Sauvegarde du canal dans la mémoire RTC (survit au Deep Sleep) permte de connaitre le canal du maitre
RTC_DATA_ATTR int currentChannel = 6; 

typedef struct {
    uint8_t  room_id;
    uint8_t  slave_id;
    float    temperature;  
    float    humidity;     
    float    mc1p0;
    float    mc2p5;
    float    mc4p0;
    float    mc10p0;
    float    nc0p5;
    float    nc1p0;
    float    nc2p5;
    float    nc4p0;
    float    nc10p0;
    float    typicalParticleSize;
} SensorData;

SensorData donne_total;
bool peerAdded = false;

// Variables pour communiquer entre le callback et la boucle principale
volatile bool ackReceived = false;
volatile bool sendSuccess = false;

SPS30 sps30;
SHT40 sht40;

// -- Callback ESP-NOW ---------------------------------------
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    ackReceived = true;
    sendSuccess = (status == ESP_NOW_SEND_SUCCESS);
}

// -- Init ESP-NOW -------------------------------------------
bool initEspNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // On force l'antenne sur le dernier canal connu
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Erreur d'initialisation");
        return false;
    }
    
    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, MASTER_MAC, 6);
    peerInfo.channel = currentChannel; 
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESP-NOW] Erreur ajout du pair");
        return false;
    }

    return true;
}

// -- Setup --------------------------------------------------
void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    Serial.printf("\n=== Esclave Démarré | Canal RTC : %d ===\n", currentChannel);

    peerAdded = initEspNow();
    
    donne_total.room_id  = ROOM_ID;
    donne_total.slave_id = SLAVE_ID;
    
    sps30.wakeup();
}

// -- Loop ---------------------------------------------------
void loop() {
    //Acquisition des données
    sps30.startMeasurement(1); 
    delay(10000); 
    
    Capteur_PM_float pmData = sps30.mesure();
    sps30.stopMeasurement();
    SHT40_Data dhtData = sht40.readMeasurement();
    
    donne_total.temperature = dhtData.isValid ? dhtData.temperature : 0.0;
    donne_total.humidity = dhtData.isValid ? dhtData.humidity : 0.0;
    donne_total.mc1p0 = pmData.mc1p0;
    donne_total.mc2p5 = pmData.mc2p5;
    donne_total.mc4p0 = pmData.mc4p0;
    donne_total.mc10p0 = pmData.mc10p0;
    donne_total.nc0p5 = pmData.nc0p5;
    donne_total.nc1p0 = pmData.nc1p0;
    donne_total.nc2p5 = pmData.nc2p5;
    donne_total.nc4p0 = pmData.nc4p0;
    donne_total.nc10p0 = pmData.nc10p0;
    donne_total.typicalParticleSize = pmData.typicalParticleSize;
    
    //Envoi et gestion du réseau
    if (peerAdded) {
        ackReceived = false;
        esp_now_send(MASTER_MAC, (uint8_t *)&donne_total, sizeof(donne_total));

        // Attente de l'accusé de réception (Max 500ms pour ne pas bloquer)
        unsigned long startWait = millis();
        while (!ackReceived && millis() - startWait < 500) {
            delay(10);
        }

        if (sendSuccess) {
            Serial.printf("[ESP-NOW] Succès sur le canal %d.\n", currentChannel);
        } else {
            Serial.println("[ESP-NOW] Échec. Début du balayage des canaux...");
            
            //Ca n'a pas marché, on tente de trouver le canal du maître en balayant tous les canaux (1 à 13)
            for (int c = 1; c <= 13; c++) {
                if (c == currentChannel) continue; // On ne retente pas celui qui vient d'échouer

                Serial.printf("Test du canal %d...\n", c);
                
                // Modification physique du canal Wi-Fi
                esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
                
                // Mise à jour du pair ESP-NOW
                esp_now_peer_info_t peerInfo = {};
                memcpy(peerInfo.peer_addr, MASTER_MAC, 6);
                peerInfo.channel = c;
                esp_now_mod_peer(&peerInfo); 

                // Nouvel essai d'envoi
                ackReceived = false;
                esp_now_send(MASTER_MAC, (uint8_t *)&donne_total, sizeof(donne_total));

                startWait = millis();
                while (!ackReceived && millis() - startWait < 500) {
                    delay(10);
                }

                if (sendSuccess) {
                    currentChannel = c; // Sauvegarde du nouveau canal dans la variable globale
                    Serial.printf("Nouveau canal valide trouvé et sauvegardé : %d\n", currentChannel);
                    break; // On a trouvé le maître, on arrête de chercher
                }
            }
        }
    }
    
    // Attente avant le prochain cycle (ajusté pour garder une boucle d'environ 1 minute)
    //A voir si on mets un deep sleep à la place pour économiser de l'énergie
    delay(49000); 
}