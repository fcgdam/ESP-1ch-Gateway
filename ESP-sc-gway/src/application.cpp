/*******************************************************************************
 * Copyright (c) 2016 Maarten Westenberg version for ESP8266
 * Copyright (c) 2015 Thomas Telkamp for initial Raspberry Version
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Notes:
 * - Once call gethostbyname() to get IP for services, after that only use IP
 *	 addresses (too many gethost name makes ESP unstable)
 * - Only call yield() in main stream (not for background NTP sync).
 *
 * 2016-06-12 - Charles-Henri Hallard (http://hallard.me and http://github.com/hallard)
 * 							added support for WeMos Lora Gateway
 * 							added AP mode (for OTA)
 * 							added Over The Air (OTA) feature
 * 							added support for onboard WS2812 RGB Led
 *							see https://github.com/hallard/WeMos-Lora
 *
 *******************************************************************************/

using namespace std;

#include "secrets.h"
#include <sys/time.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>	// https://github.com/PaulStoffregen/Time)

#include "loraModem.h"
#include "aux.h"          // Auxiliary functions common to several modules.

extern "C" {
#include "user_interface.h"
#include "lwip/err.h"
#include "lwip/dns.h"
}

#include "ESP-sc-gway.h"	// This file contains configuration of GWay
#include "RGBLed.h"				// Thid file is for onboard RGBLED of WeMos Lora Shield
#include "webserver.h"
#include "OLEDDisplay.h"

/*******************************************************************************
 *
 * Configure these values if necessary!
 *
 *******************************************************************************/

// SX1276 - ESP8266 connections - Pins defined for WEMOS-LORA shield.
int LORAMODEM_ssPin = DEFAULT_PIN_SS;
int LORAMODEM_dio0  = DEFAULT_PIN_DIO0;
int LORAMODEM_RST   = DEFAULT_PIN_RST;

// Set spreading factor (SF7 - SF12)
//sf_t sf = SF7;
sf_t LORAMODEM_sf = _SPREADING;

// Set location, description and other configuration parameters
// Defined in ESP-sc_gway.h
//
float lat			= _LAT;						// Configuration specific info. Edit ESP-sc_gway.h for setting it up
float lon			= _LON;
int   alt			= _ALT;

char platform[24]	   = _PLATFORM; 				// platform definition
char email[40]	     = _EMAIL;    				// used for contact email
char description[64] = _DESCRIPTION;				// used for free form description

IPAddress ntpServer;							// IP address of NTP_TIMESERVER
IPAddress ttnServer;							// IP Address of thethingsnetwork server
IPAddress gwDevice;               // Local IP of the gateway device

// Wifi definitions
// Array with SSID and password records. Set WPA size to number of entries in array
// WIFI Settings:
// #define NUMAPS 4  // Number of available access points
// static char const *APs[NUMAPS][2] = {
//   {"ap1","pwd1"},
//   {"ap2","pwd2"},
//   {"ap3","pwd3"},
//   {"ap4","pwd4"}
// };
#include "secrets.h" // Comment out this line and uncomment the above lines to
                     // configure your access points.
                     // This file is gitignored


int debug =  DEBUG;									// Debug level! 0 is no msgs, 1 normal, 2 is extensive

uint32_t stattime = 0;	// last time we sent a stat message to server
uint32_t pulltime = 0;	// last time we sent a pull_data request to server

// Lora Counters for statistics
uint32_t LORA_rx_rcv;
uint32_t LORA_rx_ok;
uint32_t LORA_rx_bad;
uint32_t LORA_rx_nocrc;
uint32_t LORA_pkt_fwd;

uint8_t MAC_address[6];
char    MAC_char[18];

WiFiUDP  Udp;
uint32_t lasttime;
uint32_t lastTimeSt;
uint8_t  buff_up[TX_BUFF_SIZE];
uint8_t  buff_down[RX_BUFF_SIZE];

unsigned long WIFImillis = 0 ; // for displaying RSSI on the display

// ----------------------------------------------------------------------------
// DIE is not use actively in the source code anymore.
// It is replaced by a Serial.print command so we know that we have a problem
// somewhere.
// There are at least 3 other ways to restart the ESP. Pick one if you want.
// ----------------------------------------------------------------------------
void die(const char *s)
{
  Serial.println(s);
	delay(50);
	// system_restart();						// SDK function
	// ESP.reset();
	abort();									// Within a second
}

