/*
 * ESP32 ESCLAVE — capteur particules SPS30 (Sensirion)
 * ---------------------------------------------------------
 * - Lit DHT22 (température + humidité)
 * - Lit SPS30 en I²C (PM1.0, PM2.5, PM4.0, PM10)
 * - Envoie les données via ESP-NOW au maître de la salle
 *
 * CÂBLAGE SPS30 en mode I²C
 *   Pin 1 (VDD)  → 5V
 *   Pin 2 (SDA)  → GPIO21  (+ pull-up 10kΩ vers 3.3V)
 *   Pin 3 (SCL)  → GPIO22  (+ pull-up 10kΩ vers 3.3V)
 *   Pin 4 (SEL)  → GND     (sélectionne le mode I²C)
 *   Pin 5 (GND)  → GND
 *
 * CONFIGURATION
 *   Relever l'adresse MAC du maître via son Serial Monitor
 *   Coller l'adresse dans MASTER_MAC ci-dessous
 *   Ajuster ROOM_ID et SLAVE_ID
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include "I2S.h"

// -- Configuration -----------------------------------------
#define ROOM_ID           1
#define SLAVE_ID          1
#define DHT_PIN           4
#define DHT_TYPE          DHT22
#define SEND_INTERVAL_MS  5000

// Adresse MAC du maître — à remplir une fois rasp recut
uint8_t MASTER_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// -- Structure de données (identique dans le maître) --------
typedef struct {
    uint8_t  room_id;
    uint8_t  slave_id;
    float    temperature;  // °C
    float    humidity;     // %
    uint16_t pm1_0;        // µg/m³
    uint16_t pm2_5;        // µg/m³
    uint16_t pm10;         // µg/m³
    bool     dht_ok;
    bool     pms_ok;
} SensorData;

// -- Objets globaux -----------------------------------------
SensorData payload;
bool      peerAdded  = false;
bool      sps30Ready = false;
unsigned long lastSendMs  = 0;

// -- Callback ESP-NOW ---------------------------------------
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    Serial.printf("[ESP-NOW] Envoi %s\n",
        status == ESP_NOW_SEND_SUCCESS ? "OK" : "ÉCHEC");
}

// -- Init ESP-NOW -------------------------------------------
bool initEspNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Initialisation échouée !");
        return false;
    }
    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, MASTER_MAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESP-NOW] Ajout du pair échoué !");
        return false;
    }

    Serial.println("[ESP-NOW] Initialisé.");
    return true;
}



// -- Setup --------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.printf("\n=== Esclave Salle %d, ID %d ===\n", ROOM_ID, SLAVE_ID);

    WiFi.mode(WIFI_STA);
    Serial.printf("MAC esclave : %s\n", WiFi.macAddress().c_str());


    peerAdded = initEspNow();

    payload.room_id  = ROOM_ID;
    payload.slave_id = SLAVE_ID;
}





// -- Envoi ESP-NOW ------------------------------------------
void sendData() {
    if (!peerAdded) return;

    esp_err_t err = esp_now_send(
        MASTER_MAC,
        (uint8_t *)&payload,
        sizeof(payload)
    );

    if (err != ESP_OK) {
        Serial.printf("[ESP-NOW] Erreur : %d\n", err);
    }
}

// -- Loop ---------------------------------------------------
void loop() {
    if (millis() - lastSendMs >= SEND_INTERVAL_MS) {
        lastSendMs = millis();
        sendData();
    }
}