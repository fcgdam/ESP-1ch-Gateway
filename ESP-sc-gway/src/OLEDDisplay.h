#define OLED_I2C_ADR 0      // I2C OLED display Address: 0 -> 0x3C (default) 1 - 0x3D
#define OLED_PIN_RESET 255  // Reset pin not used

void OLEDDisplay_Init( void );
void OLEDDisplay_println(const char *str);
void OLEDDisplay_Animate( void );
