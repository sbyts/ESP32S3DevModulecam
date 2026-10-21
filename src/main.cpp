#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <esp_http_server.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "global_config.h"  // Глобальний міст для передачі змінних
#include "ConfigManager.h"   // Чисті C-style обгортки ініціалізації веб-сервера
// === ДОДАЙТЕ ЦІ ДВА РЯДКИ СЮДИ (Виділяємо пам'ять під змінні в головному файлі) ===
String global_cam_resolution = "VGA";
String global_cam_brightness = "0";

#define PIR_PIN          3  
#define OLED_SDA         2  
#define OLED_SCL         1  
#define SCREEN_WIDTH   128 
#define SCREEN_HEIGHT   64 
#define OLED_RESET      -1 

// === РОЗПИНОВКА КАМЕРИ ===
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM    13

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ГЛОБАЛЬНІ СТАТУСИ СИСТЕМИ ДЛЯ ІКОНОК
bool camOk = false;
bool wifiOk = false;
bool motionDetected = false;

// Змінні для програмного фільтра датчика руху
unsigned long lastDebounceTime = 0;  
unsigned long debounceDelay = 800;    
int lastPirState = LOW;
int currentPirState = LOW;
bool isCalibrated = false;

// === БІТМАПИ ІКОНОК (Розмір 16х16 пікселів) ===
// 1. Іконка Wi-Fi Підключено
const unsigned char epd_bitmap_wifi_on [] PROGMEM = {
	0x03, 0xc0, 0x0f, 0xf0, 0x3c, 0x3c, 0x70, 0x0e, 0xc3, 0xc3, 0x0f, 0xf0, 0x1c, 0x38, 0x30, 0x0c, 
	0x03, 0xc0, 0x07, 0xe0, 0x00, 0x00, 0x01, 0x80, 0x03, 0xc0, 0x03, 0xc0, 0x01, 0x80, 0x01, 0x80
};
// 2. Іконка Wi-Fi Помилка
const unsigned char epd_bitmap_wifi_off [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x0c, 0x30, 0x06, 0x60, 0x03, 0xc0, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 
	0x01, 0x80, 0x01, 0x80, 0x03, 0xc0, 0x06, 0x60, 0x0c, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// 3. Іконка Камера OK
const unsigned char epd_bitmap_cam_on [] PROGMEM = {
	0x00, 0x00, 0x3f, 0xfc, 0x60, 0x06, 0x40, 0x02, 0x47, 0xe2, 0x4c, 0x32, 0x58, 0x1a, 0x50, 0x0a, 
	0x50, 0x0a, 0x58, 0x1a, 0x4c, 0x32, 0x47, 0xe2, 0x40, 0x02, 0x60, 0x06, 0x3f, 0xfc, 0x00, 0x00
};
// 4. Іконка Камера Помилка
const unsigned char epd_bitmap_cam_off [] PROGMEM = {
	0x00, 0x00, 0x3f, 0xfc, 0x60, 0x06, 0x40, 0x02, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 
	0x41, 0x82, 0x40, 0x02, 0x41, 0x82, 0x40, 0x02, 0x41, 0x82, 0x60, 0x06, 0x3f, 0xfc, 0x00, 0x00
};
// 5. Іконка Рух Виявлено (Людина, що біжить)
const unsigned char epd_bitmap_motion_on [] PROGMEM = {
	0x01, 0x80, 0x03, 0xc0, 0x03, 0xc0, 0x01, 0x80, 0x0f, 0xfe, 0x1c, 0x70, 0x38, 0x38, 0x30, 0x18, 
	0x1b, 0xd0, 0x0f, 0xe0, 0x07, 0xc0, 0x0e, 0xe0, 0x1c, 0x70, 0x38, 0x38, 0x70, 0x1c, 0xe0, 0x0f
};
// 6. Іконка Руху Немає (Спокій)
const unsigned char epd_bitmap_motion_off [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x07, 0xe0, 0x0e, 0x70, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 
	0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0e, 0x70, 0x07, 0xe0, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00
};

// --- ФУНКЦІЯ ОНОВЛЕННЯ ЕКРАНА ---
void updateScreen(String ipAddress) {
  display.clearDisplay();
  
  // 1. Малюємо верхню статусну панель (Іконки)
  if (wifiOk) display.drawBitmap(15, 5, epd_bitmap_wifi_on, 16, 16, SSD1306_WHITE);
  else display.drawBitmap(15, 5, epd_bitmap_wifi_off, 16, 16, SSD1306_WHITE);
  
  if (camOk) display.drawBitmap(56, 5, epd_bitmap_cam_on, 16, 16, SSD1306_WHITE);
  else display.drawBitmap(56, 5, epd_bitmap_cam_off, 16, 16, SSD1306_WHITE);
  
  if (motionDetected) display.drawBitmap(96, 5, epd_bitmap_motion_on, 16, 16, SSD1306_WHITE);
  else display.drawBitmap(96, 5, epd_bitmap_motion_off, 16, 16, SSD1306_WHITE);
  
  // Текстові підписи під іконками
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(12, 25); display.print("NET");
  display.setCursor(53, 25); display.print("CAM");
  display.setCursor(93, 25); display.print("PIR");

  // Роздільна лінія між статусами та IP адресою
  display.drawFastHLine(0, 38, 128, SSD1306_WHITE);
  
  // 2. Рядок з IP адресою / Інформація про Точку Доступу
  display.setCursor(5, 48);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("IP: " + ipAddress);
  } else if (WiFi.getMode() == WIFI_AP) {
    display.print("AP: 192.168.4.1:81"); // Вказуємо порт адмінки на екрані
  } else {
    display.print("IP: DISCONNECTED");
  }
  
  display.display();
}

