
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <string>
#include <Arduino.h>
#include "I2C.h"


//Cette fonction permet d'envoyer les données pour seulement une batterie
void envoierDonnees(DonneesCapteur data, BLEAdvertising *pAdvertising) {
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

    // Construction du payload binaire
    // Structure : [0xFF 0xFF] [24 octets de données] = 26 octets total
    std::string myData = "";
    myData += (char)0xFF;  // ID fabricant byte 1
    myData += (char)0xFF;  // ID fabricant byte 2

    // Copie de la structure DonneesCapteur, pour envoie
    auto appendFloat = [&](float val) {
    uint8_t* bytes = (uint8_t*)&val;
    for (int i = 0; i < 4; i++) {
        myData += (char)bytes[i];
    }
    };

    appendFloat(data.tensionBus_V);
    appendFloat(data.courant_A);
    appendFloat(data.puissance_W);
    appendFloat(data.tensionShunt_mV);
    appendFloat(data.temperature_C);
    appendFloat(data.temperaturebatterie_C);

    oAdvertisementData.setManufacturerData(myData);
    oAdvertisementData.setFlags(0x04);  // BR_EDR_NOT_SUPPORTED

    pAdvertising->setAdvertisementData(oAdvertisementData);
    pAdvertising->start();

    Serial.printf("Diffusion : %.2fV | %.2fA | %.2fW | %.2fmV | %.1f°C | %.1f°C\n",
    data.tensionBus_V,
    data.courant_A,
    data.puissance_W,
    data.tensionShunt_mV,
    data.temperature_C,
    data.temperaturebatterie_C
    );

    delay(500); //Diffusion pendant 500ms donnée envoye 4-5 fois

    pAdvertising->stop();
}