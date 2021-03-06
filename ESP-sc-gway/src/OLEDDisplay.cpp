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

int cycledisplay = 0;
String gwIP = "10.1.1.1";
String gwBR = "460800";

const char *Cday;
int Chour;
int Cminute;
int Csecond;
long CRSSI;
bool beat = true;

void OLED_getLoraStats() {
  OL_LORA_rx_rcv   = getLoraRXRCV();
  OL_LORA_rx_ok    = getLoraRXOK();
  OL_LORA_rx_bad   = getLoraRXBAD();
  OL_LORA_rx_nocrc = getLoraRXNOCRC();
  OL_LORA_pkt_fwd  = getLoraPKTFWD();
}

void OLED_setIP2Display(String ip) {
  gwIP = ip;
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

void OLEDDisplay_IPBD() {
  // Displays IP and Baudrate on the displaying
  // This is to prevent burning of the oled...
  oled.setCursor(0,0);
  oled.print("IP: ");
  oled.setCursor(0,8);
  oled.println(gwIP);
  oled.setCursor(0,24);
  oled.println("BaudRate:");
  oled.println(gwBR);
}

void OLEDDisplay_Time() {
  oled.setCursor( 0 , 0 );
  //oled.print(Cday);
  //oled.print(" ");
  if ( Chour < 10 ) oled.print("0"); // Print leading zero
  oled.print( Chour );
  if ( beat )
    oled.print(":");
  else
    oled.print(" ");
  if ( Cminute < 10 ) oled.print("0"); // Print leading zero
  oled.print( Cminute );
}

// Draws an RSSI icon
// RSSI > -65  -> Good
const int wGood[] = { 0x43 , 0x44 , 0x02 , 0x74 , 0x73 , 0x00 , 0x7F, 0x7F};
// RSSI > -70 < -65  -> OK
const int wOk[] = { 0x43 , 0x44 , 0x02 , 0x74 , 0x73 , 0x00 , 0x7C, 0x00};
// RSSI > -80 < -70  -> BAD
const int wBad[] = { 0x43 , 0x44 , 0x02 , 0x74 , 0x73 , 0x00 , 0x00, 0x00};
// RSSI < -80 -> Very BAD - Icon with blinking !
const int wVBad1[] = { 0x43 , 0x44 , 0x02 , 0x04 , 0x03 , 0x00 , 0x00, 0x5C};
const int wVBad2[] = { 0x43 , 0x44 , 0x02 , 0x04 , 0x03 , 0x00 , 0x00, 0x00};

void OLEDRSSI_Icon() {
  int pos = 56 ; // Byte position for icon
  int i , j = 0;
  int *graph;


  if ( CRSSI > - 65 )
    graph = (int *)&wGood;
  else
    if ( CRSSI > -70 )
      graph = (int *)&wOk;
    else
      if ( CRSSI > -80 )
        graph = (int *)&wBad;
      else
        if ( beat )
          graph = (int *)&wVBad1;
        else
          graph = (int *)&wVBad2;

  for ( i = pos ; i < pos+8 ; i++  ) { // Right now the icon is on the top right (i=72)
    screen[i] = graph[j];
    j++;
  }

}


void OLEDDisplay_Animate() {
  uint32_t nowseconds = (uint32_t) millis() /1000;

  nowseconds = (uint32_t) millis() /1000;
  if (nowseconds - displayseconds >= 1 ) {		// Wake up every xx seconds
        if ( cycledisplay == 0 ) {
          oled.clear(PAGE);

          OLEDRSSI_Icon();
          OLEDDisplay_Time();

          OLEDDisplay_Status();
          oled.display();
        } else {
          oled.clear(PAGE);
          OLEDDisplay_IPBD();
          oled.display();
        }
        cycledisplay++;
        cycledisplay = cycledisplay % 2;

        displayseconds = nowseconds;
  }

  beat = !beat;
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
