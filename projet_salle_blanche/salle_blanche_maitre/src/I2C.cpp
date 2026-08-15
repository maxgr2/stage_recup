#include "I2C.h"
#include <Arduino.h>



// --- Constructeur du SPS30 ---
SPS30::SPS30(uint8_t address) { // On peut ignorer l'adresse car elle est fixe pour ce capteur
}

void SPS30::write(uint16_t reg, uint16_t val){
    Wire.beginTransmission(SPS30_I2C_ADDR_69);
    Wire.write((reg >> 8) & 0xFF); // MSB du registre
    Wire.write(reg & 0xFF);        // LSB du registre
    Wire.write((val >> 8) & 0xFF); // MSB de la valeur
    Wire.write(val & 0xFF);        // LSB de la valeur
    uint8_t crcData[2] = { (uint8_t)((val >> 8) & 0xFF), (uint8_t)(val & 0xFF) };
    Wire.write(calcCrc(crcData));
    Wire.endTransmission();
}

uint8_t SPS30::calcCrc(uint8_t data[2]) {
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

bool SPS30::read(uint16_t reg, uint16_t &result) {
    //demande de la valeur du registre
    Wire.beginTransmission(SPS30_I2C_ADDR_69);
    Wire.write((reg >> 8) & 0xFF); // MSB du registre
    Wire.write(reg & 0xFF);        // LSB du registre
    Wire.endTransmission();
    delay(1);
    //attente de la réponse du capteur
    Wire.requestFrom((uint8_t)SPS30_I2C_ADDR_69, (uint8_t)3);
    if (Wire.available() == 3) {
        uint8_t msb = Wire.read();
        uint8_t lsb = Wire.read();
        uint8_t crc_recu = Wire.read();
        

        // vérification des données avec le crc
        uint8_t data_pour_crc[2] = {msb, lsb};
        if (calcCrc(data_pour_crc) != crc_recu) {
            Serial.println("Erreur de CRC : La donnée reçue est corrompue.");
            return false;
        }
        result = (msb << 8) | lsb;
        return true;
    }
    
    Serial.println("Erreur : Le capteur ne répond pas.");
    return false;
}

void SPS30::wakeup() {
    // Envoi spécial : pas de valeur ni de CRC
    Wire.beginTransmission(SPS30_I2C_ADDR_69);
    Wire.write((SPS30_WAKE_UP_CMD_ID >> 8) & 0xFF);
    Wire.write(SPS30_WAKE_UP_CMD_ID & 0xFF);
    Wire.endTransmission();
    delay(50); // Le SPS30 a besoin de 50ms pour se réveiller

    Wire.beginTransmission(SPS30_I2C_ADDR_69);
    Wire.write((SPS30_WAKE_UP_CMD_ID >> 8) & 0xFF);
    Wire.write(SPS30_WAKE_UP_CMD_ID & 0xFF);
    Wire.endTransmission();
    delay(50); //
}

void SPS30::sleep(){
    this->write(SPS30_SLEEP_CMD_ID, 0);
}

void SPS30::startFanCleaning(){
    this->write(SPS30_START_FAN_CLEANING_CMD_ID, 0);
}


/**
 * * @param format données renvoyer par le capteur :
 *  2 : uint16_t
 *  1 : float
 */
void SPS30::startMeasurement(int format){
    if (format == 1){
        this->write(SPS30_START_MEASUREMENT_CMD_ID, 0x0300);
    } else if (format == 2){
        this->write(SPS30_START_MEASUREMENT_CMD_ID, 0x0500);
    } else {
        Serial.println("Format de mesure invalide. Utilisez 1 pour uint16 ou 2 pour float.");
    }
}

bool SPS30::readDataReadyFlag(){
    uint16_t dataReadyFlag;
    this->read(SPS30_READ_DATA_READY_FLAG_CMD_ID, dataReadyFlag);
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

float SPS30::bytesToFloat(uint8_t msb1, uint8_t lsb1, uint8_t crc1, uint8_t msb2, uint8_t lsb2, uint8_t crc2) {
    uint32_t combined = (msb1 << 24) | (lsb1 << 16) | (msb2 << 8) | lsb2;
    float result;
    memcpy(&result, &combined, sizeof(float));
    uint8_t dataForCrc[2] = { msb1, lsb1 };
    uint8_t calccrc1=this->calcCrc(dataForCrc);
    u_int8_t calccrc2;
    if (calccrc1 == crc1) {
        dataForCrc[0] = msb2;
        dataForCrc[1] = lsb2;
        calccrc2=this->calcCrc(dataForCrc);
        if ( calccrc2== crc2) {
            return result;
        }
    }
     Serial.printf("FLOAT RAW: 0x%02X 0x%02X CRC1_recu=0x%02X CRC1_calc=0x%02X | "
                  "0x%02X 0x%02X CRC2_recu=0x%02X CRC2_calc=0x%02X\n",
                  msb1, lsb1, crc1, calccrc1, msb2, lsb2, crc2, calccrc2);
    Serial.println("Erreur de CRC");
    return 0.0f; // Retourne 0 en cas d'erreur de CRC
}
/**
 * * @brief retourne la mesure effectuer par le capteur compteur de particule
 **/
Capteur_PM_float SPS30::mesure(){
    if (!this->readDataReadyFlag()) {
        Serial.println("Aucune nouvelle mesure disponible. Veuillez réessayer plus tard.");
        return Capteur_PM_float(); // Retourne une structure vide
    } else {
        Capteur_PM_float mesure;
        uint8_t rawData[60];

        //Pour faire la lecture des données, on envoie une requete de lecture au capteur et on lit les 60 octets de données qui sont renvoyés
        Wire.beginTransmission(SPS30_I2C_ADDR_69);
        Wire.write((SPS30_READ_VALUES_CMD_ID >> 8) & 0xFF);
        Wire.write(SPS30_READ_VALUES_CMD_ID & 0xFF);
        Wire.endTransmission();
        delay(5);
        uint8_t received = Wire.requestFrom((uint8_t)SPS30_I2C_ADDR_69, (uint8_t)60);
        Serial.printf("Octets reçus : %d\n", received);
        if (Wire.available() < 60) {
            Serial.println("Erreur : réponse incomplète.");
            return Capteur_PM_float();
        }
        for (int i = 0; i < 60; i++) {
            rawData[i] = Wire.read();
        }
        //traitement des données
       // Dans rawData j'ai 2 donnée intéressante 1 CRC et chaque donnée et sur 2 fois
       mesure.mc1p0 =this->bytesToFloat(rawData[0], rawData[1], rawData[2], rawData[3], rawData[4], rawData[5]);
       mesure.mc2p5 = this->bytesToFloat(rawData[6], rawData[7], rawData[8], rawData[9], rawData[10], rawData[11]);
       mesure.mc4p0 = this->bytesToFloat(rawData[12], rawData[13], rawData[14], rawData[15], rawData[16], rawData[17]);
       mesure.mc10p0 = this->bytesToFloat(rawData[18], rawData[19], rawData[20], rawData[21], rawData[22], rawData[23]);
       mesure.nc0p5 = this->bytesToFloat(rawData[24], rawData[25], rawData[26], rawData[27], rawData[28], rawData[29]);
       mesure.nc1p0 = this->bytesToFloat(rawData[30], rawData[31], rawData[32], rawData[33], rawData[34], rawData[35]);
       mesure.nc2p5 = this->bytesToFloat(rawData[36], rawData[37], rawData[38], rawData[39], rawData[40], rawData[41]);
       mesure.nc4p0 = this->bytesToFloat(rawData[42], rawData[43], rawData[44], rawData[45], rawData[46], rawData[47]);
       mesure.nc10p0 = this->bytesToFloat(rawData[48], rawData[49], rawData[50], rawData[51], rawData[52], rawData[53]);
       mesure.typicalParticleSize = this->bytesToFloat(rawData[54], rawData[55], rawData[56], rawData[57], rawData[58], rawData[59]);

        return mesure;
    }
}

void SPS30::stopMeasurement(){
    this->write(SPS30_STOP_MEASUREMENT_CMD_ID, 0);
}

bool SPS30::setAutoCleaningInterval(uint16_t hours) {
    // La commande attend la valeur en heures sur 16 bits
    this->write(SPS30_AUTO_CLEANING_INTERVAL_CMD_ID, hours);
    
    
    Serial.print("Intervalle de nettoyage automatique programmé à ");
    Serial.print(hours);
    Serial.println(" heures.");
    
    return true;
}

SHT40::SHT40(uint8_t address) {
    _i2cAddress = address; // On sauvegarde l'adresse I2C passée en paramètre 
}


uint8_t SHT40::calculateCRC(uint8_t data[], uint8_t length) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

SHT40_Data SHT40::readMeasurement() {
    SHT40_Data result;
    result.isValid = false;
    result.temperature = 0.0;
    result.humidity = 0.0;

    uint8_t buffer[6];
    if (this->_read_data(SHT40_CMD_MEASURE_HIGH_PRECISION, buffer, 6)) {
        uint16_t rawTemp = ((uint16_t)buffer[0] << 8) | buffer[1];
        uint16_t rawHum = ((uint16_t)buffer[3] << 8) | buffer[4];

        result.temperature = _convertTemperature(rawTemp);
        result.humidity = _convertHumidity(rawHum);
        result.isValid = true;
    } else {
        Serial.println("Erreur lecture SHT40 (CRC ou I2C)");
    }

    return result;
}
float SHT40::_convertTemperature(uint16_t raw) {
    return -45.0 + 175.0 * ((float)raw / 65535.0);
}

float SHT40::_convertHumidity(uint16_t raw) {
    float rh = -6.0 + 125.0 * ((float)raw / 65535.0);
    //  C'est un pourcentage
    if (rh < 0) rh = 0;
    if (rh > 100) rh = 100;
    return rh;
}

bool SHT40::_read_data(uint8_t command, uint8_t* buffer, size_t length) {
    // Envoi de la commande
    Wire.beginTransmission(SHT40_I2C_ADDR);
    Wire.write(command);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    // Délai d'attente selon la précision (max 10ms pour haute précision)
    // On utilise un délai générique sûr pour toutes les mesures sans chauffage
    delay(10); 

    // Lecture des données (2 octets de données + 1 octet CRC par valeur)
    // Pour Temp + Hum, on attend 6 octets
    Wire.requestFrom((uint8_t)SHT40_I2C_ADDR, (uint8_t)length);
    
    if (Wire.available() < length) {
        return false;
    }

    for (int i = 0; i < length; i++) {
        buffer[i] = Wire.read();
    }

    // Vérification CRC pour la Température (octets 0, 1, 2)
    uint8_t crcTemp = calculateCRC(buffer, 2);
    if (crcTemp != buffer[2]) {
        return false;
    }

    // Vérification CRC pour l'Humidité (octets 3, 4, 5)
    uint8_t crcHum = calculateCRC(&buffer[3], 2);
    if (crcHum != buffer[5]) {
        return false;
    }

    return true;
}


SHT30::SHT30(uint8_t address) : _i2cAddress(address) {}

// --- Méthodes Publiques ---

SHT30_Data SHT30::readMeasurement() {
    SHT30_Data data = {0.0f, 0.0f, false};
    
    // Commande de mesure: Haute précision, Clock stretching désactivé (0x2400)
    if (!_sendCommand(0x2400)) {
        return data; 
    }
    
    // Le SHT30 requiert jusqu'à 15ms pour effectuer une conversion haute précision
    delay(15);
    
    uint8_t buffer[6];
    Wire.requestFrom(_i2cAddress, (uint8_t)6);
    
    if (Wire.available() != 6) {
        return data; // Problème de communication
    }
    
    for (int i = 0; i < 6; i++) {
        buffer[i] = Wire.read();
    }
    
    // Vérification de l'intégrité des données via CRC
    uint8_t tempRawArr[2] = {buffer[0], buffer[1]};
    uint8_t humRawArr[2] = {buffer[3], buffer[4]};
    
    if (_calculateCRC(tempRawArr, 2) != buffer[2] || 
        _calculateCRC(humRawArr, 2) != buffer[5]) {
        return data; // Le CRC ne correspond pas
    }
    
    // Reconstitution des données brutes sur 16 bits
    uint16_t rawTemp = (buffer[0] << 8) | buffer[1];
    uint16_t rawHum  = (buffer[3] << 8) | buffer[4];
    
    // Conversion finale
    data.temperature = _convertTemperature(rawTemp);
    data.humidity = _convertHumidity(rawHum);
    data.valid = true;
    
    return data;
}

void SHT30::softReset() {
    // Commande Soft Reset (0x30A2)
    _sendCommand(0x30A2);
    delay(2); // Le capteur a besoin d'environ 1.5ms pour redémarrer
}

bool SHT30::readSerialNumber(char* buffer, size_t bufferSize) {
    // Commande de lecture du Serial Number (0x3780)
    if (!_sendCommand(0x3780)) return false;
    
    
    Wire.requestFrom(_i2cAddress, (uint8_t)6);
    if (Wire.available() != 6) return false;
    
    uint8_t data[6];
    for (int i = 0; i < 6; i++) {
        data[i] = Wire.read();
    }
    
    // Optionnel mais robuste : vérifier le CRC du SN
    uint8_t snPart1[2] = {data[0], data[1]};
    uint8_t snPart2[2] = {data[3], data[4]};
    if (_calculateCRC(snPart1, 2) != data[2] || _calculateCRC(snPart2, 2) != data[5]) {
        return false;
    }
    
    // Le numéro de série du SHT30 est stocké sur 32 bits
    uint32_t serialNumber = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | 
                            ((uint32_t)data[3] << 8)  | data[4];
                            
    snprintf(buffer, bufferSize, "%lu", (unsigned long)serialNumber);
    return true;
}

bool SHT30::_sendCommand(uint16_t command) {
    Wire.beginTransmission(_i2cAddress);
    // Découpage de la commande 16 bits en 2 octets
    Wire.write(command >> 8);   // MSB
    Wire.write(command & 0xFF); // LSB
    return (Wire.endTransmission() == 0);
}

float SHT30::_convertTemperature(uint16_t raw) {
    // Formule SHT30 : T = -45 + 175 * (S_T / (2^16 - 1))
    return -45.0f + 175.0f * ((float)raw / 65535.0f);
}

float SHT30::_convertHumidity(uint16_t raw) {
    // Formule SHT30 : RH = 100 * (S_RH / (2^16 - 1))
    return 100.0f * ((float)raw / 65535.0f);
}

uint8_t SHT30::_calculateCRC(uint8_t data[], uint8_t length) {
    uint8_t crc = 0xFF; // Valeur initiale
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31; // Polynôme P(x) = x^8 + x^5 + x^4 + 1
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

 IPS7100:: IPS7100(uint8_t address) {
    _i2cAddress = address; // On sauvegarde l'adresse I2C passée en paramètre
}
// Fonction CRC officielle fournie par le fabricant 
uint16_t IPS7100::calcCrc(uint8_t *byte, int len) {
    int i, j;
    uint16_t data = 0;
    uint16_t crc = 0xffff;
    for (j = 0; j < len; j++) {
        data = (uint16_t)0xff & byte[j];
        for (i = 0; i < 8; i++, data >>= 1) {
            if ((crc & 0x0001) ^ (data & 0x0001))
                crc = (crc >> 1) ^ 0x8408;
            else
                crc >>= 1;
        }
    }
    crc = ~crc;
    data = crc;
    crc = (crc << 8) | (data >> 8 & 0xff);
    return crc; // retourne bien 16 bits
}

void IPS7100::write(uint16_t reg, uint16_t val) {
    Wire.beginTransmission(_i2cAddress);
    Wire.write((uint8_t)reg);              // Commande (ex: 0x10)
    Wire.write((uint8_t)(val & 0xFF));     // Paramètre (1 seul octet, pas 2)
    Wire.endTransmission();
}

// Lecture de blocs de données via I2C (Format de lecture standard) 
bool IPS7100::readBlock(uint8_t reg, uint8_t *buffer, uint8_t length) {
    const uint8_t maxTentatives = 4;

    for (uint8_t tentative = 0; tentative < maxTentatives; tentative++) {
        Wire.beginTransmission(_i2cAddress);
        Wire.write(reg);
        if (Wire.endTransmission() != 0) { // STOP complet
            return false;
        }

        if (Wire.requestFrom((int)_i2cAddress, (int)length) != length) {
            Serial.println("Erreur I2C : nombre d'octets reçus incorrect.");
            continue; // on retente
        }

        for (uint8_t i = 0; i < length; i++) {
            buffer[i] = Wire.read();
        }

        // Les 2 derniers octets du bloc sont le CRC16 (big endian)
        uint16_t crcAttendu = calcCrc(buffer, length - 2);
        uint16_t crcRecu = ((uint16_t)buffer[length - 2] << 8) | buffer[length - 1];

        if (crcAttendu == crcRecu) {
            return true; // données valides
        }

        Serial.println("Erreur de CRC IPS7100 : donnée corrompue, nouvelle tentative...");
        delay(10);
    }

    Serial.println("Erreur : échec de lecture IPS7100 après plusieurs tentatives (CRC).");
    return false;
}
// Conversion de 4 octets (IEEE-754 Little Endian: DCBA -> Réel) 
float  IPS7100::bytesToFloat(uint8_t msb1, uint8_t lsb1, uint8_t msb2, uint8_t lsb2) {
    // Ordre Little Endian inversé par le capteur (DCBA) 
    uint32_t combined = ((uint32_t)lsb2 << 24) | ((uint32_t)msb2 << 16) | ((uint32_t)lsb1 << 8) | msb1;
    float value;
    memcpy(&value, &combined, sizeof(float));
    return value;
}

// Démarrer la mesure (Commande 0x10, paramètre 3 pour 1000ms ou 1 pour 200ms) 
void  IPS7100::startMeasurement() {
    this->write(0x24, 0x0002); // Démarrage de la mesure

    delay(10); // Petit délai pour s'assurer que le capteur est prêt
    write(0x23, 0x0000); // Démarre avec un intervalle de 200ms
    write(0x10, 0x0003); // Démarre avec un intervalle de 1000ms
}

// Arrêter la mesure (Commande 0x23, paramètre 0x0001) Dans les fait on entre en power saving mode
void  IPS7100::stopMeasurement() {
    write(0x23, 0x0001);
}

// Le capteur  IPS utilise une méthode de scrutation (polling), il n'y a pas de pin "Data Ready" dédiée en I2C 
bool  IPS7100::readDataReadyFlag() {
    // On peut interroger le statut via la commande 0x6a (Read Status) 
    uint8_t buffer[3];
    if (readBlock(0x6A, buffer, 3)) {
        // Retourne true si le capteur est actif
        return true; 
    }
    return false;
}

// Fonction utilitaire pour décoder les 4 octets non signés du Particle Count (PC)
uint32_t bytesToUint32(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    return ((uint32_t)b1 << 24) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 8) | b4;
}

Capteur_PM_IPS_float  IPS7100::mesure() {
    Capteur_PM_IPS_float resultat = {0}; // Initialisation sécurisée de toutes les variables à 0.0f

    uint8_t bufferPM[32];
    if (readBlock(0x12, bufferPM, 32)) { // Demande de 32 octets pour la série 7 
        // Le bin 0.3 µm (index 4 à 7) est volontairement ignoré.
        resultat.mc0p1 = bytesToFloat(bufferPM[0], bufferPM[1], bufferPM[2], bufferPM[3]); // Bin 0.1 µm
        resultat.mc0p3 = bytesToFloat(bufferPM[4], bufferPM[5], bufferPM[6], bufferPM[7]); // Bin 0.3 µm
        resultat.mc0p5 = bytesToFloat(bufferPM[8], bufferPM[9], bufferPM[10], bufferPM[11]); // Bin 0.5 µm
        resultat.mc1p0  = bytesToFloat(bufferPM[12], bufferPM[13], bufferPM[14], bufferPM[15]); // Bin 1.0 µm
        resultat.mc2p5  = bytesToFloat(bufferPM[16], bufferPM[17], bufferPM[18], bufferPM[19]); // Bin 2.5 µm
        resultat.mc5p0  = bytesToFloat(bufferPM[20], bufferPM[21], bufferPM[22], bufferPM[23])+bytesToFloat(bufferPM[24], bufferPM[25], bufferPM[26], bufferPM[27]); // Bin 10.0 µm
        //On s'en fiche de 10µm
    }

    uint8_t bufferPC[30];

    if (readBlock(0x11, bufferPC, 30)) { // Demande de 30 octets (28 de données + 2 de CRC) 
        // Les comptages sont de simples entiers non signés sur 4 octets 
        // Le bin 0.3 µm (index 4 à 7) est volontairement ignoré.
        resultat.nc0p1  = (float)bytesToUint32(bufferPC[0],  bufferPC[1],  bufferPC[2],  bufferPC[3]);  // Bin 0.1 µm
        resultat.nc0p3  = (float)bytesToUint32(bufferPC[4],  bufferPC[5],  bufferPC[6],  bufferPC[7]);  // Bin 0.3 µm
        resultat.nc0p5  = (float)bytesToUint32(bufferPC[8],  bufferPC[9],  bufferPC[10], bufferPC[11]); // Bin 0.5 µm
        resultat.nc1p0  = (float)bytesToUint32(bufferPC[12], bufferPC[13], bufferPC[14], bufferPC[15]); // Bin 1.0 µm
        resultat.nc2p5  = (float)bytesToUint32(bufferPC[16], bufferPC[17], bufferPC[18], bufferPC[19]); // Bin 2.5 µm
        resultat.nc5p0  = (float)bytesToUint32(bufferPC[20], bufferPC[21], bufferPC[22], bufferPC[23])+(float)bytesToUint32(bufferPC[24], bufferPC[25], bufferPC[26], bufferPC[27]); // Bin 5.0 µm
    }

    return resultat;
}