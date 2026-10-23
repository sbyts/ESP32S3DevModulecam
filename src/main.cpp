// src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"

// 1. Конфігурація циклічного буфера логів (Виправлено: оголошено перед використанням)
#define LOG_MAX_ROWS 20
#define LOG_ROW_SIZE 80 

char sys_logs[LOG_MAX_ROWS][LOG_ROW_SIZE] = {0};
int log_index = 0;

void logger(String message) {
    // Дублюємо в реальний апаратний Serial порт
    Serial.println(message);
    
    // Записуємо в статичний циклічний масив, затираючи старі записи по колу
    strncpy(sys_logs[log_index], message.c_str(), LOG_ROW_SIZE - 1);
    sys_logs[log_index][LOG_ROW_SIZE - 1] = '\0'; // Гарантований нуль-термінатор для JSON
    
    log_index = (log_index + 1) % LOG_MAX_ROWS;
}

// 2. Підключення решти модулів проєкту
#include "global_config.h"  
#include "camera_pins.h"
#include "camera_server.h"
#include "ConfigManager.h"   
#include "DisplayManager.h"
#include "CloudManager.h"

// Виділення пам'яті під глобальні конфіги
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
    logger("[System] Старт мікроконтролера ESP32-S3...");
    
    pinMode(PIR_PIN, INPUT_PULLDOWN);

    // Ініціалізація OLED екрана
    initDisplay();

    // Запуск менеджера конфігурацій (Завантаження Flash, старт Wi-Fi/AP та адмінки на Порту 81)
    logger("[System] Запуск ConfigManager...");
    initConfigManager();

    // Конфігурація матриці камери
    logger("[Camera] Налаштування пінів OV2640...");
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0; 
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; 
    config.pin_d1 = Y3_GPIO_NUM; 
    config.pin_d2 = Y4_GPIO_NUM; 
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; 
    config.pin_d5 = Y7_GPIO_NUM; 
    config.pin_d6 = Y8_GPIO_NUM; 
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; 
    config.pin_pclk = PCLK_GPIO_NUM; 
    config.pin_vsync = VSYNC_GPIO_NUM; 
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; 
    config.pin_sccb_scl = SIOC_GPIO_NUM; 
    config.pin_pwdn = PWDN_GPIO_NUM; 
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 10000000; 
    config.pixel_format = PIXFORMAT_JPEG;
    
    // Перевірка наявності апаратної пам'яті PSRAM
    if(psramInit() && psramFound()){
        logger("[Camera] Апаратну пам'ять PSRAM знайдено успішно.");
        config.fb_location = CAMERA_FB_IN_PSRAM; 
        config.fb_count = 2; 
        config.jpeg_quality = 10;
        
        if (global_cam_resolution == "UXGA") config.frame_size = FRAMESIZE_UXGA;
        else if (global_cam_resolution == "CIF") config.frame_size = FRAMESIZE_CIF;
        else config.frame_size = FRAMESIZE_VGA;
    } else {
        logger("[WARNING] PSRAM не знайдено! Перехід у режим DRAM (Обмеження пам'яті).");
        config.fb_location = CAMERA_FB_IN_DRAM; 
        config.fb_count = 1; 
        config.jpeg_quality = 12;
        config.frame_size = FRAMESIZE_SVGA;
    }
    config.grab_mode = CAMERA_GRAB_LATEST;
    
    logger("[Camera] Ініціалізація сенсора камери...");
    if (esp_camera_init(&config) == ESP_OK) {
        camOk = true;
        logger("[Camera] Камеру успішно запущено.");
        sensor_t * s = esp_camera_sensor_get();
        if (s) s->set_brightness(s, global_cam_brightness.toInt());
    } else {
        logger("[ERROR] Не вдалося ініціалізувати камеру OV2640!");
    }

    // Мережева перевірка та запуск стрімінг-сервера камери
    if (WiFi.status() == WL_CONNECTED) {
        wifiOk = true;
        logger("[Network] Підключено до Wi-Fi. IP: " + WiFi.localIP().toString());
        startCameraServer(); // Старт сервера камери на порту 80
    } else {
        wifiOk = false;
        logger("[Network] Wi-Fi не знайдено. Активовано точку доступу AP: 192.168.4.1");
    }

    updateScreen(wifiOk, camOk, motionDetected);

    // Калібрування апаратного PIR-датчика руху
    logger("[PIR] Очікування стабілізації датчика...");
    for (int i = 5; i > 0; i--) { 
        showCalibration(i);
        delay(1000);
    }
    isCalibrated = true;
    logger("[System] Система повністю готова до роботи!");
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
                logger("[Security] Виявлено рух датчиком PIR!");
            } else {
                logger("[Security] Рух припинився, спокійний режим.");
            }
            
            // Миттєво оновлюємо іконки на фізичному екрані
            updateScreen(wifiOk, camOk, motionDetected);
            
            // Перевірка умов відправки знімка на Google Диск
            if (motionDetected && wifiOk) {
                if (global_drive_enable == "1") {
                    logger("[Cloud] Перемикач хмари активний, надсилаємо фото...");
                    sendPhotoToGoogleDrive();
                } else {
                    logger("[Cloud] Рух є, але відправку на GDrive вимкнено.");
                }
            }
        }
    }
    lastPirState = reading;
}