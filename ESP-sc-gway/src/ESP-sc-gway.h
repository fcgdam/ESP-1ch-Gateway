//
// Author: Maarten Westenberg
// Version: 1.0.0
// Date: 2016-03-25
//
// This file contains a number of compile-time settings that can be set on (=1) or off (=0)
// The disadvantage of compile time is minor compared to the memory gain of not having
// too much code compiled and loaded on your ESP8266.
//
// See http://pubsubclient.knolleary.net/api.html for API description of MQTT
//
//
// 2016-06-12 - Charles-Henri Hallard (http://hallard.me and http://github.com/hallard)
//              added support for WeMos Lora Gateway
//              added AP mode (for OTA)
//              added Over The Air (OTA) feature
//              added support for onboard WS2812 RGB Led
//              refactored include source file
//              see https://github.com/hallard/WeMos-Lora
//
// ----------------------------------------------------------------------------------------

#include <Arduino.h>

#define VERSION " ! V. 3.0.0, 151116"

// Enable WeMos Lora Shield Gateway
// see https://github.com/hallard/WeMos-Lora
// Uncomment this line if you're using this shield as gateway
#define WEMOS_LORA_GW

/*******************************************************************************
 *
 * Configure these values if necessary!
 *
 *******************************************************************************/

 // Initial value of debug var. Can be changed using the admin webserver
 // Debug level! 0 is no msgs, 1 normal, 2 is extensive
 // For operational use, set initial DEBUG vaulue 0
 #define DEBUG 1

 #define CFG_sx1276_radio		// Define the correct radio type that you are using
 //#define CFG_sx1272_radio		// sx1272 not supported yet

 #define STATISTICS 1			// Gather statistics on sensor and Wifi status

 // Set the Gateway Settings
 #define _SPREADING  SF9						// We receive and sent on this Spreading Factor (only)
 #define _LOCUDPPORT 1700						// Often 1701 is used for upstream comms

 // The Gateway Listen frequency is set on loramodem.h.

 #define _PULL_INTERVAL 31						// PULL_DATA messages to server to get downstream
 #define _STAT_INTERVAL 61						// Send a 'stat' message to server
 #define _NTP_INTERVAL  3600					// How often doe we want time NTP synchronization

// TTN Server definitions
//#define _TTNSERVER "croft.thethings.girovito.nl"
//#define _TTNSERVER "router.eu.thethings.network"
#define _TTNSERVER "bridge.eu.thethings.network"

// Secondary server to send data. I'm using Node-Red with a UDP Listener Node
#define _UDPSERVER "192.168.1.17"

// TTN related
#define SERVER1 _TTNSERVER    // The Things Network: croft.thethings.girovito.nl "54.72.145.119"
#define PORT1   1700          // The port on which to send data

#define SERVER2 _UDPSERVER
#define PORT2   1700

// Gateway Ident definitions
#define _DESCRIPTION "ESP Gateway"
#define _EMAIL "set@email.com"
#define _PLATFORM "ESP8266"
// Defined on secrets.h
//#define _LAT 52.0000000
//#define _LON 6.00000000
//#define _ALT 0

// WiFi definitions

#define WIFI_CREDENTIALS  0         // Use the below _SSID and _PASS to connect to WIFI_CREDENTIALS
//#define WIFI_CREDENTIALS 1        // Use the WPA table to try to connect to several AP's. Need to change on CPP file the WPA array.

// Setup your Wifi SSID and password
// If your device already connected to your Wifi, then
// let as is (stars), it will connect using
// your previous saved SDK credentials
#define _SSID     "*********"
#define _PASS     "*********"

// Access point Password
#define _AP_PASS  "1Ch@n3l-Gateway"


// SX1276 - ESP8266 connections
#ifdef WEMOS_LORA_GW
  #define DEFAULT_PIN_SS    16          // GPIO16, D0 - Slave Select pin
  #define DEFAULT_PIN_DIO0  15          // GPIO15, D8
  #define DEFAULT_PIN_RST   NOT_A_PIN   // Unused
#else
  #define DEFAULT_PIN_SS    15          // GPIO15, D8
  #define DEFAULT_PIN_DIO0  5           // GPIO5,  D1
  #define DEFAULT_PIN_RST   NOT_A_PIN   // Unused
#endif

// Definitions for the admin webserver
#define A_SERVER   1      // Define local WebServer only if this define is set
#define SERVERPORT 8080   // local webserver port

#define A_MAXBUFSIZE 192  // Must be larger than 128, but small enough to work
#define _BAUDRATE 460800  // Works for debug messages to serial momitor (if attached).

// ntp
#define NTP_TIMESERVER "pt.pool.ntp.org"  // Country and region specific
#define NTP_INTERVAL  3600  // How often doe we want time NTP synchronization
#define NTP_TIMEZONES 1     // How far is our Timezone from UTC (excl daylight saving/summer time)

#define OLED_DISPLAY 1      // Enable the Wemos OLED Display shield usage: 1-> ON   0-> Not connected
// For OLED settings see OLEDDisplay.h file

#define STATUS_SIZE   512   // This should(!) be enough based on the static text part.. was 1024
