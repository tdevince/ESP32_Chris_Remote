#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <FS.h>
#include <LittleFS.h>  //File system on ESP32
#include <WiFiManager.h> 
#include <driver/rtc_io.h> //Use this library for RTC IO Sleep/Wakeup
#include <U8g2lib.h> // Include the U8g2 library for graphics
#include <Wire.h> // Include the Wire library for I2C communication
#include <SPI.h>  // Include the SPI library for SPI communication
#include<esp_now.h>
#include "Definitions.h"

/**WARNING*********************************************************
 * *******WARNING**************************************************
 * **************WARNING******************************************
 * Warning:  Before flashing a new ESP32, first upload code with 
 * "FORMAT_LITTLEFS_IF_FAILED" set to "true"  Then reflash with 
 * "FORMAT_LITTLEFS_IF_FAILED" set to "false".  Otherwise the file system
 * will not be formatted, and will not properly store login information.  The
 * ESP32 may then no  longer be able to connect via WiFi.  If a USB is not
 * available, the ESP32 may then become trash!!!
 * 
 */

#define FORMAT_LITTLEFS_IF_FAILED false//only need to format FS the first time

/*
Hardware Connections
======================
Push Button to GPIO 13 and 14 pulled high with a 10K Ohm
resistor

NOTE:
======
Only RTC IO can be used as a source for external wake
source. They are pins: 0-21 for ESP32-S2

*/
#define UpButton              GPIO_NUM_13     // Only RTC IO are allowed for Wake - ESP32 Pin example
#define DownButton              GPIO_NUM_14     // Only RTC IO are allowed for Wake - ESP32 Pin example
#define CalButton              GPIO_NUM_18     // Calibration of the Controller
#define OTAButton              GPIO_NUM_8     // OTA update 
#define BattPin                GPIO_NUM_4     //ADC measurement of Battery voltage
#define I2C_ADDRESS 0x3C // I2C address for the OLED display
#define I2C_SDA 21 // SDA pin for ESP32
#define I2C_SCL 19 // SCL pin for ESP32

uint8_t ControllerAddress[]={0x68, 0xB6, 0xB3, 0x08, 0xD7, 0x6A}; //MAC address of Chris Controller

// Create an instance of the web server on port 80
WebServer server(80);

WiFiManager wifiManager;  //start wifiManager

// Define bitmask for multiple GPIOs
uint64_t bitmask = BUTTON_PIN_BITMASK(DownButton) | BUTTON_PIN_BITMASK(UpButton);
//RTC_DATA_ATTR int bootCount = 0; //RTC_DATA_ATTR is used to store data in RTC memory
//RTC memory is retained over deep sleep and reboots


U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);

//Creates a varaible called peerInfo of the data structury type esp_now_peer_info_t to 
//hold information about the peer
esp_now_peer_info_t peerInfo; 

/************WiFi Variables************** */
String ESPHostName="Chris_Remote";

/**
 * @brief start the LittleFS file system.  For first time, set FORMAT_LITTLEFS_IF_FAILED to true
 * to format memory the first time.  Otherwise, set to false.
 * 
 */
void prepareLittleFS()
{
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    Serial.print("LittleFS mount failed");
    return;
  }
}

/**
 * @brief looks to see if the OTAMode button is pressed 
 * 
 * Note: IRAM_ATTR should be used with ESP32 chips.  This places the interupt routine
 * in RAM.  This is faster than keeping it in flash.  Also, the interrupt may be 
 * missed if flash is being used at the time of interrupt if it's not placed in RAM
 */
void IRAM_ATTR OTAButtonPress()
{
  if(millis()-LastOTAPress>PressInterval) // prevent button bouncing
  {
    LastOTAPress=millis(); // Update the last press time
    OTAMode =!OTAMode;
    updateOTA=true;
  }
}

/**
 * @brief looks to see if the CalMode button is pressed 
 * 
 * Note: IRAM_ATTR should be used with ESP32 chips.  This places the interupt routine
 * in RAM.  This is faster than keeping it in flash.  Also, the interrupt may be 
 * missed if flash is being used at the time of interrupt if it's not placed in RAM
 */
