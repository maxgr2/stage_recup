#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include "I2C.h"
#include "connection.h"
#include "ecran.h"

#define ROOM_ID           1
#define Master_ID         4
#define MESURE_INTERVALLE 60000ul
#define MESURE_DUREE      10000ul

// Config réseau
const char *SSID        = "SALLE_BLANCHE";
const char *PASSWORD    = "SALLE_BLANCHE";
const char *DEVICE_NAME = "esp32-gateway";
const char *BROKER_NAME = "192.168.42.1"; // nom mDNS du Pi
const char *TOPIC       = "ets/salle1/mesures";

typedef struct {
    uint8_t  room_id;
    uint8_t  slave_id;
    float    temperature;
    float    humidity;
    float    mc0p1, mc0p3, mc0p5, mc1p0, mc2p5, mc5p0;
    float    nc0p1, nc0p3, nc0p5, nc1p0, nc2p5, nc5p0;
    float    typicalParticleSize;
} SensorData;

SensorData donnees_maitre;
SensorData donnees_esclave;
volatile bool donnee_recue = false;

// Instances des capteurs[cite: 6, 7]
SPS30 sps30;
IPS7100 IPS7100;
SHT40 sht40;

// Énumération pour suivre quel capteur est branché
enum TypeCapteur { CAPTEUR_AUCUN, CAPTEUR_SPS30, CAPTEUR_IPS7100 };
TypeCapteur capteurActif = CAPTEUR_AUCUN;

// Deux timers bien séparés
unsigned long timerMinute = 0;  // cadence 60s entre cycles
unsigned long timerMesure = 0;  // attente 10s pour la mesure
bool mesureEnCours = false;


void doSubscriptions() {
    // Rien à faire mais nécessaire
}

 void onMessage(char *topic, byte *payload, unsigned int length) {
    // Traitement des messages entrants si besoin
    Serial.printf("[MQTT] Message reçu sur %s\n", topic);
}


void recep_espNow(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len == sizeof(SensorData)) {
        memcpy(&donnees_esclave, incomingData, sizeof(donnees_esclave));
        donnee_recue = true;
    }
}

void sendToMQTT(SensorData &data) {
    if (!Connection::isOnline()) {
        Serial.println("[MQTT] Pas connecté, donnée ignorée");
        return;
    }

    char jsonBuffer[512];
    //magie magie ca marche
    snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{\"room\":%d,\"slave\":%d,\"temp\":%.4f,\"hum\":%.4f,"
        "\"mc0p1\":%.4f,\"mc0p3\":%.4f,\"mc0p5\":%.4f,\"mc1p0\":%.4f,\"mc2p5\":%.4f,\"mc5p0\":%.4f,"
        "\"nc0p1\":%.1f,\"nc0p3\":%.1f,\"nc0p5\":%.1f,\"nc1p0\":%.1f,\"nc2p5\":%.1f,\"nc5p0\":%.1f}",
        data.room_id, data.slave_id, data.temperature, data.humidity,
        data.mc0p1, data.mc0p3, data.mc0p5, data.mc1p0, data.mc2p5, data.mc5p0,
        data.nc0p1, data.nc0p3, data.nc0p5, data.nc1p0, data.nc2p5, data.nc5p0
    );

    Connection::publish(TOPIC, jsonBuffer);
    Serial.printf("[MQTT] Données esclave %d envoyées\n", data.slave_id);
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("[WIFI-EVENT] event=%d\n", event);
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.printf("[WIFI-EVENT] Déconnecté, raison=%d\n", info.wifi_sta_disconnected.reason);
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(100000); 
    
    // --- ROUTINE DE DÉTECTION DU CAPTEUR I2C ---
    Serial.println("[I2C] Recherche des capteurs de particules...");
    
    // Test du SPS30 (Adresse 0x69)[cite: 6]
    Wire.beginTransmission(0x69);
    if (Wire.endTransmission() == 0) {
        Serial.println("[I2C] SPS30 détecté avec succès !");
        capteurActif = CAPTEUR_SPS30;
        sps30.wakeup(); 
        sps30.setAutoCleaningInterval(168); 
    } 
    else {
        // Test de l'IPS7100 (Adresse 0x4B)[cite: 6]
        Wire.beginTransmission(0x4B);
        if (Wire.endTransmission() == 0) {
            Serial.println("[I2C] IPS7100 détecté avec succès !");
            capteurActif = CAPTEUR_IPS7100;
        } else {
            Serial.println("[I2C] AVERTISSEMENT : Aucun capteur de particules détecté !");
        }
    }
    
    Connection::attach(doSubscriptions, onMessage);
    Serial.println("[DEBUG] Scan des réseaux WiFi...");
    int n = WiFi.scanNetworks();
    Serial.printf("[DEBUG] %d réseaux trouvés\n", n);
    for (int i = 0; i < n; i++) {
        Serial.printf("[DEBUG] %d: %s (canal %d, RSSI %d)\n", 
            i, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
    }
    WiFi.onEvent(onWiFiEvent);
    Connection::setup(SSID, PASSWORD, 0, DEVICE_NAME, BROKER_NAME);

    // ESP-NOW (WiFi doit être en STA, Connection::setup() le fait déjà)
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Erreur fatale");
        return;
    }
    esp_now_register_recv_cb(recep_espNow);

    timerMinute = millis();
}

