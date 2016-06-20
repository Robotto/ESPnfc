# ESPnfc
NodeMCU module (ESP8266) using a PN532 NFC chip to read tags and do a thing with UDP

Follow the install instructions for the PN532/NFC library [here](https://github.com/Seeed-Studio/PN532)

Connects to wifi and waits for a NFC tags with specific UIDs. Sends a UDP packet if a matching tag is found.

This specific version is used to unlock a door, hence the unlock() function. (and the IPAddress object called doorIP)

To enable debug message, define DEBUG in PN532/PN532_debug.h, which is part of the NFC library.

Runs on an ESP8266 (nodeMCU v0.9) connected to an adafruit NFC (PN532) shield

HW setup:

| NFC     | NodeMCU | (ESP)    |
|---------|---------|----------|
| IRQ     | D3      | (GPIO0)  |
| RST     | D4      | (GPIO2)  |
| SCK     | D5      | (GPIO14) |
| MISO    | D6      | (GPIO12) |
| MOSI    | D7      | (GPIO13) |
| SDA(SS) | D2      | (GPIO4)  |
