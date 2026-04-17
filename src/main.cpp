#include <Arduino.h>
#include <driver/dac.h>
#include <time.h>

//#include <HardwareTimer.h> 

/* #include "lib\ble_helpers.h"
#include "lib\config.h"
#include "i2c_helpers.h"
#include "sensor_helpers.h" */



#define ADC_PIN1 36
#define ADC_PIN2 39
#define ADC_PIN3 34
#define OUTPUT_PIN 25           // Pin GPIO pour ESP32
#define MEASURE_PIN 46          // Exemple d'entrée ADC pour ESP32


int dephasage=0;

const uint16_t valuesinus[] = {
128, 136, 143, 151, 159, 167, 174, 182,
189, 196, 202, 209, 215, 220, 226, 231,
235, 239, 243, 246, 249, 251, 253, 254,
255, 255, 255, 254, 253, 251, 249, 246,
243, 239, 235, 231, 226, 220, 215, 209,
202, 196, 189, 182, 174, 167, 159, 151,
143, 136, 128, 119, 112, 104, 96, 88,
81, 73, 66, 59, 53, 46, 40, 35,
29, 24, 20, 16, 12, 9, 6, 4,
2, 1, 0, 0, 0, 1, 2, 4,
6, 9, 12, 16, 20, 24, 29, 35,
40, 46, 53, 59, 66, 73, 81, 88,
96, 104, 112, 119};
int freq = 1000;
int i = 0;

// Timer0 Configuration Pointer (Handle)
hw_timer_t *Timer0_Cfg = NULL;


// The Timer0 ISR Function (Executes Every Timer0 Interrupt Interval)
void IRAM_ATTR Timer0_ISR()
{
  // Send SineTable Values To DAC One By One
  dac_output_voltage(DAC_CHANNEL_1, valuesinus[i]);
  if(i == 100)
  {
    i = 0;
  }
}

void setup() 
{
  // Configure Timer0 Interrupt
  Timer0_Cfg = timerBegin(0, 80, true);
  timerAttachInterrupt(Timer0_Cfg, &Timer0_ISR, true);
  timerAlarmWrite(Timer0_Cfg, 10, true);
  timerAlarmEnable(Timer0_Cfg);
  // Enable DAC1 Channel's Output
  dac_output_enable(DAC_CHANNEL_1);
  Serial.begin(115200);

}

int mesure() {
  int adcValue = analogRead(MEASURE_PIN);
  return adcValue;
}


//obtenir la température
/*
void Temperature() {
    int adc_1_0 = analogRead(A0);
    int adc_1_1 = analogRead(ADC_PIN2);
    int adc_1_2 = analogRead(A3);

    float v1 = 5 * adc_1_0 / 4095.0;
    float v2 = 5 * adc_1_1 / 4095.0;
    float v3 = 5 * adc_1_2 / 4095.0;

    float r1 = R_FIXED * v1 / (5 - v1);
    float r2 = R_FIXED * v2 / (5 - v2);
    float r3 = R_FIXED * v3 / (5 - v3);

    float tempC1 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r1/R0)) - 273.15) + 5.0;
    float tempC2 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r2/R0)) - 273.15) + 5.0;
    float tempC3 = (1.0 / (1.0/T0 + (1.0/BETA) * log(r3/R0)) - 273.15) + 5.0;

    Serial.print("Temp 1: "); Serial.print(tempC1); Serial.print(" °C, ");
    Serial.print("Temp 2: "); Serial.print(tempC2); Serial.print(" °C, ");
    Serial.print("Temp 3: "); Serial.print(tempC3); Serial.println(" °C");
    Serial.print("ADC: "); Serial.print(adc_1_0); Serial.print(", "); Serial.print(adc_1_1); Serial.print(", "); Serial.println(adc_1_2);
    
    delay(1000);
}
*/
void loop() {
    /*//Temperature();
    // Envoi de l'impulsion
    digitalWrite(DAC_CHANNEL_1, HIGH);
    int startTime = micros();
   // Attente de la réponse
    while (digitalRead(DAC_CHANNEL_1) == LOW) {
      // Boucle jusqu'à détection
    }
    
    int endTime = micros();
    digitalWrite(DAC_CHANNEL_1, LOW);

    int responseTime = endTime - startTime;


    delay(1000);*/
    //Mesure de l'impédance d'une simple resistance
    int adcValue = mesure();
    Serial.print("ADC Value: ");
    Serial.println(adcValue);
    int vraiValue = adcValue * 5 / 4095; // Convertir la valeur ADC en tension (0-5V)
    Serial.print("Tension: ");
    Serial.print(vraiValue);
    Serial.println(" V");
    int impedance = 5*10000/vraiValue - 10000; // Calculer l'impédance (en ohms)
    Serial.print("Impédance: ");
    Serial.print(impedance);
    Serial.println(" Ohms");
    delay(1000);
} 
