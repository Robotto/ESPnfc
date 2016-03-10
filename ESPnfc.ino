/**************************************************************************/
/*!

    Connects to wifi and waits for an NFC tag with a specific UID.
    Sends a UDP packet if tag is found.

    This specific version is used to unloack a door, hence the unlock() function. (and the IPAddress object called doorIP)

    To enable debug message, define DEBUG in PN532/PN532_debug.h

	Runs on an ESP8266 (nodeMCU v0.9) connected to an adafruit NFC (PN532) shield

	HW setup:
	NFC 	NodeMCU (ESP):
	IRQ 	D3 		(GPIO0)
	RST 	D4 		(GPIO2)
	SCK 	D5 		(GPIO14)
	MISO 	D6 		(GPIO12)
	MOSI 	D7 		(GPIO13)
	SDA(SS)	D2		(GPIO4)

*/
/**************************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>

#include <SPI.h>
#include <PN532_SPI.h>
#include "PN532.h"

void unlock();
void errorLoop();
void wifiCheck();

const char* ssid     = "SSID";
const char* password = "PASS";

IPAddress doorIP(192,168,13,37);

//UDP stuff:
WiFiUDP Udp;
const unsigned int remotePort = 1337;
const int UDP_PACKET_SIZE = 7;
byte packetBuffer[ UDP_PACKET_SIZE ]; //buffer to hold and outgoing packets

PN532_SPI pn532spi(SPI, D2);
PN532 nfc(pn532spi);


void setup(void) {
  Serial.begin(115200);
  Serial.println("Hello!");
  WiFi.begin(ssid, password);

  delay(500);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  String ipString=String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]);
  Serial.print("WiFi up!");
  Serial.print("  IPv4: ");
  Serial.println(ipString);



  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }

  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);

  // Set the max number of retry attempts to read from a card
  // This prevents us from waiting forever for a card, which is
  // the default behaviour of the PN532.
  //nfc.setPassiveActivationRetries(0xFF);
  nfc.setPassiveActivationRetries(10);

  // configure board to read RFID tags
  nfc.SAMConfig();


  Serial.println("Waiting for an ISO14443A card");
}

void loop(void) {

	wifiCheck();

  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);

  if (success) {
    Serial.println("Found a card!");
    Serial.print("UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("UID Value: ");
    for (uint8_t i=0; i < uidLength; i++)
    {
      Serial.print(" 0x");Serial.print(uid[i], HEX);
    }
    Serial.println("");

    // wait until the card is taken away
    while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength)) {}
    if(uid[0]==0xDE && uid[1]==0xAD && uid[2]==0xBE && uid[3]==0xEF && uid[4]==0x13 && uid[5]==0x37 && uid[6]==0x42)
    	{
    	unlock();
    	delay(3000);
		}
  }
  else
  {
    // PN532 probably timed out waiting for a card
    yield();
  }
}



void unlock()
{
  Serial.println("sending UDP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, UDP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0]='H';
  packetBuffer[1]='E';
  packetBuffer[2]='L';
  packetBuffer[3]='L';
  packetBuffer[4]='O';
  packetBuffer[5]='!';
  packetBuffer[6]='\n';

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(doorIP, remotePort); //NTP requests are to port 123
  Udp.write(packetBuffer, UDP_PACKET_SIZE);
  Udp.endPacket();
}

void wifiCheck()
{
if(WiFi.status() != WL_CONNECTED)
{
  int tickCount=0;
  Serial.println("Wifi dropped. Retry in 30 seconds.");

  delay(30000);

  Serial.println("Connecting");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println(".");
      tickCount++;
      if(tickCount>100)
      {
          Serial.println("ded...");
          errorLoop();
      }
   }

   Serial.println("Connected!");
   //if connectivity has been restored, reset state?
}
}

void errorLoop()
{
  unsigned long currentEpoch,epochAtError=millis();
  unsigned long lastmillis=0;

  while(1)
  {
    while(millis()<lastmillis+1000) yield(); //WAIT FOR ABOUT A SECOND
    currentEpoch+=((millis()-lastmillis)/1000); //add  a second or more to the current epoch
    lastmillis=millis();
    if (currentEpoch>epochAtError+300) ESP.reset(); //reset after 5 minutes
  }
}