// ----------------------------------------------------------------------------
// Print the current time
// ----------------------------------------------------------------------------
void printTime() {
	const char * Days [] ={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
	Serial.printf("%s %02d:%02d:%02d", Days[weekday()-1], hour(), minute(), second() );
}


// =============================================================================
// NTP TIME functions

const int NTP_PACKET_SIZE = 48;					// Fixed size of NTP record
byte packetBuffer[NTP_PACKET_SIZE];

// ----------------------------------------------------------------------------
// Send the request packet to the NTP server.
//
// ----------------------------------------------------------------------------
void sendNTPpacket(IPAddress& timeServerIP) {
  // Zeroise the buffer.
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	packetBuffer[0] = 0b11100011;   			// LI, Version, Mode
	packetBuffer[1] = 0;						// Stratum, or type of clock
	packetBuffer[2] = 6;						// Polling Interval
	packetBuffer[3] = 0xEC;						// Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;

	Udp.beginPacket(timeServerIP, (int) 123);	// NTP Server and Port

	if ((Udp.write((char *)packetBuffer, NTP_PACKET_SIZE)) != NTP_PACKET_SIZE) {
		die("sendNtpPacket:: Error write");
	}
	else {
		// Success
	}
	Udp.endPacket();
}


// ----------------------------------------------------------------------------
// Get the NTP time from one of the time servers
// Note: As this function is called from SyncINterval in the background
//	make sure we have no blocking calls in this function
// ----------------------------------------------------------------------------
time_t getNtpTime()
{
  WiFi.hostByName(NTP_TIMESERVER, ntpServer);
  //while (Udp.parsePacket() > 0) ; 			// discard any previously received packets
  for (int i = 0 ; i < 4 ; i++) { 				// 5 retries.
    sendNTPpacket(ntpServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 5000)
	{
      if (Udp.parsePacket()) {
        Udp.read(packetBuffer, NTP_PACKET_SIZE);
        // Extract seconds portion.
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secSince1900 = highWord << 16 | lowWord;
        Udp.flush();
        return secSince1900 - 2208988800UL + NTP_TIMEZONES * SECS_PER_HOUR;
		// UTC is 1 TimeZone correction when no daylight saving time
      }
      //delay(10);
    }
  }
  return 0; 									// return 0 if unable to get the time
}

// ----------------------------------------------------------------------------
// Set up regular synchronization of NTP server and the local time.
// ----------------------------------------------------------------------------
void setupTime() {
  setSyncProvider(getNtpTime);
  setSyncInterval(NTP_INTERVAL);
}



// ============================================================================
// UDP AND WLAN FUNCTIONS

// ----------------------------------------------------------------------------
// GET THE DNS SERVER IP address
// ----------------------------------------------------------------------------
IPAddress getDnsIP() {
	ip_addr_t dns_ip = dns_getserver(0);
	IPAddress dns = IPAddress(dns_ip.addr);
	return((IPAddress) dns);
}


// ----------------------------------------------------------------------------
// Read DOWN a package from UDP socket, can come from any server
// Messages are received when server responds to gateway requests from LoRa nodes
// (e.g. JOIN requests etc.) or when server has downstream data.
// We repond only to the server that sent us a message!
// ----------------------------------------------------------------------------
int readUdp(int packetSize, uint8_t * buff_down)
{
  uint8_t  protocol;
  uint16_t token;
  uint8_t  ident;
  char     LoraBuffer[64]; 						//buffer to hold packet to send to LoRa node

  if (packetSize > RX_BUFF_SIZE) {
	   Serial.print(F("readUDP:: ERROR package of size: "));
     Serial.println(packetSize);
	   Udp.flush();
	   return(-1);
  }

  Udp.read(buff_down, packetSize);
  IPAddress remoteIpNo = Udp.remoteIP();
  unsigned int remotePortNo = Udp.remotePort();

  uint8_t * data = buff_down + 4;
  protocol = buff_down[0];
  token = buff_down[2]*256 + buff_down[1];
  ident = buff_down[3];

  // now parse the message type from the server (if any)
  switch (ident) {
	case PKT_PUSH_DATA: // 0x00 UP
		if (debug >=1) {
			Serial.print(F("PKT_PUSH_DATA:: size ")); Serial.print(packetSize);
			Serial.print(F(" From ")); Serial.print(remoteIpNo);
			Serial.print(F(", port ")); Serial.print(remotePortNo);
			Serial.print(F(", data: "));
			for (int i=0; i<packetSize; i++) {
				Serial.print(buff_down[i],HEX);
				Serial.print(':');
			}
			Serial.println();
		}
	break;
	case PKT_PUSH_ACK:	// 0x01 DOWN
		if (debug >= 1) {
			Serial.print(F("PKT_PUSH_ACK:: size ")); Serial.print(packetSize);
			Serial.print(F(" From ")); Serial.print(remoteIpNo);
			Serial.print(F(", port ")); Serial.print(remotePortNo);
			Serial.print(F(", token: "));
			Serial.println(token, HEX);
		}
	break;
	case PKT_PULL_DATA:	// 0x02 UP
		Serial.print(F(" Pull Data"));
		Serial.println();
	break;
	case PKT_PULL_RESP:	// 0x03 DOWN

		lastTimeSt = micros();					// Store the tmst this package was received
		// Send to the LoRa Node first (timing) and then do messaging
		if (sendPacket(data, packetSize-4) < 0) {
			return(-1);
		}

		// Now respond with an PKT_PULL_ACK; 0x04 UP
		buff_up[0]=buff_down[0];
		buff_up[1]=buff_down[1];
		buff_up[2]=buff_down[2];
		buff_up[3]=PKT_PULL_ACK;
		buff_up[4]=0;

		// Only send the PKT_PULL_ACK to the UDP socket that just sent the data!!!
		Udp.beginPacket(remoteIpNo, remotePortNo);
		if (Udp.write((char *)buff_up, 4) != 4) {
			Serial.println("PKT_PULL_ACK:: Error writing Ack");
		}
		else {
			if (debug>=1) {
				Serial.print(F("PKT_PULL_ACK:: tmst="));
				Serial.println(micros());
			}
		}
		//yield();
		Udp.endPacket();

		if (debug >=1) {
			Serial.print(F("PKT_PULL_RESP:: size ")); Serial.print(packetSize);
			Serial.print(F(" From ")); Serial.print(remoteIpNo);
			Serial.print(F(", port ")); Serial.print(remotePortNo);
			Serial.print(F(", data: "));
			data = buff_down + 4;
			data[packetSize] = 0;
			Serial.print((char *)data);
			Serial.println(F("..."));
		}

	break;
	case PKT_PULL_ACK:	// 0x04 DOWN; the server sends a PULL_ACK to confirm PULL_DATA receipt
		if (debug >= 2) {
			Serial.print(F("PKT_PULL_ACK:: size ")); Serial.print(packetSize);
			Serial.print(F(" From ")); Serial.print(remoteIpNo);
			Serial.print(F(", port ")); Serial.print(remotePortNo);
			Serial.print(F(", data: "));
			for (int i=0; i<packetSize; i++) {
				Serial.print(buff_down[i],HEX);
				Serial.print(':');
			}
			Serial.println();
		}
	break;
	default:
		Serial.print(F(", ERROR ident not recognized: "));
		Serial.println(ident);
	break;
  }

  // For downstream messages, fill the buff_down buffer

  return packetSize;
}

void setupHandleOTA() {
  char thishost[17];
  // Set Hostname for OTA and network (add only 2 last bytes of last MAC Address)
  sprintf_P(thishost, PSTR("ESP-TTN-GW-%04X"), ESP.getChipId() & 0xFFFF);

  ArduinoOTA.setHostname(thishost);
  ArduinoOTA.begin();

  // OTA callbacks
  ArduinoOTA.onStart([]() {
    // Light of the LED, stop animation
    LedRGBOFF();
    Serial.println(F("\r\nOTA Starting"));
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    uint8_t percent=progress/(total/100);

    #ifdef RGB_LED_PIN
      // hue from 0.0 to 1.0 (rainbow) with 33% (of 0.5f) luminosity
      if (percent % 4 >= 2) {
        rgb_led.SetPixelColor(0, HslColor( (float) percent * 0.01f , 1.0f, 0.3f ));
        rgb_led.SetPixelColor(1, RgbColor(0));
      } else {
        rgb_led.SetPixelColor(0, RgbColor(0));
        rgb_led.SetPixelColor(1, HslColor( (float) percent * 0.01f , 1.0f, 0.3f ));
      }
      rgb_led.Show();
    #endif

    if (percent % 10 == 0) {
      Serial.print('.');
    }
  });

  ArduinoOTA.onEnd([]() {
    #ifdef RGB_LED_PIN
    rgb_led.SetPixelColor(0, HslColor(COLOR_ORANGE/360.0f, 1.0f, 0.3f));
    rgb_led.SetPixelColor(1, HslColor(COLOR_ORANGE/360.0f, 1.0f, 0.3f));
    rgb_led.Show();
    #endif
    Serial.println(F("Done Rebooting"));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef RGB_LED_PIN
    rgb_led.SetPixelColor(0, HslColor(COLOR_RED/360.0f, 1.0f, 0.3f));
    rgb_led.SetPixelColor(1, HslColor(COLOR_RED/360.0f, 1.0f, 0.3f));
    rgb_led.Show();
    #endif
    Serial.println(F("Error"));
    ESP.restart();
  });

}


// ----------------------------------------------------------------------------
// Function to join the Wifi Network
// XXX Maybe we should make the reconnect shorter in order to avoid watchdog resets.
//	It is a matter of returning to the main loop() asap and make sure in next loop
//	the reconnect is done first thing.
// ----------------------------------------------------------------------------
int WlanConnect(char * wifi_ssid, char * wifi_pass) {
  char thishost[17];
  // Set Hostname for OTA and network (add only 2 last bytes of last MAC Address)
  sprintf_P(thishost, PSTR("ESP-TTN-GW-%04X"), ESP.getChipId() & 0xFFFF);

  WiFiMode_t con_type ;
  int ret = WiFi.status();

  WiFi.mode(WIFI_AP_STA);

	if (debug>=1) {
	  Serial.print(F("========== SDK Saved parameters Start"));
	  WiFi.printDiag(Serial);
	  Serial.println(F("========== SDK Saved parameters End"));
	}

  if ( strncmp(wifi_ssid, "**", 2) && strncmp(wifi_pass, "**", 2) ) {
    Serial.println(F("Sketch contain SSID/PSK will set them"));
  }

  // No empty sketch SSID, try connect
  if (*wifi_ssid!='*' && *wifi_pass!='*' ) {
    Serial.printf("connecting to %s with psk %s\r\n", wifi_ssid, wifi_pass );
    WiFi.begin(wifi_ssid, wifi_pass);
  } else {
    // empty sketch SSID, try autoconnect with SDK saved credentials
    Serial.println(F("No SSID/PSK defined in sketch\r\nConnecting with SDK ones if any"));
  }

  // Loop until connected or 20 sec time out
  unsigned long this_start = millis();

  #ifdef WEMOS_LORA_GW
  LedRGBON( COLOR_ORANGE_YELLOW, RGB_WIFI);
  LedRGBSetAnimation(333, RGB_WIFI, 0, RGB_ANIM_FADE_IN);
  #endif

  while ( WiFi.status() !=WL_CONNECTED && millis()-this_start < 20000 ) {
    #ifdef WEMOS_LORA_GW
    LedRGBAnimate();
    #endif
    delay(1);
  }

  // Get latest WifI Status
  ret =  WiFi.status();

 // connected remove AP
  if (  ret == WL_CONNECTED  ) {
    WiFi.mode(WIFI_STA);
  } else {
    // Need Access point configuration
    // SSID = hostname
    Serial.printf("Starting AP  : %s", thishost);

    // STA+AP Mode without connected to STA, autoconnec will search
    // other frequencies while trying to connect, this is causing issue
    // to AP mode, so disconnect will avoid this
    if (ret != WL_CONNECTED) {
      // Disable auto retry search channel
      WiFi.disconnect();
    }

    // protected network
    Serial.printf(" with key %s\r\n", _AP_PASS);
    WiFi.softAP(thishost,_AP_PASS);

    Serial.print(F("IP address   : ")); Serial.println(WiFi.softAPIP());
    Serial.print(F("MAC address  : ")); Serial.println(WiFi.softAPmacAddress());
  }

  #ifdef WEMOS_LORA_GW
  con_type = WiFi.getMode();
  Serial.print(F("WIFI="));
  if (con_type == WIFI_STA) {
    // OK breathing cyan
    wifi_led_color=COLOR_YELLOW;
    Serial.println(F("STA"));
    Serial.print("IP Address: "); Serial.println(WiFi.localIP().toString());

  } else if ( con_type==WIFI_AP || con_type==WIFI_AP_STA ) {
    // Owe're also in AP ? breathing Yellow
    wifi_led_color=COLOR_ORANGE;
    if (con_type==WIFI_AP) {
      Serial.println(F("AP"));
    } else {
      Serial.println(F("AP_STA"));
    }
  } else {
    // Error breathing red
    wifi_led_color=COLOR_RED;
    Serial.println(F("???"));
  }

  // Set Breathing color to indicate connexion type on LED 2
  LedRGBON(wifi_led_color, RGB_WIFI);
  LedRGBSetAnimation(333, RGB_WIFI, 0, RGB_ANIM_FADE_IN);

  #endif

  setupHandleOTA();

  return ret;
}

// ----------------------------------------------------------------------------
// Send an UDP/DGRAM message to the MQTT server
// If we send to more than one host (not sure why) then we need to set sockaddr
// before sending.
// ----------------------------------------------------------------------------
void sendUdp(uint8_t * msg, int length) {
	int l;
	bool err = true;               // Let's assume that we are going to fail

	if (WiFi.status() != WL_CONNECTED) {
		Serial.println(F("sendUdp: ERROR not connected to WLAN"));
		Udp.flush();

    // Let's try to connect again
		if (WlanConnect( (char *) _SSID, (char *)_PASS) < 0) {
			Serial.print(F("sendUdp: ERROR connecting to WiFi"));
			yield();
			err = true;
		}
		if (debug>=1) Serial.println(F("WiFi reconnected"));
		delay(10);
    err = false;
	} else {
    err = false;    //We are connected.
  }

	//send the update to the TTN back end server.
	if ( !err ) {
    err = true;

    // Send data to the first server, normally the TTN server.
    Udp.beginPacket(ttnServer, (int) PORT1);
	  if ((l = Udp.write((char *)msg, length)) != length) {
		   Serial.println("sendUdp:: Error write");
    } else {
       err = false;
		   if (debug>=2) {
			   Serial.print(F("sendUdp 1: sent "));
			   Serial.print(l);
			   Serial.println(F(" bytes"));
		   }
	  }
	  yield();
	  Udp.endPacket();

    // Send data to the secondary server.
    #ifdef SERVER2
    // Serial.print("Sending packet to secondary server...");
    // Serial.print( SERVER2 );
    // Serial.print(":");
    // Serial.println(PORT2);
		// delay(1);
		Udp.beginPacket((char *)SERVER2, (int) PORT2);

		if ((l = Udp.write((char *)msg, length)) != length) {
				Serial.println(F("sendUdp:: Error write"));
		} else {
			err = false;
			if (debug>=2) {
				Serial.printf("sendUdp: sent %d bytes",l);
			}
		}

		yield();
		Udp.endPacket();
    #endif
	}

  // 1 fade out animation green if okay else otherwhise
  LedRGBON(err ? COLOR_RED : COLOR_GREEN, RGB_WIFI, true);
  LedRGBSetAnimation(1000, RGB_WIFI, 1, RGB_ANIM_FADE_OUT);

}


// ----------------------------------------------------------------------------
// connect to UDP – returns true if successful or false if not
// ----------------------------------------------------------------------------
bool UDPconnect() {

	bool ret = false;
	if (debug>=1) Serial.println(F("Connecting to UDP..."));
	unsigned int localPort = _LOCUDPPORT;
	if (Udp.begin(localPort) == 1) {
		if (debug>=1) Serial.println(F("Connection successful!"));
		ret = true;
	}
	else{
		//Serial.println("Connection failed");
	}
	return(ret);
}

// ----------------------------------------------------------------------------
// Setup the LoRa environment on the connected transceiver.
// - Determine the correct transceiver type (sx1272/RFM92 or sx1276/RFM95)
// - Set the frequency to listen to (1-channel remember)
// - Set Spreading Factor (standard SF7)
// The reset RST pin might not be necessary for at least the RGM95 transceiver
// ----------------------------------------------------------------------------
void SetupLoRa()
{
	Serial.println(F("Trying to Setup LoRa Module with"));
	Serial.printf("CS=GPIO%d  DIO0=GPIO%d  Reset=", LORAMODEM_ssPin, LORAMODEM_dio0 );

  if (LORAMODEM_RST == NOT_A_PIN ) {
    Serial.println(F("Unused"));
  } else {
    Serial.printf("GPIO%d\r\n", LORAMODEM_RST);
  }
  setLoraDebug(DEBUG); // Set debug mode for Lora Modem functions.
  setLoraModem( LORAMODEM_ssPin, LORAMODEM_dio0, NOT_A_PIN , NOT_A_PIN , LORAMODEM_RST , _SPREADING , false );
	initLoraModem();
}

void getLoraStats() {
  LORA_rx_rcv   = getLoraRXRCV();
  LORA_rx_ok    = getLoraRXOK();
  LORA_rx_bad   = getLoraRXBAD();
  LORA_rx_nocrc = getLoraRXNOCRC();
  LORA_pkt_fwd  = getLoraPKTFWD();
}

// ----------------------------------------------------------------------------
// Send UP periodic Pull_DATA message to server to keepalive the connection
// and to invite the server to send downstream messages when available
// *2, par. 5.2
//	- Protocol Version (1 byte)
//	- Random Token (2 bytes)
//	- PULL_DATA identifier (1 byte) = 0x02
//	- Gateway unique identifier (8 bytes) = MAC address
// ----------------------------------------------------------------------------
void pullData() {

    uint8_t pullDataReq[12]; 						// status report as a JSON object
    int pullIndex=0;
 	  int i;

    // pre-fill the data buffer with fixed fields
    pullDataReq[0]  = PROTOCOL_VERSION;						// 0x01
	  uint8_t token_h = (uint8_t)rand(); 						// random token
    uint8_t token_l = (uint8_t)rand();						// random token
    pullDataReq[1]  = token_h;
    pullDataReq[2]  = token_l;
    pullDataReq[3]  = PKT_PULL_DATA;						// 0x02

	  // READ MAC ADDRESS OF ESP8266
    pullDataReq[4]  = MAC_address[0];
    pullDataReq[5]  = MAC_address[1];
    pullDataReq[6]  = MAC_address[2];
    pullDataReq[7]  = 0xFF;
    pullDataReq[8]  = 0xFF;
    pullDataReq[9]  = MAC_address[3];
    pullDataReq[10] = MAC_address[4];
    pullDataReq[11] = MAC_address[5];

    pullIndex = 12;											// 12-byte header

    pullDataReq[pullIndex] = 0; 							// add string terminator, for safety

    //if (debug>= 2) {
		  Serial.print(F("PKT_PULL_DATA request: <"));
		  Serial.print(pullIndex);
		  Serial.print(F("> "));
		  for (i=0; i<pullIndex; i++) {
			  Serial.print(pullDataReq[i],HEX);				// DEBUG: display JSON stat
			  Serial.print(':');
		  }
		  Serial.println();
	  //}
    //send the update
    sendUdp(pullDataReq, pullIndex);
}


// ----------------------------------------------------------------------------
// Send UP periodic status message to server even when we do not receive any
// data.
// Parameter is socketr to TX to
// ----------------------------------------------------------------------------
// void sendstat() {
//
//     uint8_t status_report[STATUS_SIZE]; 					// status report as a JSON object
//     char stat_timestamp[32];								// XXX was 24
//     time_t t;
// 	  char clat[12]={0};
// 	  char clon[12]={0};
//
//     int stat_index=0;
//
//     // pre-fill the data buffer with fixed fields
//     status_report[0]  = PROTOCOL_VERSION;					// 0x01
//     status_report[3]  = PKT_PUSH_DATA;						// 0x00
//
// 	  // READ MAC ADDRESS OF ESP8266
//     status_report[4]  = MAC_address[0];
//     status_report[5]  = MAC_address[1];
//     status_report[6]  = MAC_address[2];
//     status_report[7]  = 0xFF;
//     status_report[8]  = 0xFF;
//     status_report[9]  = MAC_address[3];
//     status_report[10] = MAC_address[4];
//     status_report[11] = MAC_address[5];
//
//     uint8_t token_h   = (uint8_t)rand(); 					// random token
//     uint8_t token_l   = (uint8_t)rand();					// random token
//     status_report[1]  = token_h;
//     status_report[2]  = token_l;
//     stat_index = 12;										// 12-byte header
//
//     t = now();												// get timestamp for statistics
//
// 	  sprintf(stat_timestamp, "%d-%d-%2d %d:%d:%02d CET", year(),month(),day(),hour(),minute(),second());
// 	  yield();
//
// 	  ftoa(lat,clat,5);										// Convert lat to char array with 4 decimals
// 	  ftoa(lon,clon,5);										// As Arduino CANNOT prints floats
//
// 	  // Build the Status message in JSON format, XXX Split this one up...
// 	  delay(1);
//
//     getLoraStats();
//
//     int j = snprintf((char *)(status_report + stat_index), STATUS_SIZE-stat_index,
// 		"{\"stat\":{\"time\":\"%s\",\"lati\":%s,\"long\":%s,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%u.0,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}",
// 		stat_timestamp, clat, clon, (int)alt, LORA_rx_rcv, LORA_rx_ok, LORA_pkt_fwd, 0, 0, 0,platform,email,description);
//
// 	  yield();												// Give way to the internal housekeeping of the ESP8266
// 	  if (debug >=1) { delay(1); }
//
//     stat_index += j;
//     status_report[stat_index] = 0; 							// add string terminator, for safety
//
//     //if (debug>=2) {
// 	    Serial.print(F("stat update: <"));
// 	    Serial.print(stat_index);
// 	    Serial.print(F("> "));
// 	    Serial.println((char *)(status_report+12));			// DEBUG: display JSON stat
//
// 		  // for (int i=0; i < stat_index; i++) {
// 			//   Serial.print(status_report[i],HEX);				// DEBUG: display JSON stat
// 			//   Serial.print(':');
// 		  // }
// 		  // Serial.println();
// 	  //}
//
//     //send the update
//     // delay(1);
//     sendUdp(status_report, stat_index);
//     return;
//   }

void sendstat() {

    uint8_t status_report[STATUS_SIZE]; 					// status report as a JSON object
    char stat_timestamp[32];								// XXX was 24
    time_t t;
	char clat[10]={0};
	char clon[10]={0};

    int stat_index=0;

    // pre-fill the data buffer with fixed fields
    status_report[0]  = PROTOCOL_VERSION;					// 0x01
    status_report[3]  = PKT_PUSH_DATA;						// 0x00

	// READ MAC ADDRESS OF ESP8266, and return unique Gateway ID consisting of MAC address and 2bytes 0xFF
    status_report[4]  = MAC_address[0];
    status_report[5]  = MAC_address[1];
    status_report[6]  = MAC_address[2];
    status_report[7]  = 0xFF;
    status_report[8]  = 0xFF;
    status_report[9]  = MAC_address[3];
    status_report[10] = MAC_address[4];
    status_report[11] = MAC_address[5];

    uint8_t token_h   = (uint8_t)rand(); 					// random token
    uint8_t token_l   = (uint8_t)rand();					// random token
    status_report[1]  = token_h;
    status_report[2]  = token_l;
    stat_index = 12;										// 12-byte header

    t = now();												// get timestamp for statistics

	// XXX Using CET as the current timezone. Change to your timezone
	sprintf(stat_timestamp, "%04d-%02d-%02d %02d:%02d:%02d CET", year(),month(),day(),hour(),minute(),second());
	yield();

	ftoa(lat,clat,4);										// Convert lat to char array with 4 decimals
	ftoa(lon,clon,4);										// As Arduino CANNOT prints floats

	// Build the Status message in JSON format, XXX Split this one up...
	delay(1);

    int j = snprintf((char *)(status_report + stat_index), STATUS_SIZE-stat_index,
		"{\"stat\":{\"time\":\"%s\",\"lati\":%s,\"long\":%s,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%u.0,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}",
		stat_timestamp, clat, clon, (int)alt, LORA_rx_rcv, LORA_rx_ok, LORA_pkt_fwd, 0, 0, 0,platform,email,description);

	yield();												// Give way to the internal housekeeping of the ESP8266
	if (debug >=1) { delay(1); }
    stat_index += j;
    status_report[stat_index] = 0; 							// add string terminator, for safety

    if (debug>=2) {
		Serial.print(F("stat update: <"));
		Serial.print(stat_index);
		Serial.print(F("> "));
		Serial.println((char *)(status_report+12));			// DEBUG: display JSON stat
	}

    //send the update
	// delay(1);
    sendUdp(status_report, stat_index);
	return;
}



// Segregated setup functions

void setup_RGBLeds() {
  #ifdef WEMOS_LORA_GW
    rgb_led.Begin();
    LedRGBOFF();
  #endif
}

void setup_OLED() {
  #ifdef OLED_DISPLAY
    Serial.println(F("OLED init..."));
    OLEDDisplay_Init();
  #endif

}

// void setup_WIFI() {
//   #ifdef OLED_DISPLAY
//     OLEDDisplay_println("Set WIFI:");
//   #endif
//
//   Serial.println("Connecting to WIFI...");
//   // Setup WiFi UDP connection. Give it some time ..
//   if (WlanConnect( (char *) _SSID, (char *)_PASS) == WL_CONNECTED) {
//     // If we are here we are connected to WLAN
//     // So now test the UDP function
//     #ifdef OLED_DISPLAY
//       OLEDDisplay_println("WIFI OK!");
//     #endif
//
//     if (!UDPconnect()) {
//       Serial.println("Error UDPconnect");
//     }
//   }
//
//   WiFi.macAddress(MAC_address);
//   for (int i = 0; i < sizeof(MAC_address); ++i){
//     sprintf(MAC_char,"%s%02x:",MAC_char,MAC_address[i]);
//   }
//   Serial.print("MAC: ");
//   Serial.println(MAC_char);
// }

void setup_WIFI() {
    bool connected = false;
    char *ssid;
    char *pwd;
    int cntAP = 0;
    int tries = 0;
    String out;

    #ifdef OLED_DISPLAY
      OLEDDisplay_println("WIFI:");
    #endif

    Serial.println("Connecting to WIFI...");
    WiFi.mode(WIFI_STA);

    while ( !connected ) {
      ssid = (char *)APs[cntAP][0];
      pwd  = (char *)APs[cntAP][1];
      Serial.print("Connecting to: ");
      Serial.println(ssid);

      OLEDDisplay_printxy( 0 , 8 , "W:                     " );
      out = "W: " + String(ssid);
      OLEDDisplay_printxy( 0 , 8 , out.c_str() );

      out = "T: " + String(tries);
      OLEDDisplay_printxy( 0 , 32, out.c_str() );

      WiFi.begin(ssid, pwd );
      if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Trying next AP...");
        Serial.print("Number of tries: ");
        Serial.println(tries);

        cntAP++;
        tries++;
        if (cntAP == NUMAPS )
          cntAP = 0;

        delay(1000);
        yield();
      } else
        connected = true;
     }

      #ifdef OLED_DISPLAY
          OLEDDisplay_Clear();
          OLEDDisplay_printxy(0,0,"WIFI OK!");
      #endif

      if (!UDPconnect()) {
          Serial.println("Error UDPconnect");
      }

      WiFi.macAddress(MAC_address);
      for (int i = 0; i < sizeof(MAC_address); ++i){
        sprintf(MAC_char,"%s%02x:",MAC_char,MAC_address[i]);
      }
      Serial.print("MAC: ");
      Serial.println(MAC_char);

      gwDevice = WiFi.localIP();
      Serial.print("GW IP: ");
      Serial.println( gwDevice.toString());
}

