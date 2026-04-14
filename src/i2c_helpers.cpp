/**
 * @file i2c_helpers.c
 * @author Nicolas Boulanger
 * @author Maxime Goncalves
 * @author Dominique Richer
 * @author Emilie Croteau
 * @date 2025-07-28
 * @brief Contient les fonctions pour interagir avec les périphériques i2c
 * @defgroup I2C_HELPERS i2c helper functions
 * @copyright
 */
#include "i2c_helpers.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>


void spi_init(void)
{
    pinMode(SPI_CS_PIN, OUTPUT);
    digitalWrite(SPI_CS_PIN, HIGH);
    SPI.begin(SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

    // ADC config
    spiWrite16(REG_ADC_CONFIG, 0x0F0F);
    spiWrite16(REG_CONFIG, 0x0000);
}

bool spiWrite16(uint8_t reg, uint16_t val)
{
    uint16_t cmd = reg;
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
    digitalWrite(SPI_CS_PIN, LOW);
    SPI.transfer(cmd);          // send write command
    SPI.transfer(highByte(val)); // send MSB
    SPI.transfer(lowByte(val));  // send LSB
    digitalWrite(SPI_CS_PIN, HIGH);
    SPI.endTransaction();
    return true;
}

bool spiRead24s(uint8_t reg, int32_t &out)
{
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
    digitalWrite(SPI_CS_PIN, LOW);
    SPI.transfer(reg + 1);       // Read command
    uint32_t b2 = SPI.transfer(0x00);
    uint32_t b1 = SPI.transfer(0x00);
    uint32_t b0 = SPI.transfer(0x00);
    digitalWrite(SPI_CS_PIN, HIGH);
    SPI.endTransaction();

    int32_t raw = (b2 << 16) | (b1 << 8) | b0;

    // sign extend
    if (raw & 0x800000)
        raw |= 0xFF000000;

    out = raw;
    return true;
}
