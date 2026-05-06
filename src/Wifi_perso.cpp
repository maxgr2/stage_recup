#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "I2C.h"

const char* SSID     = "TonWifi";
const char* PASSWORD = "TonMotDePasse";
const char* SERVER_IP   = "192.168.1.100"; // IP de ton PC
const int   SERVER_PORT = 4210;


void connecterWifi() {
  Serial.print("Connexion WiFi...");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnecté ! IP : " + WiFi.localIP().toString());
}

void envoyerDonnees(DonneesCapteur data, WiFiUDP& udp) {
  udp.beginPacket(SERVER_IP, SERVER_PORT);
  udp.write((uint8_t*)&data, sizeof(data));
  udp.endPacket();

  Serial.printf("Envoyé : %.2fV | %.2fA | %.2fW | %.2fmV | %.1f°C | %.1f°C\n",
    data.tensionBus_V,
    data.courant_A,
    data.puissance_W,
    data.tensionShunt_mV,
    data.temperature_C,
    data.temperaturebatterie_C
  );
}