void setup_LORA() {
  #ifdef OLED_DISPLAY
    OLEDDisplay_println("LORA MDM");
  #endif

  // Configure IO Pin
  pinMode(LORAMODEM_ssPin, OUTPUT);
  pinMode(LORAMODEM_dio0, INPUT);
  if ( LORAMODEM_RST != NOT_A_PIN ) pinMode(LORAMODEM_RST, OUTPUT);

  Serial.println(F("Seting up LORA..."));
  SPI.begin();
  delay(100);
  SetupLoRa();
  delay(100);

  #ifdef OLED_DISPLAY
    OLEDDisplay_println("LORA OK!");
  #endif
}

void display_Config() {
  // We choose the Gateway ID to be the Ethernet Address of our Gateway card
  // display results of getting hardware address
  //
  Serial.print("Gateway ID: ");
  Serial.print(MAC_address[0],HEX);
  Serial.print(MAC_address[1],HEX);
  Serial.print(MAC_address[2],HEX);
  Serial.print(0xFF, HEX);
  Serial.print(0xFF, HEX);
  Serial.print(MAC_address[3],HEX);
  Serial.print(MAC_address[4],HEX);
  Serial.print(MAC_address[5],HEX);

  Serial.print(", Listening at SF");
  Serial.print(LORAMODEM_sf);
  Serial.print(" on ");
  Serial.print((double)LORA_freq/1000000);
  Serial.println(" Mhz.");


}

