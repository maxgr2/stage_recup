/**
 * @file ble_helpers.c
 * @author Nicolas Boulanger
 * @author Maxime Goncalves
 * @author Dominique Richer
 * @author Emilie Croteau
 * @date 2025-07-28
 * @brief Contient les fonctions pour interagir le module bluetooth.
 *        Gestion de la connection et de génération du message d'advertisement.
 * @defgroup BLE_HELPERS BLE helper functions
 * @copyright
 */
#include <Arduino.h>
#include <math.h>
#include <ArduinoJson.h> // For JSON handling
#include "config.h"
#include "ble_helpers.h"
#include "sensor_helpers.h"
#include <BLEDevice.h>

// Déclaration des classe pour le BLE
BLEServer* pServer = NULL;
BLECharacteristic *jsonCharacteristic = NULL;

// Variable pour permettre advertisement BLE
bool deviceConnected = false;   //!< Est-ce qu'un device est connecter
bool oldDeviceConnected = false;

// --- BLE Server callbacks ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer){ 
        deviceConnected = true;
        Serial.println("Device connected");
    }
    void onDisconnect(BLEServer* pServer){ 
        deviceConnected = false;
        pServer->startAdvertising();
        Serial.println("Device disconnected");
    }
};

void getNodeName(char* name);

void ble_init(void)
{
    // Get esp mac address and generate device node name
    uint64_t chipid = ESP.getEfuseMac();
    char name[32];
    sprintf(name, "BattNode_%04x", (uint16_t)(chipid >> 32));

    BLEDevice::init(name);
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID), 30);

    jsonCharacteristic = pService->createCharacteristic(
        CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    jsonCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}

void ble_check_connection()
{
    if(deviceConnected){
        jsonAdd(jsonCharacteristic);
        delay(UPDATE_TIME); // update every 2s
    }
    if (!deviceConnected && oldDeviceConnected) {
        pServer->startAdvertising();
        oldDeviceConnected = deviceConnected;
    }
    // Handle new connection
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}

void jsonAdd(BLECharacteristic *pCharacteristic)
{
    float R, Vopen, Vload;
    bool ok = measure_Rint_once(R, Vopen, Vload);

    // Read thermistors
    int adc_1_0 = analogRead(THERMISTOR_1_PIN);
    int adc_1_1 = analogRead(THERMISTOR_2_PIN);
    int adc_1_2 = analogRead(THERMISTOR_3_PIN);

    float v1 = 3.3 * adc_1_0 / 4095.0;
    float v2 = 3.3 * adc_1_1 / 4095.0;
    float v3 = 3.3 * adc_1_2 / 4095.0;

    float r1 = R_FIXED * v1 / (3.3 - v1);
    float r2 = R_FIXED * v2 / (3.3 - v2);
    float r3 = R_FIXED * v3 / (3.3 - v3);

    float tempC1 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r1/R0)) - 273.15) + 5.0;
    float tempC2 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r2/R0)) - 273.15) + 5.0;
    float tempC3 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r3/R0)) - 273.15) + 5.0;

    char output[256];
    size_t outputCapacity = 256;
    JsonDocument doc;

    char node_name[32];
    getNodeName(node_name);

    doc["name"] = node_name;
    doc["type"] = "12v";

    JsonArray sensors = doc["sensors"].to<JsonArray>();

    JsonObject sensors_0 = sensors.add<JsonObject>();
    sensors_0["sensor"] = "temp_1";
    sensors_0["data"] = tempC1;

    JsonObject sensors_1 = sensors.add<JsonObject>();
    sensors_1["sensor"] = "temp_2";
    sensors_1["data"] = tempC2;

    JsonObject sensors_2 = sensors.add<JsonObject>();
    sensors_2["sensor"] = "temp_3";
    sensors_2["data"] = tempC3;

    JsonObject sensors_3 = sensors.add<JsonObject>();
    sensors_3["sensor"] = "voltage";
    sensors_3["data"] = Vopen;

    JsonObject sensors_4 = sensors.add<JsonObject>();
    sensors_4["sensor"] = "impedance";
    sensors_4["data"] = ok ? R : 0.0;

    doc.shrinkToFit(); // optional

    size_t n = serializeJson(doc, output);
    pCharacteristic->setValue((uint8_t*)output, n);
    pCharacteristic->notify();
}

void getNodeName(char name[32])
{
    uint64_t chipid = ESP.getEfuseMac();
    sprintf(name, "BattNode_%04x", (uint16_t)(chipid >> 32));
}
