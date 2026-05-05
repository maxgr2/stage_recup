#include <Arduino.h>

/*
Ce fichier a pour but de faire toute la gestion de l'interface I2C 
dans notre cas ils commande donc l'expadeur et donc les SSR
Et le module INA237
*/
#include "I2C.h"
#include <Wire.h>

#define MCP_ADDR 0x20

#define INA237_ADDR 0b1000100

// Registres
#define OLATA 0x14
#define OLATB 0x15



void mcpWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t mcpRead(uint8_t reg) {
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(MCP_ADDR, 1);
  return Wire.read();
}

void ssrOn(uint8_t ssrIndex) {
  if (ssrIndex < 8) {
    uint8_t etat = mcpRead(OLATA);
    mcpWrite(OLATA, etat | (1 << ssrIndex));
  } else {
    uint8_t etat = mcpRead(OLATB);
    mcpWrite(OLATB, etat | (1 << (ssrIndex - 8)));
  }
}

void ssrOff(uint8_t ssrIndex) {
  if (ssrIndex < 8) {
    uint8_t etat = mcpRead(OLATA);
    mcpWrite(OLATA, etat & ~(1 << ssrIndex));
  } else {
    uint8_t etat = mcpRead(OLATB);
    mcpWrite(OLATB, etat & ~(1 << (ssrIndex - 8)));
  }
}

void ssrToggle(uint8_t ssrIndex) {
  if (ssrIndex < 8) {
    uint8_t etat = mcpRead(OLATA);
    mcpWrite(OLATA, etat ^ (1 << ssrIndex));
  } else {
    uint8_t etat = mcpRead(OLATB);
    mcpWrite(OLATB, etat ^ (1 << (ssrIndex - 8)));
  }
}

// Éteint tous les SSR d'un coup (sécurité)
void ssrAllOff() {
  mcpWrite(OLATA, 0x00);
  mcpWrite(OLATB, 0x00);
}


void ssrSetAll(uint8_t maskA, uint8_t maskB) {
  mcpWrite(OLATA, maskA);
  mcpWrite(OLATB, maskB);
}


void inaWrite16(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(INA237_ADDR);
  Wire.write(reg);
  Wire.write((val >> 8) & 0xFF);  // MSB en premier INA veux un ACK (acknowledge sequence) tous les 8 bits
  Wire.write(val & 0xFF); //Pour limiter à 16 bits
  Wire.endTransmission();
}

int16_t inaRead16(uint8_t reg) {
  Wire.beginTransmission(INA237_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)INA237_ADDR, (uint8_t)2);
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  return (int16_t)((msb << 8) | lsb);
}

float inaLireTensionBus() {
  int16_t raw = inaRead16(0x05);
  return raw ; 
}

float inaLireTensionShunt() {
  int16_t raw = inaRead16(0x04);
  return raw ;
}

float inaLireCourant() {
  float current_lsb = 0.150 / 32768.0; // 0.150 A (maximum du courant) / 2^15 (résolution du convertisseur)
  int16_t raw = inaRead16(0x04);
  return raw * current_lsb;
}

float inaLireTemperature() {
  int16_t raw = inaRead16(0x06);
  raw >>= 4;  // Les 4 bits LSB sont inutilisés
  return raw * 0.125;
}


INA237_Mesures inaLireTout() {
  INA237_Mesures m;
  m.tensionBus_V    = inaLireTensionBus();
  m.courant_A       = inaLireCourant();
  m.tensionShunt_mV = inaLireTensionShunt();
  m.temperature_C   = inaLireTemperature();
  return m;
}