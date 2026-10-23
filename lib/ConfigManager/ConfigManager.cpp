#include <WiFi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include "ConfigManager.h"
#include "global_config.h"
#include "UiHtml.h" // Підключаємо наш HTML інтерфейс

static AsyncWebServer server(81);
static Preferences prefs;
static String wifi_ssid;
static String wifi_pass;
static String admin_user;
static String admin_pass;

// Зв'язок із масивом логів із main.cpp
extern char sys_logs[LOG_MAX_ROWS][LOG_ROW_SIZE];
extern int log_index;

static void loadFromFlash() {
    prefs.begin("system_cfg", true);
    wifi_ssid = prefs.getString("ssid", "");
    wifi_pass = prefs.getString("wifipass", "");
    admin_user = prefs.getString("adminuser", "admin");
    admin_pass = prefs.getString("adminpass", "admin123");
    global_cam_resolution = prefs.getString("res", "VGA");
    global_cam_brightness = prefs.getString("bright", "0");
    global_tg_token = prefs.getString("tgtoken", "");
    global_drive_enable = prefs.getString("d_en", "0");
    prefs.end();
}

static void saveToFlash() {
    prefs.begin("system_cfg", false);
    prefs.putString("ssid", wifi_ssid);
    prefs.putString("wifipass", wifi_pass);
    prefs.putString("adminuser", admin_user);
    prefs.putString("adminpass", admin_pass);
    prefs.putString("res", global_cam_resolution);
    prefs.putString("bright", global_cam_brightness);
    prefs.putString("tgtoken", global_tg_token);
    prefs.putString("d_en", global_drive_enable);
    prefs.end();
}

static void setupRoutes() {
    // 1. Головна веб-сторінка адмінки (Генерація темплейту)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->authenticate(admin_user.c_str(), admin_pass.c_str())){
            return request->requestAuthentication();
        }
        
        AsyncResponseStream *response = request->beginResponseStream("text/html");
        String formWithData = index_html;
        
        formWithData.replace("%WIFI_SSID%", wifi_ssid);
        formWithData.replace("%WIFI_PASS%", wifi_pass);
        formWithData.replace("%ADMIN_USER%", admin_user);
        formWithData.replace("%ADMIN_PASS%", admin_pass);
        formWithData.replace("%BRIGHTNESS%", global_cam_brightness);
        formWithData.replace("%TG_TOKEN%", global_tg_token);
        formWithData.replace("%DRIVE_CHECKED%", (global_drive_enable == "1") ? "checked" : "");
        
        formWithData.replace("%SEL_UXGA%", (global_cam_resolution == "UXGA") ? "selected" : "");
        formWithData.replace("%SEL_VGA%", (global_cam_resolution == "VGA") ? "selected" : "");
        formWithData.replace("%SEL_CIF%", (global_cam_resolution == "CIF") ? "selected" : "");
        
        response->print(formWithData);
        request->send(response);
    });

    // 2. API Телеметрії системи (On-Demand JSON)
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->authenticate(admin_user.c_str(), admin_pass.c_str())){
            return request->send(401);
        }
        String json = "{";
        json += "\"uptime\":" + String(millis() / 1000) + ",";
        json += "\"free_ram\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"min_ram\":" + String(ESP.getMinFreeHeap()) + ",";
        json += "\"psram\":" + String(ESP.getFreePsram()) + ",";
        json += "\"wifi_rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
        json += "\"cpu_mhz\":" + String(ESP.getCpuFreqMHz());
        json += "}";
        request->send(200, "application/json", json);
    });

    // 3. API Читання Циклічного Буфера Логів
    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->authenticate(admin_user.c_str(), admin_pass.c_str())){
            return request->send(401);
        }
        String json = "[";
        int start = log_index;
        bool first = true;
        
        for (int i = 0; i < LOG_MAX_ROWS; i++) {
            int idx = (start + i) % LOG_MAX_ROWS;
            if (strlen(sys_logs[idx]) > 0) {
                if (!first) json += ",";
                String escapedLog = String(sys_logs[idx]);
                escapedLog.replace("\\", "\\\\");
                escapedLog.replace("\"", "\\\"");
                json += "\"" + escapedLog + "\"";
                first = false;
            }
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    // 4. Обробка збереження форми налаштувань
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!request->authenticate(admin_user.c_str(), admin_pass.c_str())){
            return request->requestAuthentication();
        }

        wifi_ssid = request->arg("ssid");
        wifi_pass = request->arg("wifipass");
        admin_user = request->arg("adminuser");
        admin_pass = request->arg("adminpass");
        global_cam_resolution = request->arg("resolution");
        global_cam_brightness = request->arg("brightness");
        global_tg_token = request->arg("tgtoken");
        global_drive_enable = request->hasArg("drive_en") ? "1" : "0";
        
        saveToFlash();
        request->send(200, "text/plain", "Налаштування збережено! Перезавантаження ESP32-S3...");
        delay(2000);
        ESP.restart();
    });
}

void initConfigManager() {
    loadFromFlash();
    if (wifi_ssid != "") {
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        int counter = 0;
        while (WiFi.status() != WL_CONNECTED && counter < 20) {
            delay(500);
            counter++;
        }
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-S3-CAM-AP", NULL);
    }
    setupRoutes();
    server.begin();
}