void setup_TTNServer() {
  #ifdef OLED_DISPLAY
    OLEDDisplay_println("TTN N RES");
  #endif
  Serial.print("Name resolution for ");
  Serial.println(_TTNSERVER);
  WiFi.hostByName(_TTNSERVER, ttnServer);					// Use DNS to get server IP once
  delay(100);
  yield();
  yield();
  Serial.print("TTN Server is ");
  Serial.println( ttnServer );


}

void setup_NTPServer() {
  #ifdef OLED_DISPLAY
    OLEDDisplay_println("NTP Init..");
  #endif
  Serial.println("NTP Server initialization...");
	setupTime();											// Set NTP time host and interval
	setTime((time_t)getNtpTime());
	Serial.print("Time "); printTime();
	Serial.println();
}

void setup_WebAdminServer() {
  #if A_SERVER==1
    #ifdef OLED_DISPLAY
      OLEDDisplay_println("WEB Init..");
    #endif

    startWebServer(ttnServer, MAC_address);  // Passes the TTN server used and GW MAC address to be shown.
  #endif
}

// Loop segregated functions

void process_LORAWAN() {
  int buff_index;
  uint8_t buff_up[TX_BUFF_SIZE]; 						// buffer to compose the upstream packet

  // Receive Lora messages
  if ((buff_index = receivePacket(buff_up)) >= 0) {	// read is successful
    yield();
    LedRGBON(COLOR_MAGENTA, RGB_RF, true);
    LedRGBSetAnimation(1000, RGB_RF, 1, RGB_ANIM_FADE_OUT);
    sendUdp(buff_up, buff_index);					// We can send to multiple sockets if necessary
  }
  else {
    // No message received
  }
}

