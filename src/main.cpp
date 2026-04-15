#include <Arduino.h>
/* #include "lib\ble_helpers.h"
#include "lib\config.h"
#include "i2c_helpers.h"
#include "sensor_helpers.h" */

#define R_FIXED     10000.0     //Valeur de résistance pour diviseur de tension
#define BETA        4150        //3950.0
#define T0          298.15
#define R0          10000.0     //Valeur de la thermistance
void setup(){
    Serial.begin(115200);
    /*pinMode(MOSFET_GATE_PIN, OUTPUT);
    digitalWrite(MOSFET_GATE_PIN, LOW);

   /*  
    spi_init();

    ble_init(); 

    Serial.println("BLE device started, waiting for client...");*/
}
void loop() {

    //Récupération de la témpérature des 3 thermistances et affichage dans le moniteur série
    int adc_1_0 = analogRead(36);
    int adc_1_1 = analogRead(39);
    int adc_1_2 = analogRead(34);

    float v1 = 3.3 * adc_1_0 / 4095.0;
    float v2 = 3.3 * adc_1_1 / 4095.0;
    float v3 = 3.3 * adc_1_2 / 4095.0;

    
    float r1 = R_FIXED * v1 / (3.3 - v1);
    float r2 = R_FIXED * v2 / (3.3 - v2);
    float r3 = R_FIXED * v3 / (3.3 - v3);

    float tempC1 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r1/R0)) - 273.15) + 5.0;
    float tempC2 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r2/R0)) - 273.15) + 5.0;
    float tempC3 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r3/R0)) - 273.15) + 5.0;

    Serial.print("Temp 1: "); Serial.print(tempC1); Serial.print(" °C, ");
    Serial.print("Temp 2: "); Serial.print(tempC2); Serial.print(" °C, ");
    Serial.print("Temp 3: "); Serial.print(tempC3); Serial.println(" °C");


}
