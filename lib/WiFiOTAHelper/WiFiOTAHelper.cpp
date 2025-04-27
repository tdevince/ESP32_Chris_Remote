#include "WiFiOTAHelper.h"
#include "Definitions.h" // Definitions header file

/**
 * @brief start the LittleFS file system.  For first time, set FORMAT_LITTLEFS_IF_FAILED to true
 * to format memory the first time.  Otherwise, set to false.
 * 
 */
void prepareLittleFS(bool formatIfFailed) {
    if (!LittleFS.begin(formatIfFailed)) {
        Serial.println("LittleFS mount failed");
        return;
    }
    Serial.println("LittleFS mounted successfully");
}

/**
 * @brief Initializes the WiFi Connection, uses WiFiManager to handle connection
 * 
 */
void initWiFi(const String& hostName, WiFiManager& wifiManager) {
    Serial.println("Booting");
    WiFi.hostname(hostName.c_str());
    WiFi.mode(WIFI_STA);

    wifiManager.setTimeout(120);
    //wifiManager.resetSettings();  //For testing, reset credentials
 

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "hostName"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

    // Automatically connect using saved credentials
    bool res;
    // res = wifiManager.autoConnect(); // auto generated AP name from chipid
    res = wifiManager.autoConnect(hostName.c_str()); // anonymous ap

    //res = wifiManager.autoConnect("ESP32 Demo","password"); // password protected ap

    if (!res) {
        Serial.println("Failed to connect");
        ESP.restart();
    } else {
        Serial.println("Connected... :)");
    }
}

/**
 * @brief Initializes ArduinoOTA, Keep this code to allow WiFi updates
 *   
 * // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32"); note use WiFi.HostName() in initWiFi instead

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
 */
void initOTA() {
    ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

    ArduinoOTA.begin();

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.getHostname());
}

/**
 * @brief Get the OTA Mode object from the file system.  If the file does not exist, 
 * write Mode_Normal to the file system.  If in Calibration mode, get the calibration data from the file system.
 * 
 */
void getFileData()
{
  //Read SysMode from Flash, if file does not exist, write Mode_Normal.
  if(LittleFS.exists("/OTAdata.txt"))
  {
    File myFile=LittleFS.open("/OTAdata.txt",FILE_READ);
    myFile.read((byte *)&OTAMode, sizeof(OTAMode));
    myFile.close();
    char buffer[20];
    sprintf(buffer, "OTA Mode = %d", OTAMode); // Convert the integer to a string
    Serial.println(buffer);
  } else {
    File myFile=LittleFS.open("/OTAdata.txt",FILE_WRITE);
    myFile.write((byte *)&OTAMode, sizeof(OTAMode));
    myFile.close();
  }
  
  if(!OTAMode) //if we are in normal mode, get Calibration data, if not create default
  {
    if(LittleFS.exists("/Caldata.txt"))
    {
      File myFile=LittleFS.open("/Caldata.txt",FILE_READ);
      myFile.read((byte *)&CalData, sizeof(CalData));
      myFile.close();
      char buffer[200];
      sprintf(buffer, "CalData = %d, %d, %d, %d, %d, %d, %d, %d, %d", 
        CalData[0], CalData[2], CalData[4], CalData[6], CalData[8],CalData[10], CalData[12],CalData[14], CalData[16]); // Convert the integer to a string
      Serial.println(buffer);
    } else {
      File myFile=LittleFS.open("/Caldata.txt",FILE_WRITE);
      CalData[0]=800;
      for(int i=1;i<17;i++)
      {
        CalData[i]=CalData[i-1]+100; //preset all values
      }
      myFile.write((byte *)&CalData, sizeof(CalData));
      myFile.close();
    }
  }
    
}
