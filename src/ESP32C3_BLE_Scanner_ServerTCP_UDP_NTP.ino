/*
   ESP32C3_BLE_Scanner_ServerTCP_UDP_NTP.cpp
   by Matthew Ford,  2021/12/06
   (c)2021 Forward Computing and Control Pty. Ltd.
   NSW, Australia  www.forward.com.au
   This code may be freely used for both private and commerical use.
   Provide this copyright is maintained.

*/

// Needs Huge APP, 2M APP, 1M SPIFF  Partition setting

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <WiFiClient.h>
#include <WebServer.h>
#include "ntpSupport.h"

static Stream *debugPtr = NULL;

#undef RGB_BUILTIN
#define RGB_BUILTIN 7

static bool firstLoop = true;

static WebServer server(80);
static void startWebServer();
static void startTelnetServer();
static void handleTelnetConnection();

#include "ESPAutoWiFiConfig.h"
#include "pfodLinkedPointerList.h" // iterable linked list of pointers to objects
#include "LastSeen.h"  // class for storing on linked list
#include "SafeString.h"
#include <WiFi.h>

// settings for the Auto WiFi connect
static bool highForLedOn = false; // turns led OFF when WiFi connected
static size_t eepromOffset = 40; // if you use EEPROM.begin(size) in your code add the size here so AutoWiFi data is written after your data
// need to add ntpSupport getTimeZoneStorageSize() to this offset

static pfodLinkedPointerList<LastSeen> listOfLastSeen;

// returns NULL if not found
static LastSeen* getLastSeen(SafeString &name) {
  LastSeen *devicePtr = listOfLastSeen.getFirst();
  while (devicePtr) {
    if (name.startsWith(devicePtr->getDeviceName())) {
      return devicePtr;
    } // else
    devicePtr = listOfLastSeen.getNext();
  }
  return devicePtr; // null not found
}

static int scanTime = 2; //In seconds
static BLEScan *pBLEScan;

class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.haveName()) {
        cSF(sfName, 50); // max 32
        sfName = advertisedDevice.getName().c_str();
        if (debugPtr) {
          debugPtr->print("Device name: ");
          debugPtr->println(sfName);
        }
        // only scan for == upto first ,
        int idx = sfName.indexOf(',');
        if (idx >= 0) {
          sfName.substring(sfName, 0, idx);
          sfName += ',';
        }

        LastSeen *devicePtr = getLastSeen(sfName);
        if (!devicePtr) {
          // not  found add it upto first ,
          if (debugPtr) {
            debugPtr->print("Adding Device name: "); 
            debugPtr->println(sfName);
          }
          devicePtr = new LastSeen(sfName.c_str()); // note MUST use new since pfodLinkedPointerList uses delete when remove() called
          listOfLastSeen.add(devicePtr);
        }
        // update lastseen
        devicePtr->updateLastSeen(millis());
        devicePtr->setAdvertisedName(advertisedDevice.getName().c_str()); // save the full name
      }
    }
};

static TaskHandle_t bleScannerHandle = NULL;

void BLE_init() {
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(40);  // same as Apple in foreground
  pBLEScan->setWindow(30); // same as Apple in foreground, less or equal setInterval value
}

/* this function will be invoked when additionalTask was created */
void bleScannerTask( void * parameter ) {
  BLE_init();
  /* loop forever */
  for (;;) {
    pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    if (debugPtr) {
      debugPtr->print("Devices found: ");
      debugPtr->print(foundDevices.getCount());
      debugPtr->print(" on ");
      debugPtr->println(WiFi.localIP());
    }
    yield();
  }
  /* delete a task when finished, this will never happen because this is an infinite loop */
  vTaskDelete( NULL );
}

void setUpWiFiServices() {
  setNtpSupportDebug(debugPtr);
  initializeNtpSupport();
  resetDefaultTZstr(); // only need this first time through
  startWebServer();
  startTelnetServer();
}

static uint32_t chipId = 0;

