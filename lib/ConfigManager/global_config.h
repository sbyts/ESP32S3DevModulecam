#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#include <Arduino.h>

extern String global_cam_resolution;
extern String global_cam_brightness;
extern String global_tg_token;
extern String global_tg_chat_id;

// Конфігурація циклічного буфера логів
#define LOG_MAX_ROWS 20
#define LOG_ROW_SIZE 80 // Максимальна довжина одного рядка логу

extern char sys_logs[LOG_MAX_ROWS][LOG_ROW_SIZE];
extern int log_index;

// Макрос/Функція для логування замість Serial.println
void logger(String message);

// Нова змінна: "1" — увімкнено, "0" — вимкнено
extern String global_drive_enable;

#endif