void loop() {
    //On utilise des flags pour éviter que le code soit bloquant dans une tache
    Connection::loop();
    
    // 1. Vérification de la survie du WiFi 
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NET] WiFi perdu ! Tentative de reconnexion...");
        
        // On coupe tout proprement avant de relancer 
        WiFi.disconnect();
        delay(200);
        WiFi.begin(SSID, PASSWORD);
        
        // On attend 10 secondes maximum 
        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 20) {
            delay(500);
            Serial.print(".");
            timeout++;
        }
        
        // La solution nucléaire : si ça bloque toujours, on redémarre la carte ! 
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("\n[NET] Échec fatal du WiFi. Redémarrage de l'ESP32...");
            ESP.restart(); 
        }
        Serial.println("\n[NET] WiFi reconnecté !");
    }

    // Envoie des données esclave à la rasp 
    if (donnee_recue) {
        donnee_recue = false;
        sendToMQTT(donnees_esclave);
    }

    unsigned long now = millis();

    // Déclenchement mesure toutes les X secondes 
    if (!mesureEnCours && (now - timerMinute >= MESURE_INTERVALLE)) {
        if (capteurActif == CAPTEUR_SPS30) {
            sps30.startMeasurement(1); 
        } else if (capteurActif == CAPTEUR_IPS7100) {
            IPS7100.startMeasurement();
        }

        if (capteurActif != CAPTEUR_AUCUN) {
            mesureEnCours = true;
            timerMesure = now; //10s de délai 
            Serial.println("[CAPTEUR] Lancement mesure maître...");
        } else {
            // Si pas de capteur, on relance le timer pour ne pas bloquer le programme
            timerMinute = now; 
        }
    }

    // On attends 10S pour laisser le temps de faire la mesure avant de lire les données 
    if (mesureEnCours && (now - timerMesure >= MESURE_DUREE)) {
        
        if (capteurActif == CAPTEUR_SPS30) {
            Capteur_PM_float pmData = {0}; 
            pmData = sps30.mesure(); 
            sps30.stopMeasurement(); 
            Serial.println("[SPS30] Mesure maître terminée.");
            //SHT40_Data dhtData = sht40.readMeasurement(); 
        //SHT30_Data dhtData = SHT30.readMeasurement();
        donnees_maitre.room_id   = ROOM_ID; 
        donnees_maitre.slave_id  = Master_ID; 
        /*if (!dhtData.valid) {
            Serial.println("[SHT30] Erreur de lecture du capteur SHT30 !");
            donnees_maitre.temperature = NAN; 
            donnees_maitre.humidity    = NAN; 
        } else {
            Serial.printf("[SHT30] Température: %.2f °C, Humidité: %.2f %%\n", dhtData.temperature, dhtData.humidity);
        }*/

        donnees_maitre.mc1p0  = pmData.mc1p0; 
        donnees_maitre.mc2p5  = pmData.mc2p5; 
        donnees_maitre.nc0p5  = pmData.nc0p5; 
        donnees_maitre.nc1p0  = pmData.nc1p0; 
        donnees_maitre.nc2p5  = pmData.nc2p5; 

        } else if (capteurActif == CAPTEUR_IPS7100) {
            Capteur_PM_IPS_float pmData = {0}; 

            pmData = IPS7100.mesure();
            IPS7100.stopMeasurement();
            Serial.println("[IPS7100] Mesure maître terminée.");
            //SHT40_Data dhtData = sht40.readMeasurement(); 
        //SHT30_Data dhtData = SHT30.readMeasurement();
        donnees_maitre.room_id   = ROOM_ID; 
        donnees_maitre.slave_id  = Master_ID; 
        /*if (!dhtData.valid) {
            Serial.println("[SHT30] Erreur de lecture du capteur SHT30 !");
            donnees_maitre.temperature = NAN; 
            donnees_maitre.humidity    = NAN; 
        } else {
            Serial.printf("[SHT30] Température: %.2f °C, Humidité: %.2f %%\n", dhtData.temperature, dhtData.humidity);
        }*/

        donnees_maitre.mc1p0  = pmData.mc1p0; 
        donnees_maitre.mc2p5  = pmData.mc2p5; 
        donnees_maitre.mc5p0  = pmData.mc5p0; 
        donnees_maitre.nc0p5  = pmData.nc0p5; 
        donnees_maitre.nc1p0  = pmData.nc1p0; 
        donnees_maitre.nc2p5  = pmData.nc2p5; 
        donnees_maitre.nc5p0 = pmData.nc5p0;
        donnees_maitre.nc0p1 = pmData.nc0p1;
        donnees_maitre.nc0p3 = pmData.nc0p3;
        donnees_maitre.nc0p5 = pmData.nc0p5;
        donnees_maitre.mc0p1 = pmData.mc0p1;
        donnees_maitre.mc0p3 = pmData.mc0p3;
        donnees_maitre.mc0p5 = pmData.mc0p5;
        Serial.printf("PM1.0: %.2f µg/m³, PM2.5: %.2f µg/m³, PM4.0: %.2f µg/m³,\n", pmData.mc1p0, pmData.mc2p5, pmData.mc5p0); 
        Serial.printf("NC0.1: %.2f #/cm³, NC0.3: %.2f #/cm³, NC0.5: %.2f #/cm³, NC1.0: %.2f #/cm³, NC5: %.2f #/cm³\n", pmData.nc0p1, pmData.nc0p3, pmData.nc0p5, pmData.nc1p0, pmData.nc5p0); 

        }

        

        
        sendToMQTT(donnees_maitre); 

        mesureEnCours = false; 
        timerMinute = now; // repart pour 60s 
    }
}