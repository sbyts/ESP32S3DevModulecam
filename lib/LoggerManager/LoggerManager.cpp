#include "LoggerManager.h"

// Явне виділення пам'яті під статичні члени класу
char LoggerManager::sys_logs[LOG_MAX_ROWS][LOG_ROW_SIZE];
int LoggerManager::log_index = 0;

void LoggerManager::init() {
    log_index = 0;
    for (int i = 0; i < LOG_MAX_ROWS; i++) {
        sys_logs[i][0] = '\0';
    }
}

void LoggerManager::log(const String& message) {
    // Дублюємо в реальний апаратний Serial порт
    Serial.println(message);
    
    // Записуємо в статичний циклічний масив, затираючи старі записи по колу
    strncpy(sys_logs[log_index], message.c_str(), LOG_ROW_SIZE - 1);
    sys_logs[log_index][LOG_ROW_SIZE - 1] = '\0'; // Гарантований нуль-термінатор для JSON
    
    log_index = (log_index + 1) % LOG_MAX_ROWS;
}

const char* LoggerManager::getLogRow(int index) {
    if (index >= 0 && index < LOG_MAX_ROWS) {
        return sys_logs[index];
    }
    return "";
}

int LoggerManager::getLogIndex() {
    return log_index;
}

String getLogsJson() {
    String json = "{\"logs\":[";
    for (int i = 0; i < LOG_MAX_ROWS; i++) {
        json += "\"" + String(LoggerManager::getLogRow(i)) + "\"";
        if (i < LOG_MAX_ROWS - 1) json += ",";
    }
    json += "]}";
    return json;
}