// Handler для запиту фото з камери
esp_err_t photo_handler(httpd_req_t *req) {
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

// Запуск вбудованого стрімінг-сервера камери (Порт 80)
void startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;
  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t photo_uri = { .uri = "/photo", .method = HTTP_GET, .handler = photo_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &photo_uri);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(PIR_PIN, INPUT_PULLDOWN);

  Wire.begin(OLED_SDA, OLED_SCL, 400000);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while(true);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 25);
  display.print("LOADING SYSTEM...");
  display.display();

  // 1. ЗАПУСК БЕЗПЕЧНОГО СЕРВЕРА КОНФІГУРАЦІЇ (Працює ізольовано на порту 81)
  initConfigManager();

  // 2. ДИНАМІЧНА ІНІЦІАЛІЗАЦІЯ КАМЕРИ НА ОСНОВІ ДАНИХ З ВЕБ-ПАНЕЛІ
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM; config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramInit() && psramFound()){
    config.fb_location = CAMERA_FB_IN_PSRAM; config.fb_count = 2; config.jpeg_quality = 10;
    // Підставляємо налаштування якості з глобального мосту
    if (global_cam_resolution == "UXGA") config.frame_size = FRAMESIZE_UXGA;
    else if (global_cam_resolution == "CIF") config.frame_size = FRAMESIZE_CIF;
    else config.frame_size = FRAMESIZE_VGA;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM; config.fb_count = 1; config.jpeg_quality = 12;
    config.frame_size = FRAMESIZE_SVGA;
  }
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  esp_err_t err = esp_camera_init(&config);
  if (err == ESP_OK) {
    camOk = true; 
    // Застосовуємо яскравість камери з глобального мосту
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
      s->set_brightness(s, global_cam_brightness.toInt());
    }
  }

  // 3. АНАЛІЗ ПІДКЛЮЧЕННЯ ТА ЗАПУСК СТРІМІНГУ НА ПОРТУ 80
  String ipStr = "";
  if (WiFi.status() == WL_CONNECTED) {
    wifiOk = true;
    ipStr = WiFi.localIP().toString();
    startServer(); 
  } else {
    wifiOk = false;
    ipStr = WiFi.softAPIP().toString();
  }

  updateScreen(ipStr);

  // === ТАЙМЕР КАЛІБРУВАННЯ PIR ===
  for (int i = 5; i > 0; i--) { 
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 15);
    display.print("CALIBRATING PIR");
    display.setCursor(45, 35);
    display.print(i);
    display.print(" sec");
    display.display();
    delay(1000);
  }
  isCalibrated = true;
  updateScreen(ipStr);
}

void loop() {
  if (isCalibrated) {
    int reading = digitalRead(PIR_PIN);

    if (reading != lastPirState) {
      lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != currentPirState) {
        currentPirState = reading;
        motionDetected = (currentPirState == HIGH);
        // Динамічно оновлюємо іконку датчика на екрані при детекції руху
        updateScreen(wifiOk ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
      }
    }
    lastPirState = reading;
  }
}