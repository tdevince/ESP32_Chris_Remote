#ifndef ESPNOW_FUNC_H
#define ESPNOW_FUNC_H

#include <Arduino.h>
#include <esp_now.h>
#include "Definitions.h"

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len);
void SendData(DataStruct OutData);
void initESP_NOW();
void getControllerStatus();

#endif // ESPNOW_FUNC_H