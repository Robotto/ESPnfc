/**************************************************************************/
/*!

    Connects to wifi and waits for an NFC tag with a specific UID.
    Sends a UDP packet if tag is found.

    This specific version is used to unloack a door, hence the unlock() function. (and the IPAddress object called doorIP)

    To enable debug message, define DEBUG in PN532/PN532_debug.h

	Runs on an ESP8266 (nodeMCU v0.9) connected to an adafruit NFC (PN532) shield

	HW setup:
  NFC 	NodeMCU (ESP):
  SS (CS) D2    (GPIO4)
	IRQ 	  D3 		(GPIO0)
	RST 	  D4 		(GPIO2)
	SCK 	  D5 		(GPIO14)
	MISO 	  D6 		(GPIO12)
	MOSI 	  D7 		(GPIO13)
  
  Jumpers: SEL0=closed , SEL1=closed (set to SPI mode)
*/
/**************************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>

#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>

#include <ESP8266mDNS.h>

void unlock();

//UDP stuff:
WiFiUDP Udp;
//const unsigned int remotePort = 1337;
const int UDP_PACKET_SIZE = 7; //change to whatever you need.
byte packetBuffer[ UDP_PACKET_SIZE ]; //buffer to hold outgoing packets

PN532_SPI pn532spi(SPI, D2);
PN532 nfc(pn532spi);

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println("AP: " + myWiFiManager->getConfigPortalSSID());
  Serial.println("IP: " + WiFi.softAPIP().toString());
}


void setup(void) {

  WiFi.hostname("ESPNFC");
  
  //WiFi.persistent(false);
  //WiFi.mode(WIFI_STA); //prevent random APs from forming?!

  Serial.begin(115200);
  Serial.println("Hello!");
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  Serial.println("Connecting to wifi..");
  wifiManager.setAPCallback(configModeCallback); //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setConnectTimeout(30); //try to connect to known wifis for a long time before defaulting to AP mode
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "ESPNFC"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("ESPNFC")) {
    Serial.println("failed to connect and hit timeout");
    ESP.restart(); //reset and try again, or maybe put it to deep sleep 
  }

  //OTA:
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("ESPNFC");
  // No authentication by default
  ArduinoOTA.setPassword((const char *)"1804020311");
  //ArduinoOTA.setPasswordHash((const char *)"77ca9ed101ac99e43b6842c169c20fda");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA START!");
    delay(500);
  });

  ArduinoOTA.onEnd([]() {
  	Serial.println("OTA End.. brace for reset");
  	ESP.restart();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

  ArduinoOTA.onError([](ota_error_t error) {
    String buffer=String("Error[") + String(error) + String("]: ");
    if (error == OTA_AUTH_ERROR) buffer+=String("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) buffer+=String("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) buffer+=String("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) buffer+=String("Receive Failed");
    else if (error == OTA_END_ERROR) buffer+=String("End Failed");
    
    Serial.println(buffer);
  });

  ArduinoOTA.begin();

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
  Serial.println("\n-----------\n");
}

void loop(void) {

  
  #define NUM_ACCEPTED_UIDS 4 //<-----------------CHANGE THIS IF ADDING CARDS!!
  const uint8_t acceptedUIDs[NUM_ACCEPTED_UIDS][8] = {
  							// It is neccessary to account for UID length since a 7 byte ID could contain a 4 byte ID, which would cause a misfire
  							//  #0	  #1	#2	  #3    #4    #5    #6    #Length
  							   {0x04, 0x04, 0x10, 0x12, 0xFF, 0x38, 0x85, 7},//LektorP
                   //{0x04, 0x97, 0x03, 0x12, 0xFF, 0x38, 0x84, 7},//Robotto old
                   {0x7A, 0x3F, 0x54, 0xA2, 0x00, 0x00, 0x00, 4}, //robotto NEW 
  							   //{0xE0, 0x84, 0xDF, 0x16, 0x00, 0x00, 0x00, 4}, //DjAlligatorFace
  							   {0x44, 0x0F, 0x31, 0xC6, 0x00, 0x00, 0x00, 4}, //BAK
  							   {0xEA, 0xD6, 0x54, 0xA2, 0x00, 0x00, 0x00, 4} //Nikolaj
                 };
                 

  boolean success;
  uint8_t uidLength;   // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);

  if (success) {
    Serial.println("Found a card!");
    Serial.print("UID Length: ");
	  Serial.print(uidLength, DEC);
	  Serial.println(" bytes");
    Serial.print("UID Value: ");
    
    for (uint8_t i=0; i < uidLength; i++)
    {
      Serial.print(" 0x");
      Serial.print(uid[i], HEX);
    }
    Serial.println("");

    // wait until the card is taken away
    while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength)) yield(); //let ESPcore handle wifi stuff

    for (int j = 0; j < NUM_ACCEPTED_UIDS; ++j) //check each UID
    {
    	uint8_t matchingBytes=0;
    	Serial.print("Checking for matching bytes with uid #");
    	Serial.print(j);
		  Serial.print(": ");

		if(uidLength!=acceptedUIDs[j][7]) //length check
			{
				Serial.println("UID length mismatch; skipping.");
				continue;
			}

        for (uint8_t k=0; k < uidLength; k++)
        	{
    		if(uid[k]==acceptedUIDs[j][k]) matchingBytes++;
        	}

    	if (matchingBytes==uidLength) //is the id matching with an intended length?
    		{
    		Serial.print("MATCH! ");
    		unlock();
    		delay(500); //avoid UDP flooding
    		break; //would you check the rest of the UIDs?
			}
		else Serial.println("No deal.");

	}
	Serial.println("\n-----------\n");
  }
  else yield(); // PN532 probably timed out waiting for a card.. let's let the ESPcore handle wifi stuff
}


void unlock()
{

  Serial.println("Sending mDNS query");
  //BUG: only services named "esp" are found... https://github.com/esp8266/Arduino/issues/2151
  int n = MDNS.queryService("esp", "udp"); // Send out query for esp tcp services
  Serial.println("mDNS query done");
  if (n == 0) {
    Serial.println("no services found doing nothing..");
    return;
  }
  else {
    Serial.print(n);
    Serial.println(" service(s) found");
    for (int i = 0; i < n; ++i) {
      // Print details for each service found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(MDNS.hostname(i));
      Serial.print(" (");
      Serial.print(MDNS.IP(i));
      Serial.print(":");
      Serial.print(MDNS.port(i));
      Serial.println(")");


  //workaround:
  if(MDNS.hostname(i)=="esprelay"){
  Serial.println("Sending UDP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, UDP_PACKET_SIZE);
  
  packetBuffer[0]='S';
  packetBuffer[1]='E';
  packetBuffer[2]='S';
  packetBuffer[3]='A';
  packetBuffer[4]='M';
  packetBuffer[5]='E';
  packetBuffer[6]='\n';

  //Udp.beginPacket(doorIP, remotePort);
  Udp.beginPacket(MDNS.IP(i), MDNS.port(i));
  
  //Udp.beginPacket(doorAddress,remotePort);
  Udp.write(packetBuffer, UDP_PACKET_SIZE);
  Udp.endPacket();
  }
  }
}
}

