#include "Definitions.h"
#include <Arduino.h>    

//**Modes of operation variables */

volatile bool OTAMode=false; // Flag to check if OTA mode is activated
volatile bool updateOTA=false; // Flag to check if OTA update is needed
volatile bool CalMode=false;
uint32_t LastOTAPress=0; // Last time the OTA button was pressed
uint32_t PressInterval=500; // Interval to check for OTA button press
uint32_t LastCalPress=0; // Last time the Cal button was pressed
uint32_t LastDemandTime=0;// Last time up or down flow button pressed
bool DemandButtonPressed=false;//An up or down demand button was pressed
bool SleepPermmissive=false;//flag to allow to go to sleep
uint32_t preMillis=0; //  Previous Millis, used to wait for ESP-NOW data
uint32_t timeoutMillis=30000; //time to wait for new data once sent.
uint32_t LastIdleTime=0; //Last time idle time was set
uint32_t IdleInterval=15000;//Idle time before sleeping
uint32_t LastEnterActive=0;
uint32_t EnterDelayInterval=1000;
uint8_t BattState=0;


bool DeliverySuccess=false; // Flag to check if the data was delivered successfully
bool NewData=false; //flag to check if we've recieved a new data command
bool EnterActive=true; //flag to allow up and down buttons to activate and "Enter" command
uint16_t CalPageNum=1; //current calibration page number

uint16_t CalData[17]; //array to hold calibration data
uint16_t CalDataInProcess[9];//array to hold the measured Cal. data before storing
float O2Flow = 0.0; // Initial value of O2Flow
float O2FlowLast = 0.0; // Last value of O2Flow
unsigned long previousMillis = 0; // Store the last time O2Flow was updated
const long interval = 1000; // Interval for updates (1 second)

bool FirstDraw = true; // Flag to indicate if it's the first draw

DataStruct ControllerData={0,0}; //Data structure to hold data from the controller