void IRAM_ATTR CalButtonPress()
{
  if(millis()-LastCalPress>PressInterval) // prevent button bouncing
  {
    LastCalPress=millis(); // Update the last press time
    LastIdleTime=LastCalPress;
    CalMode =!CalMode;
    FirstDraw=true;
  }
}

/**
 * @brief Can be used to set the Calibration Offset/Xtalk for the VL53L1X
 * This is called from the web server
 * 
 */
void handle_CAL()
{
/*
  myServer.send(200,"text/plain", calOff); 
  */
}

/**
 * @brief updates the browser with the MAC address
 * 
 */
void handle_MAC(){
  String macAddress = WiFi.macAddress();
  server.send(200, "text/plain", macAddress); // Send the MAC address as a response
}

/**
 * @brief Fucntion called wihen new data is sent. This function simply prints 
 * if the message was successfully delivered or not. If the message is delivered
 *  successfully, the status variable returns 0, so we can set our success message 
 * to “Delivery Success”:
 * 
 * @param mac_addr 
 * @param status //from the esp_now_send_status_t data type structure
 */
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
   if (status==ESP_NOW_SEND_SUCCESS){DeliverySuccess=true;}  else {DeliverySuccess=false;}
 }
 
 /**
  * @brief The OnDataRecv() function will be called when a new packet arrives.
  * 
  * @param mac 
  * @param incomingData 
  * @param len Length of incoming data, Can only have a max of 250 bytes so use integer
  */
 void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
   memcpy(&ControllerData, incomingData, sizeof(ControllerData));
   Serial.print("Data Recieved: "); Serial.println(ControllerData.potADC);
   NewData=true;
 }
 
/**
 * @brief Send  data to the Chris Controller 
 *
 * @param OutData data to send.  
 */
void SendData(DataStruct OutData)
{
    //Send Data
    esp_err_t result = esp_now_send(ControllerAddress, (uint8_t *) &OutData, sizeof(OutData) );

    if (result==ESP_OK){
      //test code
      char buffer[100];
      sprintf(buffer,"Data sent with success");
      Serial.println(buffer);
    }
    else {
      Serial.println("Error sending the data");
    }

}

float interpolateData(uint16_t inputValue) {
  // Find the nearest two values in CalData
  uint16_t lowerValue = 0;
  uint16_t upperValue = 0;
  int lowerIndex = -1;
  int upperIndex = -1;

  Serial.print("Input Value =");Serial.println(inputValue);
  // Loop through CalData to find the nearest values
  for (int i = 0; i < 16; i++) {
      if (CalData[i] <= inputValue && (lowerIndex == -1 || CalData[i] > lowerValue)) {
          lowerValue = CalData[i];
          lowerIndex = i;
      }
      if (CalData[i + 1] >= inputValue && (upperIndex == -1 || CalData[i + 1] < upperValue)) {
          upperValue = CalData[i + 1];
          upperIndex = i + 1;
      }
  }
  char buffer[50];
  snprintf(buffer,sizeof(buffer),"Lower Value = %d, Index = %d",lowerValue,lowerIndex);
  Serial.println(buffer);
  snprintf(buffer,sizeof(buffer),"Upper Value = %d, Index = %d",upperValue,upperIndex);
  Serial.println(buffer);
  // If inputValue is out of bounds, clamp it
  if (lowerIndex == -1) {
      lowerValue = CalData[0];
      lowerIndex = 0;
  }
  if (upperIndex == -1) {
      upperValue = CalData[16];
      upperIndex = 16;
  }
  float lowerNum=(lowerIndex * 0.5)+2.0;
  float upperNum=(upperIndex * 0.5)+2.0;
  float interpolatedValue=0;
  if(lowerValue!=upperValue)
  {
  // Perform linear interpolation
  float ratio = (float)(inputValue - lowerValue) / (upperValue - lowerValue);
  Serial.println(ratio);
  Serial.print("lowerNum = ");Serial.print(lowerNum);Serial.print(": upperNum = ");Serial.println(upperNum);
  interpolatedValue = lowerNum + ratio * (upperNum - lowerNum); // Map to range 
  // Round to the nearest 0.5
  interpolatedValue = round(interpolatedValue * 2) / 2.0;
  } else {
    interpolatedValue=lowerNum;
  }
  return interpolatedValue;
}

