
#include <Arduino.h>
#include "Temperature.h"
#include "I2C.cpp"
#include <Wire.h>

void setup() {
    
    
    Wire.begin(); //I2C init
     Serial.begin(115200);
}

void loop() {
    int temp_moy=0;
    for (int i=0; i<10;i++){
        temp_moy += temperature(1);
        
    }
    float resistance = temp_moy / 10.0;
    Serial.print("Rx = ");
    Serial.print(resistance);
    Serial.println(" Ω");
    delay(1000);

}
