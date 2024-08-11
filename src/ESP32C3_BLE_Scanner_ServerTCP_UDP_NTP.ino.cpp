# 1 "C:\\Users\\Paul\\AppData\\Local\\Temp\\tmp9d0e8l24"
#include <Arduino.h>
# 1 "C:/Users/Paul/Documents/PlatformIO/Projects/240727-102916-lolin_c3_mini/src/ESP32C3_BLE_Scanner_ServerTCP_UDP_NTP.ino"
# 13 "C:/Users/Paul/Documents/PlatformIO/Projects/240727-102916-lolin_c3_mini/src/ESP32C3_BLE_Scanner_ServerTCP_UDP_NTP.ino"
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
#include "pfodLinkedPointerList.h"
#include "LastSeen.h"
#include "SafeString.h"
#include <WiFi.h>


static bool highForLedOn = false;
static size_t eepromOffset = 40;


static pfodLinkedPointerList<LastSeen> listOfLastSeen;
static LastSeen* getLastSeen(SafeString &name);
void BLE_init();
void bleScannerTask( void * parameter );
void setUpWiFiServices();
void setup();
void loop();
void startTelnetServer();
void handleTelnetConnection();
void notFound();
void handleRoot();
void startWebServer();
#line 48 "C:/Users/Paul/Documents/PlatformIO/Projects/240727-102916-lolin_c3_mini/src/ESP32C3_BLE_Scanner_ServerTCP_UDP_NTP.ino"
static LastSeen* getLastSeen(SafeString &name) {
  LastSeen *devicePtr = listOfLastSeen.getFirst();
  while (devicePtr) {
    if (name.startsWith(devicePtr->getDeviceName())) {
      return devicePtr;
    }
    devicePtr = listOfLastSeen.getNext();
  }
  return devicePtr;
}

static int scanTime = 2;
static BLEScan *pBLEScan;

class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.haveName()) {
        cSF(sfName, 50);
        sfName = advertisedDevice.getName().c_str();
        if (debugPtr) {
          debugPtr->print("Device name: ");
          debugPtr->println(sfName);
        }

        int idx = sfName.indexOf(',');
        if (idx >= 0) {
          sfName.substring(sfName, 0, idx);
          sfName += ',';
        }

        LastSeen *devicePtr = getLastSeen(sfName);
        if (!devicePtr) {

          if (debugPtr) {
            debugPtr->print("Adding Device name: ");
            debugPtr->println(sfName);
          }
          devicePtr = new LastSeen(sfName.c_str());
          listOfLastSeen.add(devicePtr);
        }

        devicePtr->updateLastSeen(millis());
        devicePtr->setAdvertisedName(advertisedDevice.getName().c_str());
      }
    }
};

static TaskHandle_t bleScannerHandle = NULL;

void BLE_init() {
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(40);
  pBLEScan->setWindow(30);
}


void bleScannerTask( void * parameter ) {
  BLE_init();

  for (;;) {
    pBLEScan->clearResults();
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    if (debugPtr) {
      debugPtr->print("Devices found: ");
      debugPtr->print(foundDevices.getCount());
      debugPtr->print(" on ");
      debugPtr->println(WiFi.localIP());
    }
    yield();
  }

  vTaskDelete( NULL );
}

void setUpWiFiServices() {
  setNtpSupportDebug(debugPtr);
  initializeNtpSupport();
  resetDefaultTZstr();
  startWebServer();
  startTelnetServer();
}

static uint32_t chipId = 0;

void setup() {
  Serial.begin(115200);

#ifdef DEBUG
  SafeString::setOutput(Serial);
  debugPtr = &Serial;
  debugPtr->println();
#endif

  for(int i=0; i<17; i=i+8)
  {
   chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }


#ifdef DEBUG
  setESPAutoWiFiConfigDebugOut(Serial);
#endif

  if (ESPAutoWiFiConfigSetup(-RGB_BUILTIN, highForLedOn, eepromOffset)) {
    return;
  }

  setUpWiFiServices();

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

  uint8_t i;
  if (telnetServer.hasClient()) {
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {

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

      telnetServer.available().stop();
    }
  }


  for (i = 0; i < MAX_SRV_CLIENTS; i++) {
    if (telnetServerClients[i] && telnetServerClients[i].connected()) {
      if (telnetServerClients[i].available()) {


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


  if (size_t len = Serial.available()) {
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);

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