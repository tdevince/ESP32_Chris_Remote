#include <Arduino.h>
#include <U8g2lib.h> // Include the U8g2 library for graphics
#include <Wire.h> // Include the Wire library for I2C communication
#include <SPI.h>  // Include the SPI library for SPI communication
#include <WiFi.h>
#include "Definitions.h"
#include "Display_Functions.h"
#include "ESPNOW_Func.h"
#include <FS.h>
#include <LittleFS.h>  //File system on ESP32




U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);

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
