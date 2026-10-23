#include "CloudManager.h"
#include "esp_camera.h"
#include <WiFiClientSecure.h>

// Зв'язуємо модуль із глобальними змінними та функцією логування з main.cpp
extern String global_tg_token;
extern String global_drive_enable;
extern void logger(String message);

void sendPhotoToGoogleDrive() {
    if (global_tg_token == "") {
        logger("[Cloud] Помилка: URL-адреса шлюзу порожня. Відміна відправки.");
        return;
    }

    logger("[Cloud] Захоплення чистого кадру JPEG з камери...");
    camera_fb_t * fb = esp_camera_fb_get();
    if(!fb) {
        logger("[Cloud] Помилка: Не вдалося отримати кадр з OV2640!");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Вимикаємо перевірку сертифіката для максимальної економії RAM

    // Автоматично розбираємо введене посилання на Хост та Шлях
    String url = global_tg_token;
    String host = "";
    String path = "";
    
    if (url.startsWith("http://")) url = url.substring(7);
    if (url.startsWith("https://")) url = url.substring(8);
    
    int splitIndex = url.indexOf('/');
    if (splitIndex != -1) {
        host = url.substring(0, splitIndex);
        path = url.substring(splitIndex);
    } else {
        host = url;
        path = "/";
    }

    logger("[Cloud] Підключення до сервера: " + host);
    
    if (client.connect(host.c_str(), 443)) {
        logger("[Cloud] Підключено! Передаємо оригінальне бінарне фото...");

        // Надсилаємо чистий POST запит без накладних витрат на Base64
        client.println("POST " + path + " HTTP/1.1");
        client.println("Host: " + host);
        client.println("Content-Type: image/jpeg");
        client.println("Content-Length: " + String(fb->len));
        client.println("Connection: close");
        client.println(); 

        // Пишемо сирі байти фотографії з пам'яті ESP32-S3 блоками по 1024 байти
        uint8_t *fbBuf = fb->buf;
        size_t fbLen = fb->len;
        for (size_t n = 0; n < fbLen; n = n + 1024) {
            if (n + 1024 < fbLen) {
                client.write(fbBuf, 1024);
                fbBuf += 1024;
            } else if (fbLen % 1024 > 0) {
                size_t remainder = fbLen % 1024;
                client.write(fbBuf, remainder);
            }
        }
        
        logger("[Cloud] JPEG буфер успішно передано на шлюз.");
        
        // Очікуємо відповідь сервера (таймаут 4 секунди)
        unsigned long timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 4000) {
                logger("[Cloud] Помилка: Таймаут відповіді сервера.");
                break;
            }
        }
        
        if (client.available()) {
            String response = client.readStringUntil('\n');
            // Прибираємо символи переносу рядка для гарного вигляду в логах
            response.trim();
            logger("[Cloud] Відповідь Google шлюзу: " + response);
        }
    } else {
        logger("[Cloud] Помилка: Не вдалося з'єднатися зі шлюзом 443.");
    }
    
    // Обов'язково повертаємо фрейм-буфер камері, щоб уникнути витоків пам'яті
    esp_camera_fb_return(fb);
    client.stop();
}