/**
 * @brief Initializes the ESP_NOW network
 * 
 */
void initESP_NOW()
{
 
  WiFi.mode(WIFI_STA);//Set the device as a WiFi Station
  esp_now_init();  //initialize ESP-NOW
  esp_now_register_send_cb(OnDataSent); //register for Send Call back to get status of transmitted packet
  esp_now_register_recv_cb(OnDataRecv); //register call back function for when data is recieved
  memcpy(peerInfo.peer_addr,ControllerAddress,sizeof(ControllerAddress));
  peerInfo.channel=0;
  peerInfo.encrypt=false;
  esp_now_add_peer(&peerInfo);  //Add peer
}

/**
 * @brief Initializes the WiFi Connection
 * 
 */
void initWiFi()
{
  Serial.println("Booting");
  WiFi.hostname(ESPHostName.c_str());
  WiFi.mode(WIFI_STA);

  wifiManager.setTimeout(120);
  //wifiManager.resetSettings();  //For testing, reset credentials
 

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "Salt Monitor"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result
 
  bool res;
  // res = wifiManager.autoConnect(); // auto generated AP name from chipid
  res = wifiManager.autoConnect("Chris Remote"); // anonymous ap
  //res = wifiManager.autoConnect("Feed Monitor"); // anonymous ap
  

  //res = wifiManager.autoConnect("Salt Monitor","password"); // password protected ap

if(!res) 
{
  Serial.println("Failed to connect");
  ESP.restart();
} else 
{
  //if you get here you have connected to the WiFi    
  Serial.println("connected... :)");
}   
/*
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
*/
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
void initOTA()
{

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
 * @brief Get the Sys Mode object from the file system.  If the file does not exist, write Mode_Normal to the file system.
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

void getControllerStatus()
{
  ControllerData.cmdESP_Now=cmdStatus;
  SendData(ControllerData);
}

void drawStartPage()
{
  if(OTAMode)
  {
    u8g2.setDrawColor(1); // Set the draw color to white
    u8g2.setFontMode(1); // Set the font mode to transparent
    u8g2.clear();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr); // Set the font for text
    u8g2.drawStr(10, 15, "IP Address:"); // Draw the formatted string at (0, 15)
    u8g2.drawStr(10, 30, WiFi.localIP().toString().c_str()); // Draw the formatted string at (25, 10)
    u8g2.drawStr(10, 45, "System Mode:"); // Draw the formatted string at (25, 10)
    u8g2.drawStr(10, 60, "OTA Mode"); // Draw the formatted string at (25, 10)
    u8g2.sendBuffer(); // Send the buffer to the display
  } else {
    u8g2.setDrawColor(1); // Set the draw color to white  
    u8g2.setFontMode(1); // Set the font mode to transparent
    u8g2.clear();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr); // Set the font for text
    u8g2.drawStr(10, 15, "Waking...."); // Draw the formatted string at (0, 15)
    u8g2.drawStr(10, 30, "Normal Mode"); // Draw the formatted string at (25, 10)
    u8g2.sendBuffer(); // Send the buffer to the display
    delay(1000);
    getControllerStatus();
    preMillis=millis();
    if(!NewData){
      Serial.println("waiting...");
      while(millis()-preMillis<timeoutMillis)
      {
        if(NewData)
        {
          NewData=false;
          break;
        }
      } 
      if(ControllerData.potADC<100){Serial.println("timed out waiting for controller");}
      char buffer[20];
      snprintf(buffer, sizeof(buffer), "Wait Time = %d", millis()-preMillis); // Format O2Flow as a string  
      Serial.println(buffer);
    }
    u8g2.clearBuffer();
    char buffer[10]; // Create a buffer to hold the string
    O2Flow=interpolateData(ControllerData.potADC);
    dtostrf(O2Flow, 5, 1, buffer);
    u8g2.drawStr(10, 30, buffer); // Draw the string on the display
    u8g2.sendBuffer();

  }

}

