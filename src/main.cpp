#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "LoggerManager.h"

#define LOG_MAX_ROWS 20
#define LOG_ROW_SIZE 80 

// Підключаємо обробку TinyML та залізну конфігурацію камери
#include "CameraProcessor.h"
#include "CameraConfig.h"

// Підключення решти модулів проєкту
#include "global_config.h"  
#include "camera_pins.h"
#include "camera_server.h"
#include "ConfigManager.h"   
#include "DisplayManager.h"
#include "CloudManager.h"

// Виділення пам'яті під глобальні конфіги та системний м'ютекс
SemaphoreHandle_t cameraMutex = NULL;

// 🔥 ДОДАЙТЕ ЦЕЙ РЯДОК СЮДИ: Реальне виділення покажчика в RAM пристрою
uint8_t *static_rgb_buf = nullptr;

String global_cam_resolution = "VGA";
String global_cam_brightness = "0";
String global_tg_token = "";
String global_tg_chat_id = "";
String global_drive_enable = "0";

#define PIR_PIN          3  

bool camOk = false;
bool wifiOk = false;
bool motionDetected = false;

unsigned long lastDebounceTime = 0;  
unsigned long debounceDelay = 800;    
int lastPirState = LOW;
int currentPirState = LOW;
bool isCalibrated = false;

void setup() {
    Serial.begin(115200);
    delay(1000);
    LoggerManager::init();
    LoggerManager::log("[System] Старт мікроконтролера ESP32-S3...");
    
    // Створення об'єкта захисту
    cameraMutex = xSemaphoreCreateMutex();
    pinMode(PIR_PIN, INPUT_PULLDOWN);

    // Ініціалізація OLED екрана
    initDisplay();

    // Запуск менеджера конфігурацій (Завантаження Flash, старт Wi-Fi/AP та адмінки на Порту 81)
    LoggerManager::log("[System] Запуск ConfigManager...");
    initConfigManager();

    // Запуск апаратної камери та створення буфера в PSRAM
    initCameraHardware();

    // Мережева перевірка та запуск стрімінг-сервера камери
    if (WiFi.status() == WL_CONNECTED) {
        wifiOk = true;
        LoggerManager::log("[Network] Підключено до Wi-Fi. IP: " + WiFi.localIP().toString());
        startCameraServer(); // Старт сервера камери на порту 80 (Містить /photo та /capture_raw)
    } else {
        wifiOk = false;
        LoggerManager::log("[Network] Wi-Fi не знайдено. Активовано точку доступу AP.");
    }

    updateScreen(wifiOk, camOk, motionDetected);

    // Калібрування апаратного PIR-датчика руху
    LoggerManager::log("[PIR] Очікування стабілизации датчика...");
    for (int i = 5; i > 0; i--) { 
        showCalibration(i);
        delay(1000);
    }
    isCalibrated = true;
    LoggerManager::log("[System] Система повністю готова до роботи!");
    updateScreen(wifiOk, camOk, motionDetected);
}

void loop() {
    if (!isCalibrated) return;

    int reading = digitalRead(PIR_PIN);
    if (reading != lastPirState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != currentPirState) {
            currentPirState = reading;
            motionDetected = (currentPirState == HIGH);
            
            if (motionDetected) {
                LoggerManager::log("[Security] Виявлено рух датчиком PIR!");
                
                // ⏳ Даємо руці 500 мс, щоб зайти в кадр
                delay(500); 
                
                // 📸 РОБИМО СЕРІЮ З 3 КАДРІВ, щоб гарантовано зловити жест в динаміці
                for (int shot = 1; shot <= 3; shot++) {
                    LoggerManager::log("[TinyML] Сканування жесту (Кадр " + String(shot) + "/3)...");
                    capture_and_process_frame(); 
                    
                    // Коротка пауза між кадрами серії
                    delay(300); 
                }
            } else {
                LoggerManager::log("[Security] Рух припинився, спокійний режим.");
            }
            
            updateScreen(wifiOk, camOk, motionDetected);
            
            if (motionDetected && wifiOk) {
                if (global_drive_enable == "1") {
                    LoggerManager::log("[Cloud] Перемикач хмари активний, надсилаємо фото...");
                    sendPhotoToGoogleDrive();
                }
            }
        }
    }
    lastPirState = reading;
}