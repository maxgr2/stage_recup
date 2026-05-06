#include <BluetoothSerial.h>
#include "I2C.h"

BluetoothSerial SerialBT;



void envoyerDonnees(DonneesCapteur data) {
  SerialBT.write((uint8_t*)&data, sizeof(data));

  Serial.printf("Envoyé : %.2fV | %.2fA | %.2fW | %.2fmV | %.1f°C | %.1f°C\n",
    data.tensionBus_V,
    data.courant_A,
    data.puissance_W,
    data.tensionShunt_mV,
    data.temperature_C,
    data.temperaturebatterie_C
  );
}

void initBluetooth() {
  SerialBT.begin("ESP32_batterie"); // nom visible lors du jumelage
  Serial.println("Bluetooth démarré, en attente de connexion...");
}