void setup() {
  Serial.begin(115200);

#ifdef DEBUG
  SafeString::setOutput(Serial); // for debug error msgs
  debugPtr = &Serial;
  debugPtr->println();
#endif

  for(int i=0; i<17; i=i+8) 
  {
	  chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  /* Initialise wifi module */
#ifdef DEBUG
  setESPAutoWiFiConfigDebugOut(Serial); // turns on debug output for the ESPAutoWiFiConfig code
#endif
  
  if (ESPAutoWiFiConfigSetup(-RGB_BUILTIN, highForLedOn, eepromOffset)) { // check if we should start access point to configure WiFi settings
    return; // in config mode so skip rest of setup
  }

  setUpWiFiServices(); // do this first. Get continual reboots if create BLE task first and then call this

  BaseType_t err = xTaskCreate(
                     bleScannerTask,
                     "bleScannerTask",
                     102400,
                     NULL,
                     1,
                     &bleScannerHandle);
  if (err != (BaseType_t)1 && debugPtr) {
      debugPtr->print("xTaskCreatePinnedToCore returned:");
      debugPtr->println((int)err);
  }
}

void loop() {

  if (firstLoop && debugPtr) {
    debugPtr->printf("ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    debugPtr->printf("This chip has %d cores\n", ESP.getChipCores());
    debugPtr->print("Chip ID: "); 
    debugPtr->println(chipId);
    firstLoop = false;
  }

  if (ESPAutoWiFiConfigLoop()) { 
    return;
  }

  server.handleClient();
  yield();
  processNTP();
  yield();
  handleTelnetConnection();
  yield();
}

// ------------------ trival telnet server -------------------------
//how many clients should be able to telnet to this ESP32
#define MAX_SRV_CLIENTS 1

static WiFiServer telnetServer(23);
static WiFiClient telnetServerClients[MAX_SRV_CLIENTS];

void startTelnetServer() {
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  if (debugPtr) {
    debugPtr->print("Ready! Use 'telnet ");
    debugPtr->print(WiFi.localIP());
    debugPtr->println(" 23' to connect");
  }
}

void handleTelnetConnection() {
  //check if there are any new clients
  uint8_t i;
  if (telnetServer.hasClient()) {
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      //find free/disconnected spot
      if (!telnetServerClients[i] || !telnetServerClients[i].connected()) {
        if (telnetServerClients[i]) telnetServerClients[i].stop();
        telnetServerClients[i] = telnetServer.available();
        if (!telnetServerClients[i] && debugPtr) {
            debugPtr->println("available broken");
        }

        if (debugPtr) {
          debugPtr->print("New client: ");
          debugPtr->print(i); debugPtr->print(' ');
          debugPtr->println(telnetServerClients[i].remoteIP());
        }
        break;
      }
    }
    if (i >= MAX_SRV_CLIENTS) {
      //no free/disconnected spots so reject
      telnetServer.available().stop();
    }
  }

  //check clients for data
  for (i = 0; i < MAX_SRV_CLIENTS; i++) {
    if (telnetServerClients[i] && telnetServerClients[i].connected()) {
      if (telnetServerClients[i].available()) {

        //get data from the telnet client and push it to the UART
        Serial.println();
        Serial.print(" >>>> Telnet:");
        while (telnetServerClients[i].available()) 
          Serial.write(telnetServerClients[i].read());
        Serial.println();
      }
    }
    else {
      if (telnetServerClients[i]) {
        telnetServerClients[i].stop();
      }
    }
  }

  //check UART for data
  if (size_t len = Serial.available()) {
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);
    //push UART data to all connected telnet clients
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      if (telnetServerClients[i] && telnetServerClients[i].connected()) {
        telnetServerClients[i].write(sbuf, len);
        delay(1);
      }
    }
  }
}

void notFound() {
  server.send(404, "text/plain", "Not found");
}

void handleRoot() {
  if (debugPtr) {
    debugPtr->println(">>> WebServer handleRoot");
  }
  String msg = "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>BLE Temperature Sensors</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>";
  time_t now;
  msg += "TimeZone: ";
  msg += getCurrentTZ();
  msg += "<br>";
  msg += getCurrentTZdescription();
  msg += "<p>At ";
  now = time(nullptr); 
  msg += ctime(&now);
  msg += "<br>";
  msg += "The BLE devices found were:-<br>";
  
  LastSeen *devicePtr = listOfLastSeen.getFirst();
  msg += "<h1>";
  if (devicePtr == NULL) {
    msg += "No devices found so far.<br>";
  } else {
    while (devicePtr != NULL) {
      msg += devicePtr->getAdvertisedName();
      msg += "<font size=\"-1\"> ";
      msg += (millis() - devicePtr->getLastSeen()) / 1000.0;
      msg += " sec ago</font>";
      msg += "<br>";
      devicePtr = listOfLastSeen.getNext();
    }
  }
  msg += "</h1>";
  msg += "</body></html>";

  server.send(200, "text/html", msg);
}

void startWebServer() {
  server.on("/", handleRoot);
  server.onNotFound(notFound);
  server.begin();
  if (debugPtr) {
    debugPtr->println("webserver started");
  }
}
