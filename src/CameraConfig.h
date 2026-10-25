#pragma once
#include <Arduino.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include "global_config.h"
#include <LoggerManager.h>

extern bool camOk;

// 🔥 ВИПРАВЛЕНО: Чітко вказуємо, що static_rgb_buf — це зовнішній покажчик на масив uint8_t
extern uint8_t *static_rgb_buf;

inline bool initCameraHardware() {
    LoggerManager::log("[Camera] Налаштування пінів OV2640...");
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
    config.pixel_format = PIXFORMAT_JPEG; // Зберігаємо формат JPEG для стрімінгу
    
    if(psramInit() && psramFound()){
        LoggerManager::log("[Camera] Апаратну пам'ять PSRAM знайдено успішно.");
        config.fb_location = CAMERA_FB_IN_PSRAM; 
        config.fb_count = 2; 
        config.jpeg_quality = 10;
        
        if (global_cam_resolution == "UXGA") config.frame_size = FRAMESIZE_UXGA;
        else if (global_cam_resolution == "CIF") config.frame_size = FRAMESIZE_CIF;
        else config.frame_size = FRAMESIZE_VGA;

        // Безпечне виділення пам'яті в зовнішній PSRAM (без переповнення dram0)
        static_rgb_buf = (uint8_t *)heap_caps_malloc(640 * 480 * 2, MALLOC_CAP_SPIRAM);
        if (static_rgb_buf) {
            LoggerManager::log("[TinyML] Статичний буфер декодера успішно виділено в PSRAM.");
        } else {
            LoggerManager::log("[CRITICAL ERROR] Помилка алокації буфера в PSRAM!");
        }
    } else {
        LoggerManager::log("[WARNING] PSRAM не знайдено! Перехід у режим DRAM (Обмеження пам'яті).");
        config.fb_location = CAMERA_FB_IN_DRAM; 
        config.fb_count = 1; 
        config.jpeg_quality = 12;
        config.frame_size = FRAMESIZE_SVGA;
    }
    config.grab_mode = CAMERA_GRAB_LATEST;
    
    LoggerManager::log("[Camera] Ініціалізація сенсора камери...");
    if (esp_camera_init(&config) == ESP_OK) {
        camOk = true;
        LoggerManager::log("[Camera] Камеру успішно запущено.");
        sensor_t * s = esp_camera_sensor_get();
        if (s) s->set_brightness(s, global_cam_brightness.toInt());
        return true;
    } else {
        LoggerManager::log("[ERROR] Не вдалося ініціалізувати камеру OV2640!");
        camOk = false;
        return false;
    }
}