void process_TTN() {
  // Receive UDP PUSH_ACK messages from server. (*2, par. 3.3)
  // This is important since the TTN broker will return confirmation
  // messages on UDP for every message sent by the gateway. So we have to consume them..
  // As we do not know when the server will respond, we test in every loop.
  //
  int packetSize = Udp.parsePacket();
  if (packetSize >0) {
    yield();
    if (readUdp(packetSize , buff_down )>0) {
      // 1 fade out animation green if okay else otherwhise
      #ifdef WEMOS_LORA_GW
      LedRGBON(COLOR_GREEN, RGB_WIFI, true);
      #endif
    } else {
      #ifdef WEMOS_LORA_GW
      LedRGBON(COLOR_ORANGE, RGB_WIFI, true);
      #endif
    }
    LedRGBSetAnimation(1000, RGB_WIFI, 1, RGB_ANIM_FADE_OUT);
  }
}


void process_GateWay() {
  uint32_t nowseconds = (uint32_t) millis() /1000;

  // stat PUSH_DATA message (*2, par. 4)
	//
	nowseconds = (uint32_t) millis() /1000;
  if (nowseconds - stattime >= _STAT_INTERVAL) {		// Wake up every xx seconds
        sendstat();										// Show the status message and send to server
		stattime = nowseconds;
  }

	yield();

	// send PULL_DATA message (*2, par. 4)
	//
	nowseconds = (uint32_t) millis() /1000;
  if (nowseconds - pulltime >= _PULL_INTERVAL) {		// Wake up every xx seconds
        pullData();										// Send PULL_DATA message to server
	    	pulltime = nowseconds;
  }

}