/**
 * @brief Function to print the calibration pages on the OLED display
 * 
 * @param PageNum The page number to print
 */
void printCalPages(uint16_t PageNum)
{
  if(PageNum==1)
  {
      //First print instructions:
      u8g2.clearBuffer();
      u8g2.clear();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(5,13,"Cal. Mode: Use Up or");
      u8g2.drawStr(5,28,"Dwn to turn controller");
      u8g2.drawStr(5,43,"to desired flow. Press");
      u8g2.drawStr(5,58,"Up & Down to 'Enter'");
      u8g2.sendBuffer();
      EnterActive=true;  //Arm to allow Enter command
  } else if(PageNum>1 && PageNum<11){
    u8g2.clearBuffer();
    u8g2.clear();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5,13,"Use Up/Dwn for");
    uint16_t O2Target = (PageNum-2)+2;
    char buffer[15];
    snprintf(buffer, sizeof(buffer), "%d.0 L/min", O2Target);
    u8g2.drawStr(25,28,buffer);
    u8g2.drawStr(5,43,"Press Up & Down");
    u8g2.drawStr(5,58,"to 'Enter'");
    u8g2.sendBuffer();
    EnterActive=true;  //Arm to allow Enter command
  } else if(PageNum==11){
    u8g2.clearBuffer();
    u8g2.clear();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5,15,"Cal. Successful");
     u8g2.drawStr(5,35,"Saving Data...");
    u8g2.sendBuffer();
    uint16_t index;
    for (int i=0;i<9;i++)
    {
      CalData[i*2]=CalDataInProcess[i];
    }
    for(int i=1;i<16;i+=2)
    {
        CalData[i]=(CalData[i-1]+CalData[i+1])/2;
    }
    //write the CalData to file
    File myFile = LittleFS.open("/Caldata.txt", FILE_WRITE);
    if (myFile) {
      myFile.write((byte *)&CalData, sizeof(CalData));
      myFile.close();
      Serial.println("Calibration data saved successfully.");
    } else {
      Serial.println("Failed to open file for writing calibration data.");
    }
    delay(5000);
    CalMode=false;
    FirstDraw=true;
    LastIdleTime=millis();
    Serial.print("Cal Change millis = ");Serial.println(LastIdleTime);
    Serial.print("Current millis = ");Serial.println(millis());
    ESP.restart();  //restarting will force the display to show current controller position
  } else if(PageNum==12){
    u8g2.clearBuffer();
    u8g2.clear();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0,10,"Pot. at hard stop");
     u8g2.drawStr(0,25,"<500 Range <3500");
    u8g2.drawStr(0,40,"Reset Pot, then");
    u8g2.drawStr(0,55,"repeat Cal.");
    u8g2.sendBuffer();
    delay(5000);
    CalMode=false;
    FirstDraw=true;
    LastIdleTime=millis();
  }


}

int readADCVolts()
{
uint32_t adcVoltsIn = 0;
for (int i = 0; i < 75; i++)
{
  adcVoltsIn = adcVoltsIn + analogReadMilliVolts(BattPin);
  //delay(5);
}
adcVoltsIn= adcVoltsIn/75;
return adcVoltsIn;
}

/**
 * @brief draw the battery charge state.
 * 
 */
void drawBattery() 
{
    
  uint16_t chargeLevel;
  chargeLevel = map(readADCVolts(), 474, 680, 0, 100);
  // Define the dimensions of the battery
    const int batteryWidth = 25;  // Width of the battery
    const int batteryHeight = 12;   // Height of the battery
    const int batteryX = 90;        // X position of the battery
    const int batteryY = 5;        // Y position of the battery
    // Draw the outer battery shape
    u8g2.drawFrame(batteryX, batteryY, batteryWidth, batteryHeight); // x, y, width, height

    // Draw the battery terminal
    u8g2.drawBox(batteryX + batteryWidth, batteryY + (batteryHeight/4), 2, batteryHeight/2); // x, y, width, height

    // Calculate the width of the inner battery level based on chargeLevel (0-100)
    uint16_t innerWidth = (chargeLevel * (batteryWidth - 4)) / 100; // Subtract 2 for padding

    // Draw the inner battery level
    u8g2.drawBox(batteryX + 2, batteryY + 2, innerWidth, batteryHeight - 4); // x, y, width, height

}

