#ifndef WIFI_OTA_HELPER_H
#define WIFI_OTA_HELPER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <FS.h>
#include <LittleFS.h>

// Function prototypes
void prepareLittleFS(bool formatIfFailed);
void initWiFi(const String& hostName, WiFiManager& wifiManager);
void initOTA();
void getFileData(); //function to write and read from LittleFS

#endif // WIFI_OTA_HELPER_H
