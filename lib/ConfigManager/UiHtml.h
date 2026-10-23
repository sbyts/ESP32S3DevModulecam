// lib/ConfigManager/UiHtml.h
#pragma once
#include <pgmspace.h>

const char index_html[] PROGMEM = R"rawliteral(
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
            --accent: #fbbf24;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-color); min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 20px; }
        .container { width: 100%; max-width: 600px; background-color: var(--card-color); border: 1px solid var(--border-color); border-radius: 16px; padding: 24px; box-shadow: 0 25px 50px -12px rgba(0,0,0,0.5); }
        .header { text-align: center; margin-bottom: 24px; }
        .header h1 { font-size: 24px; font-weight: 700; background: linear-gradient(to right, #60a5fa, #34d399); -webkit-background-clip: text; -webkit-text-fill-color: transparent; margin-bottom: 4px; }
        .header p { color: var(--text-muted); font-size: 14px; }
        
        /* Вкладки */
        .tabs-nav { display: flex; gap: 8px; margin-bottom: 20px; border-bottom: 1px solid var(--border-color); padding-bottom: 12px; }
        .tab-btn { flex: 1; background-color: #334155; color: var(--text-muted); border: 1px solid var(--border-color); padding: 10px; cursor: pointer; border-radius: 8px; font-size: 14px; font-weight: 600; transition: all 0.2s; outline: none; }
        .tab-btn:hover { background-color: #475569; color: var(--text-color); }
        .tab-btn.active { background: linear-gradient(to right, #3b82f6, #10b981); color: white; border: none; }
        .tab-content { display: none; }
        .tab-content.active-content { display: block; }

        .section { background-color: rgba(15, 23, 42, 0.4); border: 1px solid rgba(51, 65, 85, 0.5); border-radius: 12px; padding: 16px; margin-bottom: 16px; }
        .section-title { font-size: 12px; font-weight: 600; letter-spacing: 1px; text-transform: uppercase; margin-bottom: 12px; }
        .wifi-title { color: #60a5fa; }
        .cam-title { color: #34d399; }
        .cloud-title { color: #a855f7; }
        .security-title { color: #fbbf24; }
        .form-group { margin-bottom: 12px; }
        label { display: block; font-size: 12px; color: var(--text-muted); margin-bottom: 4px; }
        input[type="text"], input[type="password"], input[type="number"], select { width: 100%; background-color: var(--input-color); border: 1px solid var(--border-color); border-radius: 8px; padding: 10px 12px; color: var(--text-color); font-size: 14px; outline: none; }
        input:focus, select:focus { border-color: var(--primary); }
        
        /* Тумблер */
        .switch-group { display: flex; align-items: center; justify-content: space-between; padding-top: 4px; }
        .switch { position: relative; display: inline-block; width: 44px; height: 24px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #334155; transition: .3s; border-radius: 24px; }
        .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
        input:checked + .slider { background-color: var(--secondary); }
        input:checked + .slider:before { transform: translateX(20px); }

        /* Телеметрія та логи */
        .status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 16px; }
        .status-card { background: var(--input-color); border: 1px solid var(--border-color); padding: 12px; border-radius: 8px; }
        .status-card .val { font-size: 16px; font-weight: 700; color: var(--secondary); margin-top: 4px; }
        #log-output { background-color: var(--input-color); color: #22c55e; padding: 14px; border-radius: 8px; overflow-x: auto; font-family: monospace; font-size: 13px; text-align: left; max-height: 350px; overflow-y: auto; white-space: pre-wrap; border: 1px solid var(--border-color); }

        .btn { width: 100%; background: linear-gradient(to right, #3b82f6, #10b981); color: white; border: none; border-radius: 8px; padding: 12px; font-size: 14px; font-weight: 500; cursor: pointer; transition: opacity 0.2s; margin-top: 8px; box-shadow: 0 4px 12px rgba(59, 130, 246, 0.3); }
        .btn:hover { opacity: 0.9; }
        .btn-sec { background: #334155; box-shadow: none; margin-top: 12px; border: 1px solid var(--border-color); }
        .btn-sec:hover { background: #475569; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ESP32-S3 Camera Admin</h1>
            <p>Конфігурація та аналітика системи</p>
        </div>

        <!-- Навігація вкладок -->
        <div class="tabs-nav">
            <button class="tab-btn active" onclick="openTab(event, 'settings')">Налаштування</button>
            <button class="tab-btn" onclick="openTab(event, 'status')">Система</button>
            <button class="tab-btn" onclick="openTab(event, 'logs')">Логи</button>
        </div>

        <!-- ВКЛАДКА 1: НАЛАШТУВАННЯ -->
        <div id="settings" class="tab-content active-content">
            <form action="/save" method="POST">
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
                <div class="section">
                    <div class="section-title cloud-title">Шлюз Автоматизації (n8n / Webhook)</div>
                    <div class="form-group switch-group">
                        <label style="margin-bottom: 0; font-size: 13px;">Активація відправки фото</label>
                        <label class="switch">
                            <input type="checkbox" name="drive_en" value="1" %DRIVE_CHECKED%>
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="form-group" style="margin-top: 12px;">
                        <label>Webhook URL (n8n Шлюз)</label>
                        <input type="text" name="tgtoken" value="%TG_TOKEN%">
                    </div>
                </div>
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

        <!-- ВКЛАДКА 2: СИСТЕМА -->
        <div id="status" class="tab-content">
            <div class="section">
                <div class="section-title wifi-title">Стан пристрою</div>
                <div class="status-grid">
                    <div class="status-card"><label>Uptime системи</label><div class="val" id="uptime">0 сек</div></div>
                    <div class="status-card"><label>Сигнал Wi-Fi</label><div class="val" id="wifi_rssi">0 dBm</div></div>
                    <div class="status-card"><label>Вільна RAM (Heap)</label><div class="val" id="free_ram">0 KB</div></div>
                    <div class="status-card"><label>Вільна PSRAM</label><div class="val" id="free_psram">0 KB</div></div>
                    <div class="status-card"><label>Частота CPU</label><div class="val" id="cpu_mhz">0 MHz</div></div>
                    <div class="status-card"><label>Min Ever Free RAM</label><div class="val" id="min_ram">0 KB</div></div>
                </div>
                <button onclick="refreshStatus()" class="btn">Оновити дані</button>
            </div>
        </div>

        <!-- ВКЛАДКА 3: ЛОГИ -->
        <div id="logs" class="tab-content">
            <div class="section">
                <div class="section-title cloud-title">Останні 20 подій (Serial Logs)</div>
                <pre id="log-output">Завантаження логів...</pre>
                <button onclick="refreshLogs()" class="btn btn-sec">Оновити логи</button>
            </div>
        </div>
    </div>

    <script>
        let statusInterval = null;

        function openTab(evt, tabId) {
            // Ховаємо контент усіх вкладок
            document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active-content'));
            // Знімаємо активний клас зі всіх кнопок
            document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active'));
            
            // Активуємо обрану вкладку
            document.getElementById(tabId).classList.add('active-content');
            if(evt) {
                evt.currentTarget.classList.add('active');
            }
            
            clearInterval(statusInterval);
            
            if (tabId === 'status') {
                refreshStatus();
                statusInterval = setInterval(refreshStatus, 300000); // 5 хвилин фонового режиму
            } else if (tabId === 'logs') {
                refreshLogs();
            }
        }

        function refreshStatus() {
            fetch('/api/status')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('uptime').innerText = data.uptime + " сек";
                    document.getElementById('wifi_rssi').innerText = data.wifi_rssi + " dBm";
                    document.getElementById('free_ram').innerText = (data.free_ram / 1024).toFixed(1) + " KB";
                    document.getElementById('free_psram').innerText = (data.psram / 1024).toFixed(1) + " KB";
                    document.getElementById('cpu_mhz').innerText = data.cpu_mhz + " MHz";
                    document.getElementById('min_ram').innerText = (data.min_ram / 1024).toFixed(1) + " KB";
                })
                .catch(() => { console.log("Помилка телеметрії API"); });
        }

        function refreshLogs() {
            const out = document.getElementById('log-output');
            out.innerText = "Оновлення логів з ESP32-S3...";
            fetch('/api/logs')
                .then(res => res.json())
                .then(data => {
                    out.innerText = data.length ? data.join('\n') : "Буфер логів порожній.";
                    out.scrollTop = out.scrollHeight; // Автоматичний скрол до свіжих рядків
                })
                .catch(() => { out.innerText = "Не вдалося зв'язатися з ендпоінтом логів."; });
        }
    </script>
</body>
</html>
)rawliteral";