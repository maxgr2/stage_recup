
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <string>
#include <Arduino.h>
#include "I2C.h"


//Cette fonction permet d'envoyer les données pour seulement une batterie
void envoierDonnees(DonneesCapteur data, BLEAdvertising *pAdvertising, int NUMERO_BATTERIE) {
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

    // Payload : [0xFF 0xFF] [4 octets chip ID] [1 octet num batterie] [24 octets données]
    std::string myData = "";
    myData += (char)0xFF;  // ID fabricant byte 1
    myData += (char)0xFF;  // ID fabricant byte 2

    // Chip ID (4 octets) — dérivé de l'adresse MAC unique de chaque ESP32
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    uint32_t chipID = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
    chipID =chipID+1; //Pour tester coté serveur    
    uint8_t* idBytes = (uint8_t*)&chipID;
    for (int i = 0; i < 4; i++) {
        myData += (char)idBytes[i];
    }

    // Numéro de batterie (1 octet)
    myData += (char)NUMERO_BATTERIE;

    // 6 floats de données (24 octets)
      auto appendInt16 = [&](float val, float facteur) {
        int16_t v = (int16_t)(val * facteur);
        myData += (char)(v & 0xFF);
        myData += (char)((v >> 8) & 0xFF);
    };

    appendInt16(data.tensionBus_V,          100.0);  // précision 0.01 V
    appendInt16(data.courant_A,             1000.0); // précision 0.001 A
    appendInt16(data.puissance_W,           10.0);   // précision 0.1 W
    appendInt16(data.tensionShunt_mV,       100.0);  // précision 0.01 mV
    appendInt16(data.temperature_C,         10.0);   // précision 0.1 °C
    appendInt16(data.temperaturebatterie_C, 10.0);   // précision 0.1 °C
    appendInt16(data.tensionBus_charge_V, 100.0);  // précision 0.01 V

    // Total : 2 + 4 + 1 + 12 = 19 octets  ✓ largement sous la limite
    oAdvertisementData.setManufacturerData(myData);
    oAdvertisementData.setFlags(0x04);
    pAdvertising->setAdvertisementData(oAdvertisementData);
    pAdvertising->start();

    Serial.printf("Diffusion | ChipID: %08X | Batterie: %d | %.2fV | %.2fA | %.2fW | %.2fmV | %.1f°C | %.1f°C\n",
        chipID,
        NUMERO_BATTERIE,
        data.tensionBus_V,
        data.courant_A,
        data.puissance_W,
        data.tensionShunt_mV,
        data.temperature_C,
        data.temperaturebatterie_C
    );

    delay(500);
    pAdvertising->stop();
}