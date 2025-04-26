#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <Arduino.h>

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex needed for RTC IO


#define cmdUp 3  //Command from the remoe to go up one step
#define cmdDown 4 //Command from the remote to go down one step
#define cmdGoTo 5 //Command from the remote to go to a specific value
#define cmdStatus 6  //command to send status to Remote

//**Modes of operation variables */

extern volatile bool OTAMode; // Flag to check if OTA mode is activated
extern volatile bool updateOTA; // Flag to check if OTA update is needed
extern volatile bool CalMode;
extern uint32_t LastOTAPress; // Last time the OTA button was pressed
extern uint32_t PressInterval; // Interval to check for OTA button press
extern uint32_t LastCalPress; // Last time the Cal button was pressed
extern uint32_t LastDemandTime;// Last time up or down flow button pressed
extern bool UpButtonPressed;
extern bool DownButtonPressed;
extern bool DemandButtonPressed;//An up or down demand button was pressed
extern bool SleepPermmissive;//flag to allow to go to sleep
extern uint32_t preMillis; //  Previous Millis, used to wait for ESP-NOW data
extern uint32_t timeoutMillis; //time to wait for new data once sent.
extern uint32_t LastIdleTime; //Last time idle time was set
extern uint32_t IdleInterval;//Idle time before sleeping
extern uint32_t LastEnterActive;
extern uint32_t EnterDelayInterval;
extern uint8_t BattState;


extern bool DeliverySuccess; // Flag to check if the data was delivered successfully
extern bool NewData; //flag to check if we've recieved a new data command
extern bool EnterActive; //flag to allow up and down buttons to activate and "Enter" command
extern uint16_t CalPageNum; //current calibration page number

extern uint16_t CalData[17]; //array to hold calibration data
extern uint16_t CalDataInProcess[9];//array to hold the measured Cal. data before storing
extern float O2Flow; // Initial value of O2Flow
extern float O2FlowLast; // Last value of O2Flow
extern unsigned long previousMillis; // Store the last time O2Flow was updated
extern const long interval; // Interval for updates (1 second)

extern bool FirstDraw; // Flag to indicate if it's the first draw

struct DataStruct
{
  uint8_t cmdESP_Now;  //Command for ESP-NOW
  uint16_t potADC; //ADC Value
};

extern DataStruct ControllerData; //Data structure to hold data from the controller

#endif // DEFINITIONS_H