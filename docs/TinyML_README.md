# Edge AI / TinyML Integration Guide: ESP32-S3 N16R8 Security Cube

This document acts as a technical brief and guide for the localized **Gesture Recognition System (Rock-Paper-Scissors-Empty)** running on the **ESP32-S3 N16R8** micro-architecture. The project bridges the gap between hardware pixel-acquisition (OV2640) and on-chip Tensor Computation using **TensorFlow Lite for Microcontrollers (TFLM)** and **Edge Impulse**.

---

## 🛠️ Microprocessor & Hardware Specifications

- **SoC:** Espressif ESP32-S3 (Xtensa® 32-bit LX7 dual-core processor with vector instructions).
- **Memory Configuration:** **N16R8** (16 MB Quad SPI Flash + 8 MB Octal SPI External PSRAM).
- **Camera Sensor:** OV2640 (Configured to output hardware JPEG streams to maintain multi-module cloud compatibility).
- **AI Core Framework:** TensorFlow Lite Micro deployed via Edge Impulse C++ SDK static compilation.

---

## 🚀 Accomplished Implementation Steps

During development, the standalone architecture was systematically re-factored and hardened against memory corruption and system resets.

1. **Modular Code Refactoring:**
   - Isolated system initialization into `src/CameraConfig.h`.
   - Dedicated all math, cropping, color space conversion, and inference loops to `src/CameraProcessor.h`.
   - Cleansed `src/main.cpp` to handle strict event loops (PIR debouncing, task triggering).

2. **Static PSRAM Heap Allocation:**
   - Replaced risky multi-thread `malloc()` and `free()` runtime allocations with a dedicated global decoding pointer (`static_rgb_buf`) allocated once at startup using `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`. This completely eliminated memory fragmentation and HTTP 500 endpoint errors.

3. **Stack Overflow Mitigation:**
   - FreeRTOS defaults to an 8 KB loop stack on Arduino cores, which instantly crashes when TensorFlow allocates heavy hidden layers. 
   - Moved heavy Edge Impulse compiler objects (`signal_ai` and `result_ai`) into external PSRAM using the `EXT_RAM_ATTR` attribute, freeing up internal DRAM.

4. **Hardware Synchronization & Quantization Calibration:**
   - Decoded the embedded hardware JPEG stream into an RGB565 matrix in PSRAM, automatically downsampling high-res frames (UXGA/SVGA) using `JPG_SCALE_4X` / `JPG_SCALE_2X`.
   - Synchronized input tensors directly with the compiled **Quantized Int8 model variables** (`zero_point = -128`), resolving the blind-spot where the network falsely treated raw inputs as white noise.

---

## ⚠️ Known Caveats & Troubleshooting (Problematic Areas)

When resuming or retraining this project, pay extreme attention to the following pitfalls:

### 1. The Multi-Thread Mutex Conflict (Server vs. Inference)
Because the Python dataset collection script and the internal `loop()` inference call both target the camera sensor simultaneously, they **will** compete for the frame buffer.
* **Fix implemented:** Wrapped the entire pipeline in a binary semaphore `xSemaphoreTake(cameraMutex)`. 
* **Warning:** If an external network client hangs while pulling `/capture_raw`, the local PIR scan will block for up to 2 seconds. Keep server connections lightweight.

### 2. Watchdog Timer Reset (`TG1WDT_SYS_RST`)
Quantized 2D Convolutional Networks require millions of multiply-accumulate matrix calculations. On an ESP32-S3, this can hog Core 1 for **100–300ms consecutively**. FreeRTOS will interpret this as a freeze and trigger a hard reboot.
* **Fix implemented:** Fed the task watchdog manually using `esp_task_wdt_reset()` and introduced strategic system ticks via `vTaskDelay(pdMS_TO_TICKS(10))` before and after invoking the network.

### 3. Edge Impulse Input Normalization Discrepancies
This is the **#1 reason** why a model shows 98% accuracy on the Edge Impulse dashboard but stalls at ~25% random distribution on target hardware.
* **Grayscale Models:** Expect an absolute float input sequence matching the internal variables. Our final fix uses explicit `(float)gray_value - 128.0f` to shift the `0..255` range into the `.zero_point = -128` int8 layer.
* **MobileNet Transfer Learning:** If you switch back to MobileNet later, it will forcefully demand 3 sequential channels of float values scaled between `0.0f` and `1.0f`. Ensure your pixel loop adapts dynamically.

### 4. PIR Debounce & Motion Blur
PIR sensors trigger the exact millisecond a thermal draft or edge of a hand enters their field of view. If the camera fires instantly, it captures an empty room or a heavily motion-blurred smear.
* **Fix implemented:** Added an intentional `delay(500)` in `main.cpp` before scanning, followed by a burst of 3 sequential scans.
* **Retraining Advice:** If accuracy remains low under fast movement, collect data with *Motion Blur* intentionally included in the training dataset, and lock your web-admin camera resolution strictly to **VGA (640x480)** to prevent aspect-ratio squashing during central cropping.