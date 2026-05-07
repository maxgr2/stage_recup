#pragma once
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <string>
#include <Arduino.h>
#include "I2C.h"


void envoierDonnees(DonneesCapteur data, BLEAdvertising *pAdvertising, int NUMERO_BATTERIE);