/**
 * @brief Function to perform during normal operation mode
 * 
 */
void normalOps()
{
  SleepPermmissive=true;
  // Check if the interval has passed
  if (millis() - previousMillis >= interval) {
    previousMillis = millis(); // Save the last update time
    // Read the state of GPIO13 and GPIO14
    bool upState = digitalRead(UpButton) == LOW; // LOW means pressed
    bool downState = digitalRead(DownButton) == LOW; // LOW means pressed

    // Update O2Flow based on the states of GPIO13 and GPIO14
    if (upState && !downState) {//Up button pressed
      DemandButtonPressed=true;
      O2Flow += 0.5; // Increment O2Flow if GPIO13 is low and GPIO14 is high
      if(O2Flow>10.0) O2Flow=10.0; //limit max flow to 10.0 L/min
      LastDemandTime=millis();
      LastIdleTime=millis();
    } else if (!upState && downState) {//down button pressed
      DemandButtonPressed=true;
      O2Flow -= 0.5; // Decrement O2Flow if GPIO14 is low and GPIO13 is high
      if(O2Flow<2.0) O2Flow=2.0; //limit min flow to 2.0 L/min
      LastDemandTime=millis();
      LastIdleTime=millis();
    }
    // If both are low, do nothing
    // If both are high, do nothing
    // Print the current O2Flow value for debugging

    u8g2.setDrawColor(1); // Set the draw color to white
    u8g2.setFontMode(1); // Set the font mode to transparent
    char buffer[100]; // Create a buffer to hold the formatted string
    if (FirstDraw)
      {
        //u8g2.setFont(u8g2_font_ncenB08_tr); // Set the font for text
        //u8g2.setFont(u8g2_font_10x20_tr); // Set the font for text
        //u8g2.setFont(u8g2_font_helvB14_tr);
        //u8g2.setFont(u8g2_font_helvB18_tr);
        //u8g2.setFont(u8g2_font_helvB24_tr);
        O2FlowLast=O2Flow; // Store the last O2Flow value
        u8g2.clearBuffer();
        u8g2.clear();
        if(O2Flow>1)
        {
          drawBattery();
          u8g2.setFont(u8g2_font_helvB24_tr);
          //u8g2.drawStr(0, 25, "O2Flow:"); // Draw "O2Flow" at (25, 10)
          memset(buffer, 0, sizeof(buffer));
          if(O2Flow<10)// Format O2Flow as a string
          {
            snprintf(buffer, sizeof(buffer), "%.1f", O2Flow);
          } else 
          {
            snprintf(buffer, sizeof(buffer), "%.f", O2Flow);
          }
          u8g2.drawStr(10, 45, buffer); // Draw the formatted O2Flow string at (25, 40)
          u8g2.setFont(u8g2_font_helvB14_tr);
          u8g2.drawStr(60,45, "L/min"); // Draw "L/min" at (25, 40)
          u8g2.sendBuffer(); // Send the buffer to the display
          Serial.println(O2Flow);
        } else{
          u8g2.drawStr(0,25,"No Flow Data");
          u8g2.sendBuffer(); // Send the buffer to the display
        }

      }
    if (O2Flow != O2FlowLast)   //flow value has changed
    {
      u8g2.setDrawColor(0);      
      memset(buffer, 0, sizeof(buffer));
      if(O2FlowLast<10)// Format O2Flow as a string
      {
        snprintf(buffer, sizeof(buffer), "%.1f", O2FlowLast);
      } else 
      {
        snprintf(buffer, sizeof(buffer), "%.f", O2FlowLast);
      }
      u8g2.setFont(u8g2_font_helvB24_tr);
      u8g2.drawStr(10, 45, buffer); // Erase the old O2Flow string
      u8g2.sendBuffer(); // Send the buffer to the display
      u8g2.setDrawColor(1); // Set the draw color to white (draw)
      O2FlowLast = O2Flow; // Update the last O2Flow value
      memset(buffer, 0, sizeof(buffer));
      if(O2Flow<10)// Format O2Flow as a string
      {
        snprintf(buffer, sizeof(buffer), "%.1f", O2Flow);
      } else 
      {
        snprintf(buffer, sizeof(buffer), "%.f", O2Flow);
      }
      u8g2.drawStr(10, 45, buffer); // Draw the new O2Flow string
      u8g2.sendBuffer(); // Send the buffer to the display
      Serial.println(O2Flow);
    }

    FirstDraw = false; // Set FirstDraw to false after the first draw

    if(!upState && !downState && DemandButtonPressed)
    {
      if(millis()-LastDemandTime>1000) //wait one second after setting flow before updating controller
      {
        DemandButtonPressed=false;
        uint16_t NewADCIndex =(uint16_t)((O2Flow-2.0)*2);
        ControllerData.potADC=CalData[NewADCIndex];
        ControllerData.cmdESP_Now=cmdGoTo;
        SendData(ControllerData);
        LastIdleTime=millis();
        if(DeliverySuccess)
        {  //Controller should respond with current potADC value.  Wait for result
          preMillis=millis();
          if(!NewData)
          {
            u8g2.clearBuffer();
            u8g2.clear();
            u8g2.setFont(u8g2_font_helvB14_tr);
            u8g2.drawStr(10,20,"waiting...");
            u8g2.sendBuffer();
            Serial.println("waiting...");
            SleepPermmissive=false;
            while(millis()-preMillis<timeoutMillis)
            {
              LastIdleTime=millis();
              if(NewData)
              {
                NewData=false;
                break;
              }
            } 
            SleepPermmissive=true;
          }
          O2Flow=interpolateData(ControllerData.potADC);
          FirstDraw=true;
          Serial.println(O2Flow);
          LastIdleTime=millis();
          Serial.println(O2Flow);
        }
      }
    }
  }

if((millis()-LastIdleTime>IdleInterval) && SleepPermmissive)
  {
    Serial.print("Norm Ops millis = ");Serial.println(LastIdleTime);
    Serial.print("Current millis = ");Serial.println(millis());
    u8g2.sleepOn();
    Serial.println("Going to Sleep...");
    esp_deep_sleep_start();
  }

}

