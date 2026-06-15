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
// Registres INA237
#define INA237_REG_CONFIG   0x00
#define INA237_REG_SHUNT    0x04  // Tension shunt
#define INA237_REG_VBUS     0x05  // Tension bus
#define INA237_REG_TEMP     0x06
#define INA237_REG_CURRENT  0x07  // Courant (après calibration)
#define INA237_REG_POWER    0x08
#define INA237_REG_SHUNT_CAL 0x02 // Registre de calibration

// LSB sizes INA237
#define VBUS_LSB     0.003125f   // 3.125 mV/bit
#define VSHUNT_LSB   0.000005f   // 5 µV/bit (gain x1, ADCRANGE=0)
#define TEMP_LSB     0.125f      // 0.125 °C/bit

// Registres
#define OLATA 0x14
#define OLATB 0x15


void mcpInit() {
  // Mettre tous les pins en SORTIE (0 = output)
  mcpWrite(0x00, 0x00);
  mcpWrite(0x01, 0x00);
  // Éteindre tous les SSR au démarrage
  mcpWrite(OLATA, 0x00);
  mcpWrite(OLATB, 0x00);
}

void mcpInit_fille(uint8_t adresse_i2c) {
  mcpWrite_fille(0x00, 0x00, adresse_i2c); // IODIRA
  mcpWrite_fille(0x01, 0x00, adresse_i2c); // IODIRB
  
  mcpWrite_fille(OLATA, 0x00, adresse_i2c);
  mcpWrite_fille(OLATB, 0x00, adresse_i2c);
}


void mcpWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void mcpWrite_fille(uint8_t reg, uint8_t val, int adress){
  Wire.beginTransmission(adress);
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

uint8_t mcpRead_fille(uint8_t reg, int adress) {
  Wire.beginTransmission(adress);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(adress, 1);
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

void ssrOn_fille(uint8_t ssrIndex, uint8_t adresse_i2c) {
  if (ssrIndex < 8) {
    uint8_t etat = mcpRead_fille(OLATA, adresse_i2c);
    mcpWrite_fille(OLATA, etat | (1 << ssrIndex), adresse_i2c);
  } else {
    uint8_t etat = mcpRead_fille(OLATB, adresse_i2c);
    mcpWrite_fille(OLATB, etat | (1 << (ssrIndex - 8)), adresse_i2c);
  }
}


void ssrOff_fille(uint8_t ssrIndex, uint8_t adresse_i2c) {
  if (ssrIndex < 8) {
    uint8_t etat = mcpRead_fille(OLATA, adresse_i2c);
    mcpWrite_fille(OLATA, etat & ~(1 << ssrIndex), adresse_i2c);
  } else {
    uint8_t etat = mcpRead_fille(OLATB, adresse_i2c);
    mcpWrite_fille(OLATB, etat & ~(1 << (ssrIndex - 8)), adresse_i2c);
  }
}

void ssrAllOff_fille(uint8_t* adresse_i2c,int nb_cartefille) {
  while (nb_cartefille>0){
    mcpWrite_fille(OLATA, 0x00, adresse_i2c[nb_cartefille]);
    mcpWrite_fille(OLATB, 0x00, adresse_i2c[nb_cartefille]);
    nb_cartefille--;
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


void alimentation_off(int bat) {
  if (bat == 1) {
    ssrOff(7); // SSR1 pour batterie 1
  } else if (bat == 2) {
    ssrOff(4); // SSR9 pour batterie 2
  } else if (bat == 3) {
    ssrOff(2); // SSR9 pour batterie 2
  }else if (bat == 4) {
    ssrOff(0); // SSR9 pour batterie 2
  }
  else {
    Serial.println("Batterie inconnue, impossible de couper l'alimentation");
  }
}
DonneesCapteur mesures(int bat){
  ssrOff(6);
  ssrOff(5);
  ssrOff(3);
  ssrOff(1);
  if (bat==1){
    ssrOn(6);
  }
  if (bat==2){
    ssrOn(5);
  }
   if (bat==3){
    ssrOn(3);
  }
   if (bat==4){
    ssrOn(1);
  }
  delay(1000);

  DonneesCapteur m = inaLire_1_Batterie();
  return m;
}

void alimentation_on(int bat) {
  if (bat == 1) {
    ssrOn(7); // SSR1 pour batterie 1
  } else if (bat == 2) {
    ssrOn(4); // SSR9 pour batterie 2
  } else if (bat == 3) {
    ssrOn(2); // SSR9 pour batterie 2
  }else if (bat == 4) {
    ssrOn(0); // SSR9 pour batterie 2
  }
  else {
    Serial.println("Batterie inconnue, impossible d'allumer l'alimentation");
  }
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

void inaInit() {
  // Calibration : SHUNT_CAL = 819.2 * 10^6 * current_lsb * R_shunt
  // Avec current_lsb = 0.150/32768 et R_shunt = ta valeur en ohms (ex: 0.01Ω)
  float current_lsb = 0.150f / 32768.0f;
  float r_shunt = 0.01f; // À adapter selon ton shunt
  uint16_t shunt_cal = (uint16_t)(819.2e6f * current_lsb * r_shunt);
  inaWrite16(INA237_REG_SHUNT_CAL, shunt_cal);
}

float inaLireTensionBus() {
  int16_t raw = inaRead16(INA237_REG_VBUS);
  return raw * VBUS_LSB; // Résultat en Volts
}

float inaLireTensionShunt() {
  int16_t raw = inaRead16(INA237_REG_SHUNT);
  return raw * VSHUNT_LSB * 1000.0f; // Résultat en mV
}

float inaLireCourant() {
  // Le registre CURRENT (0x07) est plus fiable que recalculer depuis SHUNT
  float current_lsb = 0.150f / 32768.0f;
  int16_t raw = inaRead16(INA237_REG_CURRENT);
  return raw * current_lsb; // Résultat en Ampères
}

float inaLireTemperature() {
  int16_t raw = inaRead16(INA237_REG_TEMP);
  raw >>= 4; // 4 bits LSB inutilisés
  return raw * TEMP_LSB; // Résultat en °C
}
DonneesCapteur inaLire_1_Batterie() {
  
  DonneesCapteur m;
  m.tensionBus_V    = inaLireTensionBus();
  ssrOn(8);
  delay(1000);
  m.courant_A       = inaLireCourant();
  m.tensionShunt_mV = inaLireTensionShunt();
  m.temperature_C   = inaLireTemperature();
  m.tensionBus_charge_V = inaLireTensionBus();
  m.puissance_W     = m.tensionBus_V * m.courant_A;
  m.temperaturebatterie_C = 0; // Cette valeur devra être remplie par le capteur de température
  return m;
}