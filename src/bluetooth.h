#pragma once
#include "I2C.h"
#include <BluetoothSerial.h>

class BLEAdvertising;

void initBluetooth();
void envoyerDonnees(DonneesCapteur data);