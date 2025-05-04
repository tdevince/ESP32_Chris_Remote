#include <Arduino.h>
#include <NetworkUdp.h>
#include <WebServer.h>
#include <WiFiManager.h> 
#include <driver/rtc_io.h> //Use this library for RTC IO Sleep/Wakeup
#include <Wire.h> // Include the Wire library for I2C communication
#include "Definitions.h"
#include "Display_Functions.h"
#include "ESPNOW_Func.h"
#include "WiFiOTAHelper.h"

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

/************WiFi Variables************** */
String ESPHostName="Chris_Remote";
WebServer server(80); // Create an instance of the web server on port 80
WiFiManager wifiManager;  //start wifiManager


/*Controller MAC address.  Set this to the MAC address of the ESP32-S2_Solo-2 module used
* in the Controller
*/
uint8_t ControllerAddress[6]={0x68, 0xB6, 0xB3, 0x09, 0x1C, 0x34}; //MAC address of Chris Controller, Test
//uint8_t ControllerAddress[6] = {0x68, 0xB6, 0xB3, 0x08, 0xD7, 0x6A}; //MAC address of Chris Controller, production


// Define bitmask for multiple GPIOs for RTC wakeup
uint64_t bitmask = BUTTON_PIN_BITMASK(DownButton) | BUTTON_PIN_BITMASK(UpButton);
//RTC_DATA_ATTR int bootCount = 0; //RTC_DATA_ATTR is used to store data in RTC memory
//RTC memory is retained over deep sleep and reboots

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
    if (!CalMode){ESP.restart();} // Restart the ESP32 to abort changes
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
  prepareLittleFS(FORMAT_LITTLEFS_IF_FAILED);
  getFileData(); // Get the system mode from the file system

  if(OTAMode)
  {
    initWiFi(ESPHostName, wifiManager);
    initOTA();
    // Define the route for the "/CAL and MAC" endpoint
    server.on("/CAL", HTTP_GET, handle_CAL); // Use the handle_ADC function
    server.on("/MAC", HTTP_GET, handle_MAC); // Send the MAC address as a response
    // Start the server
    server.begin();
    Serial.println("HTTP server started");
  }else{ //if not in OTA mode, start ESPNOW
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