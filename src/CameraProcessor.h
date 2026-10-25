#pragma once
#include <Arduino.h>
#include "esp_camera.h"
#include <LoggerManager.h>
#include "esp_task_wdt.h"

// Підключаємо головний заголовочний файл розгорнутої бібліотеки Edge Impulse
#include <stojary-project-2_inferencing.h> 

// Суворе вирівнювання розміру вхідного тензора з Edge Impulse SDK (9216 для Grayscale)
#define TINYML_SIGNAL_SIZE EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE

// Статичний масив float для ШІ-кадру (9216 * 4 байти = ~36 КБ)
// Розміщуємо в зовнішній 8 МБ PSRAM через макрос EXT_RAM_ATTR, щоб не забивати оперативку DRAM
EXT_RAM_ATTR static float ai_input_buffer[TINYML_SIGNAL_SIZE];

// Статичні важкі структури Edge Impulse винесені в PSRAM для обходу обмеження стеку
EXT_RAM_ATTR static signal_t signal_ai;
EXT_RAM_ATTR static ei_impulse_result_t result_ai;

// Залишаємо масив Grayscale для сумісності з сервером збору датасету Python
uint8_t out_buf[EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT];

// Покажчик на статичний буфер декодування великого кадру в PSRAM (створений в main.cpp)
extern uint8_t *static_rgb_buf;

// Системний м'ютекс для захисту камери від паралельних запитів веб-сервера
extern SemaphoreHandle_t cameraMutex;

// Глобальні змінні геометрії для точного прорахунку Nearest Neighbor у коллбеку
static int effective_w = 640;
static int effective_h = 480;
static int crop_l = 0;
static int crop_t = 0;
static uint32_t nn_scale_x = 0;
static uint32_t nn_scale_y = 0;

/**
 * Стабільний лінійний коллбек для TensorFlow Lite.
 * Копіює підготовлені float-дані з ai_input_buffer.
 */
int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    if ((offset + length) > TINYML_SIGNAL_SIZE) {
        return -1; 
    }
    
    for (size_t i = 0; i < length; i++) {
        // Читаємо сирий піксель (0..255)
        uint8_t gray_value = out_buf[offset + i];
        
        // Суворе квантування під ваш model_variables.h (zero_point = -128)
        out_ptr[i] = (float)gray_value - 128.0f; 
    }
    return 0;
}

/**
 * Захоплює JPEG кадр, масштабує його в PSRAM, робить пропорційний квадратний кроп,
 * нормалізує пікселі до квантованого INT8 діапазону, заповнює out_buf та запускає локальний ШІ.
 */
