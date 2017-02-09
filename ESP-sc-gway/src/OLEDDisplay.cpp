// Support for Wemos Oled display
#include <Arduino.h>
#include <Wire.h>
#include <SFE_MicroOLED.h>
#include "OLEDDisplay.h"
#include "loraModem.h"

MicroOLED oled(OLED_PIN_RESET, OLED_I2C_ADR);

int SCREEN_WIDTH  = oled.getLCDWidth();
int SCREEN_HEIGHT = oled.getLCDHeight();

// For accessing the screen buffer directly
uint8_t *screen = NULL;

// Lora Counters for statistics
uint32_t OL_LORA_rx_rcv;
uint32_t OL_LORA_rx_ok;
uint32_t OL_LORA_rx_bad;
uint32_t OL_LORA_rx_nocrc;
uint32_t OL_LORA_pkt_fwd;

uint32_t displayseconds;

const char *Cday;
int Chour;
int Cminute;
int Csecond;
long CRSSI;

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

  screen = oled.getScreenBuffer();

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

void OLEDDisplay_Time() {
  oled.setCursor( 32 , 0 );
  //oled.print(Cday);
  //oled.print(" ");
  if ( Chour < 10 ) oled.print("0"); // Print leading zero
  oled.print( Chour );
  oled.print(":");
  if ( Cminute < 10 ) oled.print("0"); // Print leading zero
  oled.print( Cminute );
}

// Draws an RSSI icon
// RSSI > -65  -> Good
// RSSI > -70 < -65  -> OK
// RSSI > -80 < -70  -> BAD
// RSSI < -80 -> Very BAD
void OLEDRSSI_Icon(int x , int y , long rssi) {
  int i;

  for ( i = 0 ; i < 16 ; i++  ) {
    screen[i] = i * 2 ;
  }

}


void OLEDDisplay_Animate() {
  uint32_t nowseconds = (uint32_t) millis() /1000;

  nowseconds = (uint32_t) millis() /1000;
  if (nowseconds - displayseconds >= 1 ) {		// Wake up every xx seconds
        oled.clear(PAGE);
        OLEDDisplay_Status();
        OLEDDisplay_Time();

        oled.display();
        displayseconds = nowseconds;
  }

  yield();

}

void OLEDDisplay_SetTime(const char* day, int hour, int min ) {
  Cday = day;
  Chour = hour;
  Cminute = min;

}

void OLEDDisplay_SetRSSI(long RSSI) {
  CRSSI = RSSI;
}
