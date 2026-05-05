#include <Arduino.h>


struct INA237_Mesures {
  float tensionBus_V;
  float courant_A;
  float puissance_W;
  float tensionShunt_mV;
  float temperature_C;
};


// ===== I2C.cpp - Prototypes =====

// MCP23017 I/O Expander functions
void mcpWrite(uint8_t reg, uint8_t val);
uint8_t mcpRead(uint8_t reg);

// SSR (Solid State Relay) control functions
void ssrOn(uint8_t ssrIndex);
void ssrOff(uint8_t ssrIndex);
void ssrToggle(uint8_t ssrIndex);
void ssrAllOff();
void ssrSetAll(uint8_t maskA, uint8_t maskB);

// INA237 Current/Voltage Sensor functions
void inaWrite16(uint8_t reg, uint16_t val);
int16_t inaRead16(uint8_t reg);
float inaLireTensionBus();
float inaLireTensionShunt();
float inaLireCourant();
float inaLireTemperature();
