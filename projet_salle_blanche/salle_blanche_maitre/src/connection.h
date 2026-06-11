//Ce code est inspiré par celui de Jean-Luc Béchennec
#include <Arduino.h>
#include <stdint.h>
#include <PubSubClient.h>
#include <WiFi.h>

class Connection {
public:
    typedef enum { OFFLINE, WIFI_STBY, WIFI_OK, MDNS_OK, MQTT_OK } State;

private:
    static State sState;
    static uint32_t sLastDate;
    static uint32_t sPeriod;
    static const char *sSsid;
    static const char *sPass;
    static const char *sName;
    static const char *sBrokerName;
    static uint8_t sChannel;
    static IPAddress sBrokerIP;
    static WiFiClient sNet;
    static PubSubClient sClient;

    typedef void (*MessageHandlingFunction)(char *, byte *, unsigned int);
    static MessageHandlingFunction sMessageHandler; 

    typedef void (*SubscriptionFunction)();
    static SubscriptionFunction sDoSubscriptions;

    Connection() {} 
    
    static void update();
    static void incomingMessage(char *inTopic, byte *inPayload, unsigned int inLength);
    static void performSubscriptions();

public:
    static void setup(const char *inSsid,
                      const char *inPass,
                      uint8_t inChannel,
                      const char *inName,
                      const char *inBrokerName,
                      const uint32_t inPeriod = 1000ul);
                      
    static void loop();
    static bool isOnline();
    static void subscribe(const char *inTopic);
    static void publish(const char *inTopic, const char *inValue);
    static void attach(SubscriptionFunction inSubFunction, MessageHandlingFunction inMessageHandler);
};