void process_RGBLeds() {
  #ifdef WEMOS_LORA_GW
    // RF RGB LED should be off if no animation is running place
    if ( animationState[0].RgbEffectState == RGB_ANIM_NONE) {
      LedRGBOFF(RGB_RF);
    }
    // WIFI RGB LED should breath if no animation is running place
    if ( animationState[1].RgbEffectState == RGB_ANIM_NONE) {
      LedRGBON(COLOR_GREEN, RGB_WIFI);
      LedRGBSetAnimation(1000, RGB_WIFI);
    }

    // Manage Animations
    LedRGBAnimate();
  #endif
}

void process_WebAdminServer() {
  // Handle the WiFi server part of this sketch. Mainly used for administration of the node
  #if A_SERVER==1
    handleWebServer();
  #endif
}

void process_statusBar() {
  // unsigned long currentmillis = millis();
  //
  // if ( currentmillis - WIFImillis > 2500 ) {
  //   WIFImillis = currentmillis;
  //   long rssi = WiFi.RSSI();
  //   Serial.print("RSSI: ");
  //   Serial.println( rssi );
  // }

  // Let's print time
  const char * DaysNames[] ={"Su","Mo","Tu","We","Th","Fr","Sa"};
  //Serial.printf("%s %02d:%02d:%02d", DaysNames[weekday()-1], hour(), minute(), second() );
  OLEDDisplay_SetTime(  DaysNames[weekday()-1] , hour(), minute() );
  OLEDDisplay_SetRSSI(WiFi.RSSI());

  OLEDDisplay_Animate();

}


