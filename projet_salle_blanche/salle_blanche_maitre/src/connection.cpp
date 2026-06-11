//Ce code est inspiré par celui de Jean-Luc Béchennec
#include "connection.h"
#include <ESPmDNS.h>

Connection::State Connection::sState = OFFLINE;
uint32_t Connection::sLastDate = 0ul;
uint32_t Connection::sPeriod = 1000ul;
const char *Connection::sSsid = NULL;
const char *Connection::sPass = NULL;
const char *Connection::sName = NULL;
const char *Connection::sBrokerName = NULL;
uint8_t Connection::sChannel = 6;
IPAddress Connection::sBrokerIP = {0, 0, 0, 0};
WiFiClient Connection::sNet;
PubSubClient Connection::sClient(Connection::sNet);
Connection::MessageHandlingFunction Connection::sMessageHandler = NULL; 
Connection::SubscriptionFunction Connection::sDoSubscriptions = NULL;

void Connection::update() {
    switch (sState) {
        case OFFLINE:
            if (sSsid != NULL && sPass != NULL) {
                Serial.print("[NET] Connexion WiFi à : ");
                Serial.println(sSsid);
                WiFi.begin(sSsid, sPass, sChannel);
                sState = WIFI_STBY;
            }
            break;
            
        case WIFI_STBY:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[NET] WiFi Connecté !");
                if (sName != NULL) {
                    MDNS.begin(sName);
                    Serial.printf("[NET] mDNS actif : %s.local\n", sName);
                }
                sState = WIFI_OK;
            }
            break;
            
        case WIFI_OK:
            if (WiFi.status() == WL_CONNECTED) {
                if (sBrokerName != NULL) {
                    Serial.printf("[NET] Recherche du Broker MQTT : %s.local ...\n", sBrokerName);
                    sBrokerIP = MDNS.queryHost(sBrokerName);
                    if (sBrokerIP != IPAddress(0, 0, 0, 0)) {
                        Serial.printf("[NET] Broker trouvé : %s\n", sBrokerIP.toString().c_str());
                        sState = MDNS_OK;
                    }
                }
            } else {
                sState = OFFLINE;
            }
            break;

        case MDNS_OK:
            if (WiFi.status() == WL_CONNECTED) {
                sClient.setServer(sBrokerIP, 1883);
                sClient.setCallback(incomingMessage);
                sClient.setKeepAlive(15);
                
                // Connexion directe à MQTT
                Serial.print("[NET] Connexion MQTT...");
                if (sClient.connect(sName)) {
                    Serial.println(" OK !");
                    performSubscriptions();
                    sState = MQTT_OK;
                } else {
                    Serial.print(" Refusée, rc=");
                    Serial.println(sClient.state());
                }
            } else {
                sState = OFFLINE;
            }
            break;

        case MQTT_OK:
            if (WiFi.status() == WL_CONNECTED) {
                if (!sClient.connected()) {
                    Serial.println("[NET] Perte de connexion MQTT");  
                    sState = MDNS_OK; // Retour à l'étape précédente pour se reconnecter
                }
            } else {
                Serial.println("[NET] Perte de connexion WiFi");
                sState = OFFLINE;
            }
            break;
    }
}

void Connection::incomingMessage(char *inTopic, byte *inPayload, unsigned int inLength) {
    if (sMessageHandler != NULL) sMessageHandler(inTopic, inPayload, inLength);
}

void Connection::performSubscriptions() {
    if (sDoSubscriptions != NULL) sDoSubscriptions();
}

void Connection::attach(SubscriptionFunction inSubFunction, MessageHandlingFunction inMessageHandler) {
    sDoSubscriptions = inSubFunction;
    sMessageHandler = inMessageHandler;
}

void Connection::setup(const char *inSsid, const char *inPass, uint8_t inChannel, 
                       const char *inName, const char *inBrokerName, 
                       const uint32_t inPeriod) {
    sPeriod = inPeriod;
    sSsid = inSsid;
    sPass = inPass;
    sChannel = inChannel;
    sName = inName;
    sBrokerName = inBrokerName;
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
}

void Connection::loop() {
    const uint32_t currentDate = millis();
    if ((currentDate - sLastDate) >= sPeriod) {
        update();
        sLastDate = currentDate;
    }
    // ArduinoOTA.handle() a été supprimé ici
    sClient.loop();
}

bool Connection::isOnline() { 
    return (sState == MQTT_OK);
}

void Connection::subscribe(const char *inTopic) {
    sClient.subscribe(inTopic);
}

void Connection::publish(const char *inTopic, const char *inValue) {
    sClient.publish(inTopic, inValue);
}