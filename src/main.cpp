#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <esp_http_server.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiClientSecure.h>
#include <base64.h> 
#include "global_config.h"  
#include "ConfigManager.h"   

// Виділяємо пам'ять під глобальні змінні
String global_cam_resolution = "VGA";
String global_cam_brightness = "0";
String global_tg_token = "";
String global_tg_chat_id = "";
String global_drive_enable = "0"; // За замовчуванням вимкнено

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

bool camOk = false;
bool wifiOk = false;
bool motionDetected = false;

unsigned long lastDebounceTime = 0;  
unsigned long debounceDelay = 800;    
int lastPirState = LOW;
int currentPirState = LOW;
bool isCalibrated = false;

// === БІТМАПИ ІКОНОК (16х16) ===
const unsigned char epd_bitmap_wifi_on [] PROGMEM = {
	0x03, 0xc0, 0x0f, 0xf0, 0x3c, 0x3c, 0x70, 0x0e, 0xc3, 0xc3, 0x0f, 0xf0, 0x1c, 0x38, 0x30, 0x0c, 
	0x03, 0xc0, 0x07, 0xe0, 0x00, 0x00, 0x01, 0x80, 0x03, 0xc0, 0x03, 0xc0, 0x01, 0x80, 0x01, 0x80
};
const unsigned char epd_bitmap_wifi_off [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x0c, 0x30, 0x06, 0x60, 0x03, 0xc0, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 
	0x01, 0x80, 0x01, 0x80, 0x03, 0xc0, 0x06, 0x60, 0x0c, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char epd_bitmap_cam_on [] PROGMEM = {
	0x00, 0x00, 0x3f, 0xfc, 0x60, 0x06, 0x40, 0x02, 0x47, 0xe2, 0x4c, 0x32, 0x58, 0x1a, 0x50, 0x0a, 
	0x50, 0x0a, 0x58, 0x1a, 0x4c, 0x32, 0x47, 0xe2, 0x40, 0x02, 0x60, 0x06, 0x3f, 0xfc, 0x00, 0x00
};
const unsigned char epd_bitmap_cam_off [] PROGMEM = {
	0x00, 0x00, 0x3f, 0xfc, 0x60, 0x06, 0x40, 0x02, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 
	0x41, 0x82, 0x40, 0x02, 0x41, 0x82, 0x40, 0x02, 0x41, 0x82, 0x60, 0x06, 0x3f, 0xfc, 0x00, 0x00
};
const unsigned char epd_bitmap_motion_on [] PROGMEM = {
	0x01, 0x80, 0x03, 0xc0, 0x03, 0xc0, 0x01, 0x80, 0x0f, 0xfe, 0x1c, 0x70, 0x38, 0x38, 0x30, 0x18, 
	0x1b, 0xd0, 0x0f, 0xe0, 0x07, 0xc0, 0x0e, 0xe0, 0x1c, 0x70, 0x38, 0x38, 0x70, 0x1c, 0xe0, 0x0f
};
const unsigned char epd_bitmap_motion_off [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x07, 0xe0, 0x0e, 0x70, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0e, 0x70, 0x07, 0xe0, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00
};

void updateScreen(String ipAddress) {
  display.clearDisplay();
  
  if (wifiOk) display.drawBitmap(15, 5, epd_bitmap_wifi_on, 16, 16, SSD1306_WHITE);
  else display.drawBitmap(15, 5, epd_bitmap_wifi_off, 16, 16, SSD1306_WHITE);
  
  if (camOk) display.drawBitmap(56, 5, epd_bitmap_cam_on, 16, 16, SSD1306_WHITE);
  else display.drawBitmap(56, 5, epd_bitmap_cam_off, 16, 16, SSD1306_WHITE);
  
  if (motionDetected) display.drawBitmap(96, 5, epd_bitmap_motion_on, 16, 16, SSD1306_WHITE);
  else display.drawBitmap(96, 5, epd_bitmap_motion_off, 16, 16, SSD1306_WHITE);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(12, 25); display.print("NET");
  display.setCursor(53, 25); display.print("CAM");
  display.setCursor(93, 25); display.print("PIR");

  display.drawFastHLine(0, 38, 128, SSD1306_WHITE);
  
  display.setCursor(5, 48);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("IP: " + ipAddress);
  } else if (WiFi.getMode() == WIFI_AP) {
    display.print("AP: 192.168.4.1:81");
  } else {
    display.print("IP: DISCONNECTED");
  }
  
  display.display();
}

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

void sendPhotoToGoogleDrive() {
  if (global_tg_token == "") {
    Serial.println("[Webhook] URL-адреса шлюзу порожня. Відміна.");
    return;
  }

  Serial.println("[Webhook] Захоплення чистого кадру JPEG з камери...");
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("[Webhook] Не вдалося отримати кадр.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Вимикаємо перевірку сертифіката для економії пам'яті RAM

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

  Serial.print("[Webhook] Підключення до сервера: "); Serial.println(host);
  
  if (client.connect(host.c_str(), 443)) {
    Serial.println("[Webhook] Підключено! Надсилаємо оригінальне бінарне фото...");

    // Надсилаємо чистий POST запит без будь-яких Base64 кодувань
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
    
    Serial.println("[Webhook] Файл успішно передано на шлюз!");
    
    // Перевіряємо відповідь сервера (очікуємо HTTP/1.1 200 OK)
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 4000) {
        Serial.println("[Webhook] Помилка: Таймаут відповіді.");
        break;
      }
    }
    
    if (client.available()) {
      String response = client.readStringUntil('\n');
      Serial.print("[Webhook] Сервер відповів: "); Serial.println(response);
    }
  } else {
    Serial.println("[Webhook] Помилка: Не вдалося з'єднатися зі шлюзом.");
  }
  
  esp_camera_fb_return(fb);
  client.stop();
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

  // Запуск менеджера конфігурацій
  initConfigManager();

  // Налаштування камери
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM; config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramInit() && psramFound()){
    config.fb_location = CAMERA_FB_IN_PSRAM; config.fb_count = 2; config.jpeg_quality = 10;
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
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
      s->set_brightness(s, global_cam_brightness.toInt());
    }
  }

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

  for (int i = 5; i > 0; i--) { 
    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 15); display.print("CALIBRATING PIR");
    display.setCursor(45, 35); display.print(i); display.print(" sec");
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
        updateScreen(wifiOk ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
        
        // Перевіряємо увімкнення перемикача Хмари та статус інтернету
        if (motionDetected && wifiOk && global_drive_enable == "1") {
          sendPhotoToGoogleDrive();
        }
      }
    }
    lastPirState = reading;
  }
}