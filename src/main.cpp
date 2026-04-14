#include <Arduino.h>
#include "lib\ble_helpers.h"
#include "lib\config.h"
#include "i2c_helpers.h"
#include "sensor_helpers.h"


void setup(){
    Serial.begin(115200);
    pinMode(MOSFET_GATE_PIN, OUTPUT);
    digitalWrite(MOSFET_GATE_PIN, LOW);

    /* --- i2c init --- */
    spi_init();

    /* --- BLE setup --- */
    ble_init();

    Serial.println("BLE device started, waiting for client...");
}
void loop() {
  ble_check_connection();
}
