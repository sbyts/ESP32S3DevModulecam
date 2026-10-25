#pragma once
#include <Arduino.h>

#define LOG_MAX_ROWS 20
#define LOG_ROW_SIZE 80

class LoggerManager {
private:
    static char sys_logs[LOG_MAX_ROWS][LOG_ROW_SIZE];
    static int log_index;

public:
    // Ініціалізація (якщо знадобиться очистити буфер на старті)
    static void init();

    // Головна функція логування
    static void log(const String& message);

    // Додаткові методи для отримання доступу до логів з Web API (ендпоінт /api/logs)
    static const char* getLogRow(int index);
    static int getLogIndex();
};