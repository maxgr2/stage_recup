
float temperature(int cap);
static float lireVoltage(int pin);
float temperature_carte_fille(uint8_t adresse_i2c, uint8_t bat);
float LireCanalDifferentiel(uint8_t adresse_i2c, uint8_t paire, bool inverser_polarite);
static uint8_t build_control_byte_diff(uint8_t paire, bool inverser_polarite);
