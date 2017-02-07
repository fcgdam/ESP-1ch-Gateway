// Support for Wemos Oled display
#include <Arduino.h>
#include <Wire.h>
#include <SFE_MicroOLED.h>
#include "OLEDDisplay.h"
#include "loraModem.h"

MicroOLED oled(OLED_PIN_RESET, OLED_I2C_ADR);

int SCREEN_WIDTH  = oled.getLCDWidth();
int SCREEN_HEIGHT = oled.getLCDHeight();


// Lora Counters for statistics
uint32_t OL_LORA_rx_rcv;
uint32_t OL_LORA_rx_ok;
uint32_t OL_LORA_rx_bad;
uint32_t OL_LORA_rx_nocrc;
uint32_t OL_LORA_pkt_fwd;

uint32_t displayseconds;

void OLED_getLoraStats() {
  OL_LORA_rx_rcv   = getLoraRXRCV();
  OL_LORA_rx_ok    = getLoraRXOK();
  OL_LORA_rx_bad   = getLoraRXBAD();
  OL_LORA_rx_nocrc = getLoraRXNOCRC();
  OL_LORA_pkt_fwd  = getLoraPKTFWD();
}

void OLEDDisplay_Init() {
  oled.begin();
  oled.clear(ALL);
  oled.display();
  oled.clear(PAGE);
}

void OLEDDisplay_Clear() {
  oled.clear(ALL);
  oled.display();
  oled.clear(PAGE);
}

void OLEDDisplay_println(const char *str) {
  oled.println(str);
  oled.display();
}

void OLEDDisplay_printxy(int x, int y, const char *str) {
  oled.setCursor( x, y);
  oled.println(str);
  oled.display();
}

void OLEDDisplay_Status() {
  OLED_getLoraStats();
  oled.setCursor(0,8);
  oled.print("Rx: ");
  oled.println(OL_LORA_rx_rcv);

  oled.setCursor(0, 16);
  oled.print("Ok: ");
  oled.println(OL_LORA_rx_ok);

}

void OLEDDisplay_Animate() {
  uint32_t nowseconds = (uint32_t) millis() /1000;

  nowseconds = (uint32_t) millis() /1000;
    if (nowseconds - displayseconds >= 2 ) {		// Wake up every xx seconds
        oled.clear(PAGE);
        OLEDDisplay_Status();
        oled.display();
        displayseconds = nowseconds;
    }

  yield();

}
