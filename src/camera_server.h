#pragma once
#include <esp_http_server.h>
#include "esp_camera.h"

// Константи розміру для TinyML нейромережі
#define EI_CLASSIFIER_INPUT_WIDTH  96
#define EI_CLASSIFIER_INPUT_HEIGHT 96

// Вказуємо компілятору, що ці об'єкти виділені та описані у main.cpp
extern uint8_t out_buf[EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT];
extern bool capture_and_process_frame();

// 1. Чинний обробник для звичайних JPEG фото
inline esp_err_t photo_handler(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb); 
    return res;
}

// 2. Новий обробник сирих Grayscale байтів 96x96 для Python-скрипта
inline esp_err_t raw_capture_handler(httpd_req_t *req) {
    if (!capture_and_process_frame()) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"); // Для запобігання CORS-блокувань
    esp_err_t res = httpd_resp_send(req, (const char *)out_buf, EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT);
    
    // Невелика системна пауза для стабілізації RTOS перед наступним HTTP запитом
    vTaskDelay(pdMS_TO_TICKS(50)); 
    
    return res;
}

// 3. Функція ініціалізації сервера на Порту 80
inline void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80; 
    config.ctrl_port = 32768;
    
    // Трохи збільшуємо ліміт URI-хендлерів, щоб гарантовано вмістити новий ендпоінт
    config.max_uri_handlers = 8; 

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        // Реєстрація старого ендпоінту /photo
        httpd_uri_t photo_uri = { .uri = "/photo", .method = HTTP_GET, .handler = photo_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &photo_uri);
        
        // Реєстрація нового ендпоінту /capture_raw
        httpd_uri_t raw_uri = { .uri = "/capture_raw", .method = HTTP_GET, .handler = raw_capture_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &raw_uri);

        Serial.println("[Server] Стрімінг камери та RAW ендпоінт запущено на порту 80");
    }
}