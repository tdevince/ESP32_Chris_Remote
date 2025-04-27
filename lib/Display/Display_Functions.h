#ifndef DISPLAY_FUNC_H
#define DISPLAY_FUNC_H

#include <Arduino.h>
#include <U8g2lib.h> // Include the U8g2 library for graphics
#include "Definitions.h"


// Function prototypes
float interpolateData(uint16_t inputValue);
void drawStartPage();
void printCalPages(uint16_t pageNum);
int readADCVolts();
void drawBattery();

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;


#endif // DISPLAY_FUNC_H