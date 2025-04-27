#include <Arduino.h>
#include <WiFi.h>
#include "ESPNOW_Func.h"
#include "Definitions.h"


//Creates a varaible called peerInfo of the data structury type esp_now_peer_info_t to 
//hold information about the peer
esp_now_peer_info_t peerInfo; 

uint8_t ControllerAddress[]={0x68, 0xB6, 0xB3, 0x08, 0xD7, 0x6A}; //MAC address of Chris Controller

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
 * @brief Get the current value from the Controller 
 */
void getControllerStatus()
{
  ControllerData.cmdESP_Now=cmdStatus;
  SendData(ControllerData);
}


 