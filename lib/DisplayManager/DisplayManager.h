#pragma once
#include <Arduino.h>

#define OLED_SDA         2  
#define OLED_SCL         1  
#define SCREEN_WIDTH   128 
#define SCREEN_HEIGHT   64 
#define OLED_RESET      -1 

void initDisplay();
void updateScreen(bool wifiOk, bool camOk, bool motionDetected);
void showCalibration(int secondsLeft);