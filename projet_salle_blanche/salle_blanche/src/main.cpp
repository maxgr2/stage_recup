#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>




void setup() {
  Serial.begin(115200);
  Wire.begin(); //I2C init
  WiFi.begin("your_SSID", "your_PASSWORD");

  
}

void loop() {
  // put your main code here, to run repeatedly:
}