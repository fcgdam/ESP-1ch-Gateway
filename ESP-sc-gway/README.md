Single Channel LoRaWAN Gateway
==============================
This repository contains an implementation of a single
channel LoRaWAN gateway mashed up from several Single Channel Gateway implementations for the 
ESP8266.

Source Code comes from: [dlarue gateway][1] and [Hallard GW][10].

It has been tested on the Wemos D1 Mini, using a Semtech SX1276 transceiver 
on a [WeMos-Lora][3] shield and Wemos D1 OLED shield with an SSD1306 display.

The code is for testing and development purposes only, and is not meant 
for production usage. 


Features
--------
- listen on configurable frequency and spreading factor
- SF7 to SF12
- status updates
- can forward to two servers
- DNS support for server lookup
- NTP Support for time sync with internet time servers
- Webserver support (default port 8080)
- OTA Updates
- Access Point Mode (for OTA)

Not (yet) supported:
- PACKET_PUSH_ACK processing
- SF7BW250 modulation
- FSK modulation
- downstream messages (tx)

Added features if you're using [WeMos-Lora][3] Shield as gateway
- 2 On board RGB LED for visual 
- 1 SSD1306 I2C OLED connector
- 1 On board push button


Dependencies
------------

- [gBase64][7] library by Adam Rudd is now integrated in the project, no need to install
- [Time][5] library Arduino [documentation][6]
- [NeoPixelBus][4] library is you're using [WeMos Lora][3] Shield as gateway

Connections
-----------
See [things4u][8] in the [hardware][9] section for building and connection instructions
See [WeMos-Lora][3] github if you're using WeMos Lora Shield as gateway


Configuration
-------------

Defaults:

- LoRa:   SF7 at 868.1 Mhz
- Server: 54.229.214.112, port 1700  (The Things Network: croft.thethings.girovito.nl)
  or directly croft.thethings.girovito.nl

Edit .h file (ESP-sc-gway.h) to change configuration (look for: "Configure these values!").

Please set location, email and description.

License
-------
The source files in this repository are made available under the Eclipse
Public License v1.0, except for the base64 implementation, that has been
copied from the Semtech Packet Forwader.

[1]: https://github.com/dlarue/ESP-1ch-Gateway-v2.0.git
[2]: https://hallard.me
[3]: https://github.com/hallard/WeMos-Lora
[4]: https://github.com/Makuna/NeoPixelBus
[5]: https://github.com/PaulStoffregen/Time
[6]: http://playground.arduino.cc/code/time
[7]: https://github.com/adamvr/arduino-base64
[8]: http://things4u.github.io
[9]: http://things4u.github.io/HardwareGuide/hardware_guide.html
[10]: https://github.com/hallard/ESP-1ch-Gateway