// ========================================================================
// MAIN PROGRAM (SETUP AND LOOP)

// ----------------------------------------------------------------------------
// Setup code (one time)
// ----------------------------------------------------------------------------
void setup () {

	Serial.begin(_BAUDRATE);	// As fast as possible for bus

  Serial.print(F("\r\nBooting: "));
  Serial.println( " " __DATE__ " " __TIME__);

	if (debug >= 1) {
		Serial.println(F("! debug is ON !"));
	}

  setup_RGBLeds();

  setup_OLED();

  setup_WIFI();

  setup_LORA();

  display_Config();

  setup_TTNServer();

  setup_NTPServer();

  setup_WebAdminServer();

	Serial.println("---------------------------------");

	// Breathing now set to 1000ms
  LedRGBON(wifi_led_color, RGB_WIFI);     // The wifi_led_color value comes from the Wifi setup.
	LedRGBSetAnimation(1000, RGB_WIFI);
	LedRGBOFF(RGB_RF);

}

// ----------------------------------------------------------------------------
// LOOP
// This is the main program that is executed time and time again.
// We need to give way to the bacjend WiFi processing that
// takes place somewhere in the ESP8266 firmware and therefore
// we include yield() statements at important points.
//
// Note: If we spend too much time in user processing functions
//	and the backend system cannot do its housekeeping, the watchdog
// function will be executed which means effectively that the
// program crashes.
// ----------------------------------------------------------------------------
void loop ()
{
  process_LORAWAN();            // Check for incoming LORA data

  process_TTN();                // Check for TTN backend data and send keep alives

  process_GateWay();

  process_WebAdminServer();     // Handle web admin server

  process_RGBLeds();            // Process RGB LED animations

  process_statusBar();

  // Handle OTA
  ArduinoOTA.handle();          // Handle OTA.

}