inline bool capture_and_process_frame() {
    if (!static_rgb_buf) {
        LoggerManager::log("[TinyML] Помилка: Буфер PSRAM не ініціалізовано!");
        return false;
    }

    if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        LoggerManager::log("[TinyML] Помилка: Камера зайнята!");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        LoggerManager::log("[TinyML] Помилка: Не вдалося захопити кадр!");
        xSemaphoreGive(cameraMutex);
        return false;
    }

    // Визначаємо коефіцієнт апаратного масштабування драйвера камери Espressif
    jpg_scale_t scale = JPG_SCALE_NONE; 
    effective_w = fb->width;
    effective_h = fb->height;

    if (fb->width > 800) {
        scale = JPG_SCALE_4X;
        effective_w /= 4;
        effective_h /= 4;
    } else if (fb->width > 640) {
        scale = JPG_SCALE_2X;
        effective_w /= 2;
        effective_h /= 2;
    }

    // Декодуємо апаратний JPEG безпосередньо у наш великий буфер в PSRAM
    if (!jpg2rgb565(fb->buf, fb->len, static_rgb_buf, scale)) {
        LoggerManager::log("[TinyML] Помилка: Декодування JPEG провалилося!");
        esp_camera_fb_return(fb);
        xSemaphoreGive(cameraMutex);
        return false;
    }

    // Розрахунок геометрії центрального квадратного кропу кадру
    int min_side = (effective_w < effective_h) ? effective_w : effective_h;
    crop_l = (effective_w - min_side) / 2;
    crop_t = (effective_h - min_side) / 2;

    // Крок Nearest Neighbor ресайзу у фіксованій крапці для швидкості
    nn_scale_x = (min_side << 16) / EI_CLASSIFIER_INPUT_WIDTH;
    nn_scale_y = (min_side << 16) / EI_CLASSIFIER_INPUT_HEIGHT;

    // Лінійний прохід обробки графіки
    for (int y = 0; y < EI_CLASSIFIER_INPUT_HEIGHT; y++) {
        int src_y = crop_t + ((y * nn_scale_y) >> 16);
        
        for (int x = 0; x < EI_CLASSIFIER_INPUT_WIDTH; x++) {
            int src_x = crop_l + ((x * nn_scale_x) >> 16);
            
            if (src_x >= effective_w) src_x = effective_w - 1;
            if (src_y >= effective_h) src_y = effective_h - 1;

            int src_index = ((src_y * effective_w) + src_x) * 2;

            // Читаємо піксель RGB565 (Big Endian)
            uint16_t rgb565 = (static_rgb_buf[src_index] << 8) | static_rgb_buf[src_index + 1];

            // Точне розкладання на канали (0-255)
            uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
            uint8_t b = (rgb565 & 0x1F) << 3;

            // ШІ-яскравість за індустріальним стандартом
            uint8_t gray = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);

            // Заповнюємо out_buf для сумісності з Python-сервером
            int dest_idx = (y * EI_CLASSIFIER_INPUT_WIDTH) + x;
            out_buf[dest_idx] = gray;
        }
    }

    // Звільняємо камеру для стабільної роботи мережевих потоків
    esp_camera_fb_return(fb);
    xSemaphoreGive(cameraMutex);

    // =================================================================
    // 🔥 ЗАПУСК ЛОКАЛЬНОГО ШТУЧНОГО ІНТЕЛЕКТУ В REAL-TIME
    // =================================================================
    vTaskDelay(pdMS_TO_TICKS(15));

    signal_ai.total_length = TINYML_SIGNAL_SIZE; 
    signal_ai.get_data = &get_signal_data;

    memset(&result_ai, 0, sizeof(ei_impulse_result_t));

    // Скидаємо сторожовий таймер Watchdog
    esp_task_wdt_reset();

    // Проганяємо підготовлений буфер через ядро TensorFlow
    EI_IMPULSE_ERROR ei_err = run_classifier(&signal_ai, &result_ai, false);
    if (ei_err != EI_IMPULSE_OK) {
        LoggerManager::log("[TinyML] Помилка класифікації: " + String(ei_err));
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(5));

    // --- ОПТИМІЗОВАНА ФІЛЬТРАЦІЯ ТА АНАЛІЗ РЕЗУЛЬТАТІВ ---
    String top_label = "";
    float max_score = 0.0;

    // Шукаємо клас із найбільшою ймовірністю
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result_ai.classification[ix].value > max_score) {
            max_score = result_ai.classification[ix].value;
            top_label = result_ai.classification[ix].label;
        }
    }

    // Знижуємо поріг до 55% під специфіку квантування int8
    if (max_score > 0.55) {
        if (top_label != "empty") {
            LoggerManager::log("[TinyML] ЖЕСТ: Визначено [" + top_label + "] з точністю " + String(max_score * 100, 0) + "%");
            
            // Точка підключення фізичних дій пристрою або виводу на екран
            // triggerGestureAction(top_label);
        } else {
            LoggerManager::log("[TinyML] ЖЕСТ: Порожній фон / Спокійний режим. (" + String(max_score * 100, 0) + "%)");
        }
    } else {
        // Інформативний лог для розуміння логіки софтмаксу моделі
        LoggerManager::log("[TinyML] ЖЕСТ: Сумнів розпізнавання. Найближчий: [" + top_label + "] лише " + String(max_score * 100, 0) + "%");
    }

    return true;
}