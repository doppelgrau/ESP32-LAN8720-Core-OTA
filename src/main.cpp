#include <Arduino.h>
#include <ETH.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

// Ethernet
#define ETH_POWER_PIN   32
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_ADDR        0
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18

// global variables
byte ethernetStatus = -1;
const char* host = "ESP32Init";
WebServer server(80);

/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<html><head><title>ESP32 Initial Setup</title></head>"
"<body><h1>Update</h1><p>Please upload the apporpriate firmware for this device.</p>"
"<form method='POST' action='/update' enctype='multipart/form-data' id='upload_form'>"
  "<label for=\"update\">Firmware:</label> <input type='file' name='update'>"
  "<input type='submit' value='Update'>"
"</form>"
"</body></html>";
const char* updateOk = 
"<html><head><title>ESP32 Initial Setup</title></head>"
"<body><h1>Update success</h1><p>Device wil automatically reboot.</p>"
"</body></html>";
const char* updateFailed = 
"<html><head><title>ESP32 Initial Setup</title></head>"
"<body><h1>Update failed</h1>"
"</body></html>";

void EtherEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      ethernetStatus = 0;
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      ethernetStatus = 1;
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      ethernetStatus = 2;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      ethernetStatus = 0;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      ethernetStatus = -1;
      break;
    default:
      break;
  }
}

void setup() {
  // debug
    Serial.begin(115200);

  // Start Network
  WiFi.onEvent(EtherEvent);
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLOCK_GPIO17_OUT);
  ETH.config(IPAddress(172, 30, 0, 222),IPAddress(0, 0, 0, 0),IPAddress(255, 255, 255, 0),IPAddress(0, 0, 0, 0),IPAddress(0, 0, 0, 0));          

  // wait for Link up
  while (ethernetStatus < 2) {
    delay(100);
  }

    if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", (Update.hasError()) ? updateFailed : updateOk);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

void loop() {
  server.handleClient();
  delay(1);
}
