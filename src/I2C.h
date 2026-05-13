#pragma once
#include <Arduino.h>


struct DonneesCapteur {
  float tensionBus_V;
  float courant_A;
  float puissance_W;
  float tensionShunt_mV;
  float temperature_C;
  float temperaturebatterie_C;//cette donnée doit être rempli plus tard par le capteur de temperature
};


// MCP23017 I/O Expander functions
void mcpWrite(uint8_t reg, uint8_t val);
uint8_t mcpRead(uint8_t reg);

// SSR (Solid State Relay) control functions
void ssrOn(uint8_t ssrIndex);
void ssrOff(uint8_t ssrIndex);
void ssrToggle(uint8_t ssrIndex);
void ssrAllOff();
void ssrSetAll(uint8_t maskA, uint8_t maskB);
void alimentation_off(int bat);
void alimentation_on(int bat);
DonneesCapteur mesures(int bat);

// INA237 Current/Voltage Sensor functions
void inaWrite16(uint8_t reg, uint16_t val);
int16_t inaRead16(uint8_t reg);
float inaLireTensionBus();
float inaLireTensionShunt();
float inaLireCourant();
float inaLireTemperature();
DonneesCapteur inaLire_1_Batterie();