/**
 * @brief Function to perform during calibration
 * 
 */
void CalOps()
{
  SleepPermmissive=false;
  if(FirstDraw)
  {
    printCalPages(CalPageNum);
    FirstDraw=false;
  }
  bool upState = digitalRead(UpButton) == LOW; // LOW means pressed
  bool downState = digitalRead(DownButton) == LOW; // LOW means pressed
  //check for EnterActive Time delay
  bool TimeDelayOK=false;
  if(millis()-LastEnterActive>EnterDelayInterval)
  {
    TimeDelayOK=true;
  }
  if(upState && downState && EnterActive && TimeDelayOK) //Both buttons pressed
  {
    Serial.println("Enter Pressed");
    for(int i=0;i<9;i++)
    {
      Serial.print("CalDat[");Serial.print(i);Serial.print("] = ");Serial.print(CalDataInProcess[i]);Serial.print(", ");
    }
    Serial.println(":");
    EnterActive=false;  // disarm Enter to prevent run aways
    TimeDelayOK=false;
    LastEnterActive=millis();
    CalPageNum++;
    printCalPages(CalPageNum);
  }
  if(upState && !downState && (CalPageNum>1))
  {// Up button pressed
    if((millis()-LastDemandTime < EnterDelayInterval))
    {//if the button press was before the delay ignore the press
      return;
    } else 
    { 
      ControllerData.cmdESP_Now=cmdUp;
      SendData(ControllerData);
      if(DeliverySuccess)
      {  //Controller should respond with current potADC value.  Wait for result
        preMillis=millis();
        if(!NewData)
        {
          Serial.println("waiting...");
          while(millis()-preMillis<timeoutMillis)
          {
            if(NewData)
            {
              NewData=false;
              break;
            }
          } 
        }
        Serial.print("ADC Value = ");Serial.println(ControllerData.potADC);
        if(ControllerData.potADC<500 || ControllerData.potADC>3500)
        {
          CalPageNum=12;
          printCalPages(CalPageNum);
          return;
        }
        CalDataInProcess[CalPageNum-2]=ControllerData.potADC;
        delay(250);
        LastDemandTime=millis();
      }
    }
    
  } else if (!upState && downState && (CalPageNum>1))
  {// Down button pressed
    if((millis()-LastDemandTime < EnterDelayInterval))
    {
      return;
    } else
    {
      ControllerData.cmdESP_Now=cmdDown;
      SendData(ControllerData);
      if(DeliverySuccess)
      {  //Controller should respond with current potADC value.  Wait for result
        preMillis=millis();
        if(!NewData)
        {
          Serial.println("waiting...");
          while(millis()-preMillis<timeoutMillis)
          {
            if(NewData)
            {
              NewData=false;
              break;
            }
          } 
        }
        Serial.print("ADC Value = ");Serial.println(ControllerData.potADC);
        if(ControllerData.potADC<500 || ControllerData.potADC>3500)
        {
          CalPageNum=12;
          printCalPages(CalPageNum);
          return;
        }
        CalDataInProcess[CalPageNum-2]=ControllerData.potADC;
        delay(250);
        LastDemandTime=millis();
      }
    }
  }
  
}

