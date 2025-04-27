#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <Arduino.h>
#include <U8g2lib.h> // Include the U8g2 library for graphics


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


#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex needed for RTC IO


#define cmdUp 3  //Command from the remoe to go up one step
#define cmdDown 4 //Command from the remote to go down one step
#define cmdGoTo 5 //Command from the remote to go to a specific value
#define cmdStatus 6  //command to send status to Remote



//**Modes of operation variables */

extern volatile bool OTAMode; // Flag to check if OTA mode is activated
extern volatile bool updateOTA; // Flag to check if OTA update is needed
extern volatile bool CalMode;

//Timing variables
extern uint32_t LastOTAPress; // Last time the OTA button was pressed
extern uint32_t PressInterval; // Interval to check for OTA button press
extern uint32_t LastCalPress; // Last time the Cal button was pressed
extern uint32_t LastDemandTime;// Last time up or down flow button pressed
extern uint32_t preMillis; //  Previous Millis, used to wait for ESP-NOW data
extern uint32_t timeoutMillis; //time to wait for new data once sent.
extern uint32_t LastIdleTime; //Last time idle time was set
extern uint32_t IdleInterval;//Idle time before sleeping
extern uint32_t LastEnterActive;
extern uint32_t EnterDelayInterval;
extern unsigned long previousMillis; // Store the last time O2Flow was updated
extern const long interval; // Interval for updates (1 second)

//Flags for button presses
extern bool DemandButtonPressed;//An up or down demand button was pressed
extern bool SleepPermmissive;//flag to allow to go to sleep

extern uint8_t BattState;

extern bool DeliverySuccess; // Flag to check if the data was delivered successfully
extern bool NewData; //flag to check if we've recieved a new data command
extern bool EnterActive; //flag to allow up and down buttons to activate and "Enter" command
extern uint16_t CalPageNum; //current calibration page number

extern uint16_t CalData[17]; //array to hold calibration data
extern uint16_t CalDataInProcess[9];//array to hold the measured Cal. data before storing
extern float O2Flow; // Initial value of O2Flow
extern float O2FlowLast; // Last value of O2Flow


extern bool FirstDraw; // Flag to indicate if it's the first draw

struct DataStruct
{
  uint8_t cmdESP_Now;  //Command for ESP-NOW
  uint16_t potADC; //ADC Value
};

extern DataStruct ControllerData; //Data structure to hold data from the controller

#endif // DEFINITIONS_H