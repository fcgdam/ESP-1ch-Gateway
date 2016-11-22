// Support for Wemos Oled display
#include <Arduino.h>
#include <Wire.h>
#include <SFE_MicroOLED.h>
#include "OLEDDisplay.h"

MicroOLED oled(OLED_PIN_RESET, OLED_I2C_ADR);

int SCREEN_WIDTH  = oled.getLCDWidth();
int SCREEN_HEIGHT = oled.getLCDHeight();


void OLEDDisplay_Init() {
  oled.begin();
  oled.clear(ALL);
  oled.display();
  oled.clear(PAGE);
}

void OLEDDisplay_println(const char *str) {
  oled.println(str);
  oled.display();
}



void OLEDDisplay_Animate() {


}
