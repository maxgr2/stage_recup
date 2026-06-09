
#include "I2C.h"
#include <Arduino.h>
#include <assert.h>

// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

static uint8_t communication_buffer[60] = {0};



void SPS30_write(uint16_t reg, uint16_t val){
    Wire.beginTransmission(SPS30_I2C_ADDR_69);
    Wire.write((reg >> 8) & 0xFF); // MSB du registre
    Wire.write(reg & 0xFF);        // LSB du registre
    Wire.write((val >> 8) & 0xFF); // MSB de la valeur
    Wire.write(val & 0xFF);        // LSB de la valeur
    uint8_t crcData[2] = { (uint8_t)((val >> 8) & 0xFF), (uint8_t)(val & 0xFF) };
    Wire.write(CalcCrc(crcData));
    Wire.endTransmission();
}

uint8_t CalcCrc(uint8_t data[2]) {
    uint8_t crc = 0xFF;
    for(int i = 0; i < 2; i++) {
        crc ^= data[i];
        for(uint8_t bit = 8; bit > 0; --bit) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x31u;
            } else {
                crc = (crc << 1);
            }
        
    } }
 return crc;
}

bool SPS30_read(uint16_t reg, uint16_t &result) {
    //demande de la valeur du registre
    Wire.beginTransmission(SPS30_I2C_ADDR_69);
    Wire.write((reg >> 8) & 0xFF); // MSB du registre
    Wire.write(reg & 0xFF);        // LSB du registre
    Wire.endTransmission();

    //attente de la réponse du capteur
    Wire.requestFrom((uint8_t)SPS30_I2C_ADDR_69, (uint8_t)3);
    if (Wire.available() == 3) {
        uint8_t msb = Wire.read();
        uint8_t lsb = Wire.read();
        uint8_t crc_recu = Wire.read();

        // vérification des données avec le crc
        uint8_t data_pour_crc[2] = {msb, lsb};
        if (CalcCrc(data_pour_crc) != crc_recu) {
            Serial.println("Erreur de CRC : La donnée reçue est corrompue.");
            return false;
        }
        result = (msb << 8) | lsb;
        return true;
    }
    
    Serial.println("Erreur : Le capteur ne répond pas.");
    return false;
}

void SPS30_wakeup(){
    SPS30_write(SPS30_WAKE_UP_CMD_ID, 0);
}

void SPS30_sleep(){
    SPS30_write(SPS30_SLEEP_CMD_ID, 0);
}

void SPS30_startFanCleaning(){
    SPS30_write(SPS30_START_FAN_CLEANING_CMD_ID, 0);
}


/**
 * * @param format données renvoyer par le capteur :
 *  2 : uint16_t
 *  1 : float
 */
void SPS30_startMeasurement(int format){
    if (format == 1){
        SPS30_write(SPS30_START_MEASUREMENT_CMD_ID, 0x0300);
    } else if (format == 2){
        SPS30_write(SPS30_START_MEASUREMENT_CMD_ID, 0x0500);
    } else {
        Serial.println("Format de mesure invalide. Utilisez 1 pour uint16 ou 2 pour float.");
    }
}

bool SPS30_readDataReadyFlag(){
    uint16_t dataReadyFlag;
    SPS30_read(SPS30_READ_DATA_READY_FLAG_CMD_ID, dataReadyFlag);
    if (dataReadyFlag == 0x0000) {
        Serial.println("Aucune nouvelle mesure disponible.");
        return false;
    } else if (dataReadyFlag == 0x0001) {
        Serial.println("Nouvelle mesure prête à être lue.");
        return true;
    } else {
        Serial.println("Valeur du Data-Ready Flag inconnue : " + String(dataReadyFlag));
        return false;
    }
}

float bytesToFloat(uint8_t msb1, uint8_t lsb1, uint8_t crc1, uint8_t msb2, uint8_t lsb2, uint8_t crc2) {
    uint32_t combined = (msb1 << 24) | (lsb1 << 16) | (msb2 << 8) | lsb2;
    float result;
    memcpy(&result, &combined, sizeof(float));
    uint8_t dataForCrc[2] = { msb1, lsb1 };
    if (CalcCrc(dataForCrc) == crc1) {
        dataForCrc[0] = msb2;
        dataForCrc[1] = lsb2;
        if (CalcCrc(dataForCrc) == crc2) {
            return result;
        }
    }
    Serial.println("Erreur de CRC");
    return 0.0f; // Retourne 0 en cas d'erreur de CRC
}
/**
 * * @brief retourne la mesure effectuer par le capteur compteur de particule
 **/
Capteur_PM_float SPS30_mesure(){
    if (!SPS30_readDataReadyFlag()) {
        Serial.println("Aucune nouvelle mesure disponible. Veuillez réessayer plus tard.");
        return Capteur_PM_float(); // Retourne une structure vide
    } else {
        Capteur_PM_float mesure;
        uint8_t rawData[60];

        //Pour faire la lecture des données, on envoie une requete de lecture au capteur et on lit les 60 octets de données qui sont renvoyés
        Wire.beginTransmission(SPS30_I2C_ADDR_69);
        Wire.write((SPS30_READ_VALUES_CMD_ID >> 8) & 0xFF);
        Wire.write(SPS30_READ_VALUES_CMD_ID & 0xFF);
        Wire.endTransmission(false); // repeated start
        Wire.requestFrom((uint8_t)SPS30_I2C_ADDR_69, (uint8_t)60);

        if (Wire.available() < 60) {
            Serial.println("Erreur : réponse incomplète.");
            return Capteur_PM_float();
        }
        for (int i = 0; i < 60; i++) {
            rawData[i] = Wire.read();
        }
        //traitement des données
       // Dans rawData j'ai 2 donnée intéressante 1 CRC et chaque donnée et sur 2 fois
       mesure.mc1p0 =bytesToFloat(rawData[0], rawData[1], rawData[2], rawData[3], rawData[4], rawData[5]);
       mesure.mc2p5 = bytesToFloat(rawData[6], rawData[7], rawData[8], rawData[9], rawData[10], rawData[11]);
       mesure.mc4p0 = bytesToFloat(rawData[12], rawData[13], rawData[14], rawData[15], rawData[16], rawData[17]);
       mesure.mc10p0 = bytesToFloat(rawData[18], rawData[19], rawData[20], rawData[21], rawData[22], rawData[23]);
       mesure.nc0p5 = bytesToFloat(rawData[24], rawData[25], rawData[26], rawData[27], rawData[28], rawData[29]);
       mesure.nc1p0 = bytesToFloat(rawData[30], rawData[31], rawData[32], rawData[33], rawData[34], rawData[35]);
       mesure.nc2p5 = bytesToFloat(rawData[36], rawData[37], rawData[38], rawData[39], rawData[40], rawData[41]);
       mesure.nc4p0 = bytesToFloat(rawData[42], rawData[43], rawData[44], rawData[45], rawData[46], rawData[47]);
       mesure.nc10p0 = bytesToFloat(rawData[48], rawData[49], rawData[50], rawData[51], rawData[52], rawData[53]);
       mesure.typicalParticleSize = bytesToFloat(rawData[54], rawData[55], rawData[56], rawData[57], rawData[58], rawData[59]);

        return mesure;
    }
}

void SPS30_stopMeasurement(){
    SPS30_write(SPS30_STOP_MEASUREMENT_CMD_ID, 0);
}

// A faire la fonction de nettoyage du capteur de manière automatique

