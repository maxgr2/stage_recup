#include <Wire.h>
// ---------------------------------------------------------
/**
 * @file I2C.h
 * @brief Interface de communication I2C pour les capteurs SHT40 et SPS30
 * @details
 * Ce fichier définit les classes et fonctions nécessaires pour communiquer avec les capteurs de température, d'humidité et de particules via le bus I2C. Il inclut des méthodes pour démarrer les mesures, lire les données, et gérer les erreurs de communication.
 * Ce code est conçu pour être utilisé dans un projet ESP32 qui collecte des données environnementales et les envoie via ESP-NOW. Un écran I2C est aussi utilisé
 * Ju'tiliserai la librairie officielle pour manipuller l'écran et donc les fonction I2C de l'écran ne sont pas dans ce fichier
 *  @author Gros Maxime
 * @date 2026-06
 * @version 1.0
 * @license MIT
 */


//Capteur de temp et humidité modèle SHT40 de chez sensrion
#define SPS30_I2C_ADDR_69 0x69
#define SHT40_CMD_MEASURE_HIGH_PRECISION    0xFD  // 10ms, 0.15°C / 0.20% RH
#define SHT40_I2C_ADDR 0x40

//Capteur de particule SPS30 de chez sensirion aussi
#define SPS30_I2C_ADDR_69 0x69
#define SPS30_WAKE_UP_CMD_ID            0x0006
#define SPS30_SLEEP_CMD_ID              0x0010
#define SPS30_START_MEASUREMENT_CMD_ID  0x0010
#define SPS30_STOP_MEASUREMENT_CMD_ID   0x0104
#define SPS30_READ_VALUES_CMD_ID        0x0300
#define SPS30_READ_DATA_READY_FLAG_CMD_ID 0x0202
#define SPS30_START_FAN_CLEANING_CMD_ID 0x5607
#define SPS30_AUTO_CLEANING_INTERVAL_CMD_ID 0x8004

typedef struct {
    float mc1p0;
    float mc2p5;
    float mc4p0;
    float mc10p0;
    float nc0p5;
    float nc1p0;
    float nc2p5;
    float nc4p0;
    float nc10p0;
    float typicalParticleSize;
} Capteur_PM_float;

struct SHT40_Data {
    float temperature;
    float humidity;
    bool isValid;
};

class SHT40 {
  public:
      SHT40(uint8_t address = SHT40_I2C_ADDR);

      /**
       * @brief Lit la température et l'humidité du capteur.Lecture des mesures, hautes précision obligatoire
       * @return Structure SHT40_Data contenant les valeurs et un indicateur de validité.
       */
      SHT40_Data readMeasurement();

      // Utilitaires
      //Utiliser que lors du setup aussi
      void softReset();
      //Sera utiliser pour juste vérifier lors du setup
      bool readSerialNumber(char* buffer, size_t bufferSize); 



  private:
      uint8_t _i2cAddress;
      
      uint8_t calculateCRC(uint8_t data[], uint8_t length);
      bool _read_data(uint8_t command, uint8_t* buffer, size_t length);
      
      // Conversion des données brutes
      float _convertTemperature(uint16_t raw);
      float _convertHumidity(uint16_t raw);
};
class SPS30 {
public:
    // Constructeur
    SPS30(uint8_t address = SPS30_I2C_ADDR_69);
    /**
     * @brief Réveille le capteur. tous est dans le nom
     */
    void wakeup();

    /**
     * @brief Met le capteur en veille. tous est dans le nom aussi
     */
    void sleep();

    /**
     * @brief Lance un cycle de nettoyage manuel du ventilateur. ne pas utiliser, vaut mieux configurer le registre automatique
     */
    void startFanCleaning();
    /**
     * @brief Démarre l'acquisition des mesures.
     * @param format 1 ->loat, 2 -> uint16_t.
     */
    void startMeasurement(int format);

    /**
     * @brief Arrête l'acquisition des mesures.
     */
    void stopMeasurement();

    /**
     * @brief Vérifie si de nouvelles données sont prêtes à être lues.
     * @return true si des données sont disponibles, false sinon.
     */
    bool readDataReadyFlag();

    /**
     * @brief Lit les données brutes et les convertit en structure structurée.
     * @return Structure Capteur_PM_float remplie. 
     *         Les valeurs seront à 0.0f si la lecture échoue.
     */
    Capteur_PM_float mesure();

    bool setAutoCleaningInterval(uint16_t hours);

private:
    /**
     * @brief Calcule le CRC selon l'algorithme Sensirion.
     * @param data Tableau de 2 octets.
     * @return La valeur CRC calculée.
     */
    uint8_t calcCrc(uint8_t data[2]);
    /**
     * @brief Écrit un registre et une valeur sur le bus I2C avec CRC fait automatiquement.
     * @param reg Adresse du registre (16 bits).
     * @param val Valeur à écrire (16 bits).
     */
    void write(uint16_t reg, uint16_t val);

    /**
     * @brief Lit un registre 16 bits depuis le capteur avec vérification CRC.
     * @param reg Adresse du registre.
     * @param result Référence vers la variable de stockage du résultat.
     * @return true si la lecture et le CRC sont valides, false sinon.
     */
    bool read(uint16_t reg, uint16_t &result);

    /**
     * @brief Convertit 4 octets (2 paires MSB/LSB + CRC) en float.
     * @param msb1, lsb1, crc1 Première paire de données.
     * @param msb2, lsb2, crc2 Deuxième paire de données.
     * @return La valeur float décodée, ou 0.0f en cas d'erreur CRC.
     */
    float bytesToFloat(uint8_t msb1, uint8_t lsb1, uint8_t crc1, 
                       uint8_t msb2, uint8_t lsb2, uint8_t crc2);
};  

