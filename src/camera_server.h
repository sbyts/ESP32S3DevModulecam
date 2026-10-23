#pragma once
#include <esp_http_server.h>
#include "esp_camera.h"

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

inline void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80; 
    config.ctrl_port = 32768;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t photo_uri = { .uri = "/photo", .method = HTTP_GET, .handler = photo_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &photo_uri);
        Serial.println("[Server] Стрімінг камери запущено на порту 80");
    }
}