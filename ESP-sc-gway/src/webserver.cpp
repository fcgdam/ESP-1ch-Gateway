#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <TimeLib.h>
#include "loraModem.h"
#include "ESP-sc-gway.h"

// ================================================================================
// WEBSERVER FUNCTIONS (PORT 8080)

int webDebug = 0 ;

// You can switch webserver off if not necessary
// Probably better to leave it in though.
ESP8266WebServer server(SERVERPORT);
IPAddress TTNServer;
uint8_t GWMAC_address[6];

// ----------------------------------------------------------------------------
// Output the 4-byte IP address for easy printing
// ----------------------------------------------------------------------------
String printIP(IPAddress ipa) {
    	String response;
    	response+=(IPAddress)ipa[0]; response+=".";
    	response+=(IPAddress)ipa[1]; response+=".";
    	response+=(IPAddress)ipa[2]; response+=".";
    	response+=(IPAddress)ipa[3];
    	return (response);
}

// ----------------------------------------------------------------------------
// stringTime
// Only when RTC is present we print real time values
// t contains number of milli seconds since system started that the event happened.
// So a value of 100 wold mean that the event took place 1 minute and 40 seconds ago
// ----------------------------------------------------------------------------
String stringTime(unsigned long t) {
	String response;
	String Days[7]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

	if (t==0) { response = " -none- "; return(response); }

	// now() gives seconds since 1970
	time_t eventTime = now() - ((millis()-t)/1000);
	byte _hour   = hour(eventTime);
	byte _minute = minute(eventTime);
	byte _second = second(eventTime);

	response += Days[weekday(eventTime)-1]; response += " ";
	response += day(eventTime); response += "-";
	response += month(eventTime); response += "-";
	response += year(eventTime); response += " ";
	if (_hour < 10) response += "0";
	response += _hour; response +=":";
	if (_minute < 10) response += "0";
	response += _minute; response +=":";
	if (_second < 10) response += "0";
	response += _second;
	return (response);
}


