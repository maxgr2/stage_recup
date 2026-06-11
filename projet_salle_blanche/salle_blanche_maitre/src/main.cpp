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
const char *BROKER_NAME = "raspberrypi"; // nom mDNS du Pi
const char *TOPIC       = "ets/salle1/mesures";

typedef struct {
    uint8_t  room_id;
    uint8_t  slave_id;
    float    temperature;
    float    humidity;
    float    mc1p0, mc2p5, mc4p0, mc10p0;
    float    nc0p5, nc1p0, nc2p5, nc4p0, nc10p0;
    float    typicalParticleSize;
} SensorData;

SensorData donnees_maitre;
SensorData donnees_esclave;
volatile bool donnee_recue = false;

SPS30 sps30;
SHT40 sht40;

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
        "{\"room\":%d,\"slave\":%d,\"temp\":%.2f,\"hum\":%.2f,"
        "\"mc1p0\":%.2f,\"mc2p5\":%.2f,\"mc4p0\":%.2f,\"mc10p0\":%.2f,"
        "\"nc0p5\":%.2f,\"nc1p0\":%.2f,\"nc2p5\":%.2f,\"nc4p0\":%.2f,"
        "\"nc10p0\":%.2f,\"tps\":%.2f}",
        data.room_id, data.slave_id, data.temperature, data.humidity,
        data.mc1p0, data.mc2p5, data.mc4p0, data.mc10p0,
        data.nc0p5, data.nc1p0, data.nc2p5, data.nc4p0,
        data.nc10p0, data.typicalParticleSize
    );

    Connection::publish(TOPIC, jsonBuffer);
    Serial.printf("[MQTT] Données esclave %d envoyées\n", data.slave_id);
}


void setup() {
    Serial.begin(115200);
    Wire.begin();

    Connection::attach(doSubscriptions, onMessage);
    Connection::setup(SSID, PASSWORD, 6, DEVICE_NAME, BROKER_NAME);

    // ESP-NOW (WiFi doit être en STA, Connection::setup() le fait déjà)
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Erreur fatale");
        return;
    }
    esp_now_register_recv_cb(recep_espNow);

    sps30.wakeup();
    timerMinute = millis();
}

void loop() {
    //On utilise des flags pour éviter que le code soit bloquant dans une tache
    Connection::loop();

    // Envoie des données esclave à la rasp
    if (donnee_recue) {
        donnee_recue = false;
        sendToMQTT(donnees_esclave);
    }

    unsigned long now = millis();

    // Déclenchement mesure toutes les X secondes
    if (!mesureEnCours && (now - timerMinute >= MESURE_INTERVALLE)) {
        sps30.startMeasurement(1);
        mesureEnCours = true;
        timerMesure = now; //10s pour SPS30
        Serial.println("[SPS30] Lancement mesure maître...");
    }

    // On attends 10S pour laisser le temps de faire la mesure avant de lire les données
    if (mesureEnCours && (now - timerMesure >= MESURE_DUREE)) {
        Capteur_PM_float pmData = sps30.mesure();
        sps30.stopMeasurement();
        SHT40_Data dhtData = sht40.readMeasurement();

        donnees_maitre.room_id   = ROOM_ID;
        donnees_maitre.slave_id  = Master_ID;
        donnees_maitre.temperature = dhtData.isValid ? dhtData.temperature : 0.0f;
        donnees_maitre.humidity    = dhtData.isValid ? dhtData.humidity    : 0.0f;
        donnees_maitre.mc1p0  = pmData.mc1p0;
        donnees_maitre.mc2p5  = pmData.mc2p5;
        donnees_maitre.mc4p0  = pmData.mc4p0;
        donnees_maitre.mc10p0 = pmData.mc10p0;
        donnees_maitre.nc0p5  = pmData.nc0p5;
        donnees_maitre.nc1p0  = pmData.nc1p0;
        donnees_maitre.nc2p5  = pmData.nc2p5;
        donnees_maitre.nc4p0  = pmData.nc4p0;
        donnees_maitre.nc10p0 = pmData.nc10p0;
        donnees_maitre.typicalParticleSize = pmData.typicalParticleSize;

        sendToMQTT(donnees_maitre);

        mesureEnCours = false;
        timerMinute = now; // repart pour 60s
    }
}