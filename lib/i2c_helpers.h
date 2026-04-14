/**
 * @file i2c_helpers.h
 * @author Nicolas Boulanger
 * @author Maxime Goncalves
 * @author Dominique Richer
 * @author Emilie Croteau
 * @date 2025-07-28
 * @brief Contient les fonctions pour interagir avec les périphériques i2c
 * @defgroup I2C_HELPERS SPI helper functions
 * @copyright
 */
#ifndef SPI_HELPERS_H
#define SPI_HELPERS_H

#include <SPI.h>

/**
 * @brief Initialize SPI for INA229
 */
void spi_init(void);

/**
 * @brief SPI write 16-bit register
 */
bool spiWrite16(uint8_t reg, uint16_t val);

/**
 * @brief SPI read 16-bit register
 */
bool spiRead16(uint8_t reg, uint16_t &out);

/**
 * @brief SPI read 24-bit signed register
 */
bool spiRead24s(uint8_t reg, int32_t &out);

#endif // I2C_HELPERS_H
