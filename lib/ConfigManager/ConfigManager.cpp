#include <WiFi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include "ConfigManager.h"
#include "../../src/global_config.h"

static AsyncWebServer server(81);
static Preferences prefs;
static String wifi_ssid;
static String wifi_pass;
static String admin_user;
static String admin_pass;

// ЧАСТИНА 1: Статичний HTML та CSS стилі (Безпечно віддаються браузеру)
const char index_html_css[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 Admin Panel</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-color: #1e293b;
            --input-color: #0f172a;
            --text-color: #f8fafc;
            --text-muted: #94a3b8;
            --border-color: #334155;
            --primary: #3b82f6;
            --secondary: #10b981;
            --accent: #f59e0b;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-color); min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 20px; }
        .container { width: 100%; max-width: 400px; background-color: var(--card-color); border: 1px solid var(--border-color); border-radius: 16px; padding: 24px; box-shadow: 0 25px 50px -12px rgba(0,0,0,0.5); }
        .header { text-align: center; margin-bottom: 24px; }
        .header h1 { font-size: 24px; font-weight: 700; background: linear-gradient(to right, #60a5fa, #34d399); -webkit-background-clip: text; -webkit-text-fill-color: transparent; margin-bottom: 4px; }
        .header p { color: var(--text-muted); font-size: 14px; }
        .section { background-color: rgba(15, 23, 42, 0.4); border: 1px solid rgba(51, 65, 85, 0.5); border-radius: 12px; padding: 16px; margin-bottom: 16px; }
        .section-title { font-size: 12px; font-weight: 600; letter-spacing: 1px; text-transform: uppercase; margin-bottom: 12px; }
        .wifi-title { color: #60a5fa; }
        .cam-title { color: #34d399; }
        .security-title { color: #fbbf24; }
        .form-group { margin-bottom: 12px; }
        .form-group:last-child { margin-bottom: 0; }
        label { display: block; font-size: 12px; color: var(--text-muted); margin-bottom: 4px; }
        input[type="text"], input[type="password"], input[type="number"], select { width: 100%; background-color: var(--input-color); border: 1px solid var(--border-color); border-radius: 8px; padding: 10px 12px; color: var(--text-color); font-size: 14px; outline: none; transition: border-color 0.2s; }
        input:focus, select:focus { border-color: var(--primary); }
        .btn { width: 100%; background: linear-gradient(to right, #3b82f6, #10b981); color: white; border: none; border-radius: 8px; padding: 12px; font-size: 14px; font-weight: 500; cursor: pointer; transition: opacity 0.2s; margin-top: 8px; box-shadow: 0 4px 12px rgba(59, 130, 246, 0.3); }
        .btn:hover { opacity: 0.9; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ESP32-S3 Camera Admin</h1>
            <p>Конфігурація модулів</p>
        </div>
)rawliteral";

// ЧАСТИНА 2: Форма, куди за допомогою коду підставляються змінні
const char index_html_form[] PROGMEM = R"rawliteral(
        <form action="/save" method="POST">
            <!-- Секція Wi-Fi -->
            <div class="section">
                <div class="section-title wifi-title">Налаштування Wi-Fi</div>
                <div class="form-group">
                    <label>Назва мережі (SSID)</label>
                    <input type="text" name="ssid" value="%WIFI_SSID%">
                </div>
                <div class="form-group">
                    <label>Пароль Wi-Fi</label>
                    <input type="password" name="wifipass" value="%WIFI_PASS%">
                </div>
            </div>
            <!-- Секція Камери -->
            <div class="section">
                <div class="section-title cam-title">Параметри камери</div>
                <div class="form-group">
                    <label>Роздільна здатність</label>
                    <select name="resolution">
                        <option value="UXGA" %SEL_UXGA%>UXGA (1600x1200)</option>
                        <option value="VGA" %SEL_VGA%>VGA (640x480)</option>
                        <option value="CIF" %SEL_CIF%>CIF (400x296)</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>Яскравість (-2 до 2)</label>
                    <input type="number" name="brightness" min="-2" max="2" value="%BRIGHTNESS%">
                </div>
            </div>
            <!-- Секція Адмінки -->
            <div class="section">
                <div class="section-title security-title">Безпека Panel</div>
                <div class="form-group">
                    <label>Новий Логін</label>
                    <input type="text" name="adminuser" value="%ADMIN_USER%">
                </div>
                <div class="form-group">
                    <label>Новий Пароль</label>
                    <input type="password" name="adminpass" value="%ADMIN_PASS%">
                </div>
            </div>
            <button type="submit" class="btn">Зберегти та перезавантажити</button>
        </form>
    </div>
</body>
</html>
)rawliteral";

static void loadFromFlash() {
    prefs.begin("system_cfg", true);
    wifi_ssid = prefs.getString("ssid", "");
    wifi_pass = prefs.getString("wifipass", "");
    admin_user = prefs.getString("adminuser", "admin");
    admin_pass = prefs.getString("adminpass", "admin123");
    global_cam_resolution = prefs.getString("res", "VGA");
    global_cam_brightness = prefs.getString("bright", "0");
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
    prefs.end();
}

void setupRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->authenticate(admin_user.c_str(), admin_pass.c_str())){
            return request->requestAuthentication();
        }
        
        // ВИПРАВЛЕНО: Явно вказуємо правильний тип AsyncResponseStream* замість базового AsyncWebServerResponse*
        AsyncResponseStream *response = request->beginResponseStream("text/html");
        
        // Відправляємо статичні CSS стилі
        response->print(index_html_css);
        
        // Робимо швидку ручну заміну змінних у формі без залучення важкого парсера бібліотеки
        String formWithData = index_html_form;
        formWithData.replace("%WIFI_SSID%", wifi_ssid);
        formWithData.replace("%WIFI_PASS%", wifi_pass);
        formWithData.replace("%ADMIN_USER%", admin_user);
        formWithData.replace("%ADMIN_PASS%", admin_pass);
        formWithData.replace("%BRIGHTNESS%", global_cam_brightness);
        formWithData.replace("%SEL_UXGA%", (global_cam_resolution == "UXGA") ? "selected" : "");
        formWithData.replace("%SEL_VGA%", (global_cam_resolution == "VGA") ? "selected" : "");
        formWithData.replace("%SEL_CIF%", (global_cam_resolution == "CIF") ? "selected" : "");
        
        // Відправляємо готову форму
        response->print(formWithData);
        request->send(response);
    });

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

        saveToFlash();

        request->send(200, "text/plain", "Налаштування збережено успішно! ESP32-S3 перезавантажується...");
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