// ----------------------------------------------------------------------------
// WIFI SERVER
//
// This funtion implements the WiFI Webserver (very simple one). The purpose
// of this server is to receive simple admin commands, and execute these
// results are sent back to the web client.
// Commands: DEBUG, ADDRESS, IP, CONFIG, GETTIME, SETTIME
// The webpage is completely built response and then printed on screen.
// ----------------------------------------------------------------------------
void WifiServer(const char *cmd, const char *arg) {

	String response;
	char *dup, *pch;

	yield();
	if (webDebug >=2) {
		Serial.print(F("WifiServer new client"));
	}

	// These can be used as a single argument
	if (strcmp(cmd, "DEBUG")==0) {									// Set debug level 0-2
		webDebug=atoi(arg); response+=" webDebug="; response+=arg;
	}
	if (strcmp(cmd, "IP")==0) {										// List local IP address
		response+=" local IP=";
		response+=(IPAddress) WiFi.localIP()[0]; response += ".";
		response+=(IPAddress) WiFi.localIP()[1]; response += ".";
		response+=(IPAddress) WiFi.localIP()[2]; response += ".";
		response+=(IPAddress) WiFi.localIP()[3];
	}

	if (strcmp(cmd, "GETTIME")==0) { response += "gettime tbd"; }	// Get the local time
	if (strcmp(cmd, "SETTIME")==0) { response += "settime tbd"; }	// Set the local time
	if (strcmp(cmd, "HELP")==0)    { response += "Display Help Topics"; }
	if (strcmp(cmd, "RESET")==0)   { response += "Resetting Statistics";
  		resetLoraStats();
	}

	// Do work, fill the webpage
	delay(15);
	response +="<!DOCTYPE HTML>";
	response +="<HTML><HEAD>";
	response +="<TITLE>ESP8266 1ch Gateway</TITLE>";
	response +="</HEAD>";
	response +="<BODY>";

	response +="<h1>ESP Gateway Config:</h1>";
	response +="Version: "; response+=VERSION;
	response +="<br>ESP is alive since "; response+=stringTime(1);
	response +="<br>Current time is    "; response+=stringTime(millis());
	response +="<br>";

	response +="<h2>WiFi Config</h2>";
	response +="<table style=\"max_width: 100%; min-width: 40%; border: 1px solid black; border-collapse: collapse;\" class=\"config_table\">";
	response +="<tr>";
	response +="<th style=\"background-color: green; color: white;\">Parameter</th>";
	response +="<th style=\"background-color: green; color: white;\">Value</th>";
	response +="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">IP Address</td><td style=\"border: 1px solid black;\">"; response+=printIP((IPAddress)WiFi.localIP()); response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">IP Gateway</td><td style=\"border: 1px solid black;\">"; response+=printIP((IPAddress)WiFi.gatewayIP()); response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">NTP Server</td><td style=\"border: 1px solid black;\">"; response+=NTP_TIMESERVER; response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">LoRa Router</td><td style=\"border: 1px solid black;\">"; response+=_TTNSERVER; response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">LoRa Router IP</td><td style=\"border: 1px solid black;\">"; response+=printIP((IPAddress)TTNServer); response+="</tr>";
	response +="</table>";

	response +="<h2>System Status</h2>";
	response +="<table style=\"max_width: 100%; min-width: 40%; border: 1px solid black; border-collapse: collapse;\" class=\"config_table\">";
	response +="<tr>";
	response +="<th style=\"background-color: green; color: white;\">Parameter</th>";
	response +="<th style=\"background-color: green; color: white;\">Value</th>";
	response +="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">Free heap</td><td style=\"border: 1px solid black;\">"; response+=ESP.getFreeHeap(); response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">ESP Chip ID</td><td style=\"border: 1px solid black;\">"; response+=ESP.getChipId(); response+="</tr>";
	response +="</table>";

	response +="<h2>LoRa Status</h2>";
	response +="<table style=\"max_width: 100%; min-width: 40%; border: 1px solid black; border-collapse: collapse;\" class=\"config_table\">";
	response +="<tr>";
	response +="<th style=\"background-color: green; color: white;\">Parameter</th>";
	response +="<th style=\"background-color: green; color: white;\">Value</th>";
	response +="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">Frequency</td><td style=\"border: 1px solid black;\">"; response+=LORA_freq; response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">Spreading Factor</td><td style=\"border: 1px solid black;\">"; response+=getLoraSF(); response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">Gateway ID</td><td style=\"border: 1px solid black;\">";
	response +=String(GWMAC_address[0],HEX);									// The MAC array is always returned in lowercase
	response +=String(GWMAC_address[1],HEX);
	response +=String(GWMAC_address[2],HEX);
	response +="ffff";
	response +=String(GWMAC_address[3],HEX);
	response +=String(GWMAC_address[4],HEX);
	response +=String(GWMAC_address[5],HEX);
	response+="</tr>";
	response +="</table>";

	response +="<h2>Statistics</h2>";
	delay(1);
	response +="<table style=\"max_width: 100%; min-width: 40%; border: 1px solid black; border-collapse: collapse;\" class=\"config_table\">";
	response +="<tr>";
	response +="<th style=\"background-color: green; color: white;\">Counter</th>";
	response +="<th style=\"background-color: green; color: white;\">Value</th>";
	response +="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">Packages Received</td><td style=\"border: 1px solid black;\">"; response +=getLoraRXRCV(); response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">Packages OK </td><td style=\"border: 1px solid black;\">"; response +=getLoraRXOK(); response+="</tr>";
	response +="<tr><td style=\"border: 1px solid black;\">Packages Forwarded</td><td style=\"border: 1px solid black;\">"; response +=getLoraPKTFWD(); response+="</tr>";
	response +="<tr><td>&nbsp</td><td> </tr>";

	response +="</table>";

	response +="<br>";
	response +="<h2>Settings</h2>";
	response +="Click <a href=\"/RESET\">here</a> to reset statistics<br>";

	response +="webDebug level is: ";
	response += webDebug;
	response +=" set to: ";
	response +=" <a href=\"DEBUG=0\">0</a>";
	response +=" <a href=\"DEBUG=1\">1</a>";
	response +=" <a href=\"DEBUG=2\">2</a><br>";

	response +="Click <a href=\"/HELP\">here</a> to explain Help and REST options<br>";
	response +="</BODY></HTML>";

	server.send(200, "text/html", response);

	delay(5);
	free(dup);									// free the memory used, before jumping to other page
}

void startWebServer(IPAddress _ttnServer, uint8_t _MAC_address[]) {
  Serial.println("Web Server is ON");
  server.on("/",        []() { WifiServer("","");	      });
  server.on("/HELP",    []() { WifiServer("HELP","");	  });
  server.on("/RESET",   []() { WifiServer("RESET","");	});
  server.on("/DEBUG=0", []() { WifiServer("DEBUG","0");	});
  server.on("/DEBUG=1", []() { WifiServer("DEBUG","1");	});
  server.on("/DEBUG=2", []() { WifiServer("DEBUG","2");	});

  server.begin();											// Start the webserver
  Serial.print(F("Admin Server started on port "));
  Serial.println(SERVERPORT);

  TTNServer = _ttnServer;

  //We could do it with a mem copy
  GWMAC_address[0] = _MAC_address[0];
  GWMAC_address[1] = _MAC_address[1];
  GWMAC_address[2] = _MAC_address[2];
  GWMAC_address[3] = _MAC_address[3];
  GWMAC_address[4] = _MAC_address[4];
  GWMAC_address[5] = _MAC_address[5];
}

void handleWebServer() {
  server.handleClient();
}
