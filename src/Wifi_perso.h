#include <WiFi.h>
#include <WiFiUdp.h>


void connecterWifi();
void envoyerDonnees(DonneesCapteur data, WiFiUDP& udp);