void setup() 
{
  Serial.begin(115200);
  prepareLittleFS();
  getFileData(); // Get the system mode from the file system

  if(OTAMode)
  {
    initWiFi();
    initOTA();
    // Define the route for the "/ADC" endpoint
    server.on("/CAL", HTTP_GET, handle_CAL); // Use the handle_ADC function
    server.on("/MAC", HTTP_GET, handle_MAC); // Send the MAC address as a response
    // Start the server
    server.begin();
    Serial.println("HTTP server started");
  }else{
    initESP_NOW();
  }

  Wire.begin(I2C_SDA, I2C_SCL); // Initialize I2C with custom SDA and SCL pins

  pinMode(UpButton, INPUT); // Set UpButton pin as input
  pinMode(DownButton, INPUT); // Set DownButton pin as input
  pinMode(CalButton, INPUT_PULLUP); // Set CalButton pin as input
  pinMode(OTAButton, INPUT_PULLUP); // Set OTAButton pin as input

  //Use ext1 as a wake-up source
  esp_sleep_enable_ext1_wakeup_io(bitmask, ESP_EXT1_WAKEUP_ANY_LOW);
  // enable pull-down resistors and disable pull-up resistors
  rtc_gpio_pulldown_dis(UpButton);
  rtc_gpio_pullup_en(UpButton);
  rtc_gpio_pulldown_dis(DownButton);
  rtc_gpio_pullup_en(DownButton);
  
    
  u8g2.begin();
  //bootCount++;

  drawStartPage();

  pinMode(OTAButton, INPUT_PULLUP); // Set OTAButton pin as input
  attachInterrupt(digitalPinToInterrupt(OTAButton), OTAButtonPress, FALLING); // Attach interrupt to OTAButton

  pinMode(CalButton, INPUT_PULLUP); // Set OTAButton pin as input
  attachInterrupt(digitalPinToInterrupt(CalButton), CalButtonPress, FALLING); // Attach interrupt to CalButton

  pinMode(BattPin, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_0db);

  LastIdleTime=millis();
/**********WiFi Server Begin *****/
/*
  myServer.on("/LEVEL", handle_Level);
  myServer.on("/CAL",handle_Cal);
  myServer.begin();
*/
}

void loop() 
{
  ArduinoOTA.handle();  //handles Over The Air updates
  if(OTAMode) {server.handleClient();} // Handle incoming client requests

  if(updateOTA) // If OTA update is needed
  {
    updateOTA=false; // Reset the flag
    File myFile=LittleFS.open("/OTAdata.txt",FILE_WRITE);
    myFile.write((byte *)&OTAMode, sizeof(OTAMode));
    myFile.close();
    ESP.restart(); // Restart the ESP32 to apply changes
  }

if (!OTAMode && !CalMode) // If the system mode is normal
{
  normalOps();
} else if(!OTAMode && CalMode){
  // we need to calibrate data:
  CalOps();
}
 
}