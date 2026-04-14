/**
 * @file config.h
 * @author Nicolas Boulanger
 * @author Maxime Goncalves
 * @author Dominique Richer
 * @author Emilie Croteau
 * @date 2025-07-28
 * @brief Contient toutes les configurations de pins et différentes valeurs par défault
 * @defgroup CONFIG Configureation
 * @copyright
 */
#ifndef CONFIG_H
#define CONFIG_H

// --- Pins / HW ---
#define SPI_MOSI_PIN        7
#define SPI_MISO_PIN        8   ////a changer au besoin
#define SPI_SCLK_PIN        9
#define SPI_CS_PIN          11  /// same
#define MOSFET_GATE_PIN     5
#define THERMISTOR_1_PIN    0
#define THERMISTOR_2_PIN    1
#define THERMISTOR_3_PIN    3

// --- INA228 registers ---
// --- INA229 SPI registers ---
#define REG_CONFIG          0x00000000
#define REG_ADC_CONFIG      0x0100
#define REG_VSHUNT          0x00010000
#define REG_VBUS            0x00010100

// --- LSB (ADCRANGE=0) ---
#define VBUS_LSB_V          0.003125f
#define VSHUNT_LSB_V        0.00000125f

// --- Réglages ---
#define R_SHUNT_OHMS        0.015f
#define AVG_SAMPLES         12
#define SAMPLE_DELAY_MS     20
#define LOAD_SETTLE_MS      250
#define PRINT_PERIOD_S      5

// --- Réglage thermistances ---
#define R_FIXED     10000.0     //Valeur de résistance pour diviseur de tension
#define BETA        4150        //3950.0
#define T0          298.15
#define R0          10000.0     //Valeur de la thermistance

// --- BLE ---
#define SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define UPDATE_TIME     2000    //!< Delay pour mise à jour en ms

#endif //CONFIG_H
