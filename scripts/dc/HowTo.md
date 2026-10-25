# TinyML Dataset Collector for ESP32-S3 Cube

This Python script connects to your **ESP32-S3 IoT Autonomous Security Cube** via a dedicated raw HTTP endpoint, captures processed **96x96 Grayscale** image frames from the OV2640 camera, and saves them automatically into structured folders to build a dataset for training TinyML models (e.g., Rock-Paper-Scissors).

---

## Prerequisites & Installation

Before running the script, ensure you have Python 3 installed on your system. You will need to install the required external libraries for handling HTTP requests and image processing.

Run the following command in your terminal to install the dependencies:

```bash
pip install requests pillow
```

---

## How to Use

1. Ensure your ESP32-S3 Cube is powered on and connected to the same Wi-Fi network as your computer.
2. Run the script from your terminal.

### Default Execution
If your device is running on the default IP `192.168.1.100` and port `82`, simply execute:

```bash
python dataset_collector.py
```

### Custom IP and Port
If your router assigned a different IP address to the Cube or you modified the server port in your ESP32 code, pass them as arguments:

```bash
python dataset_collector.py --ip 192.168.0.15 --port 80
```

---

## Interactive Menu & Workflow

Once started, the script will display an interactive menu in the console:

```text
=== DATASET COLLECTOR FOR CUBE [192.168.1.100:82] ===
1. Collect ROCK (rock)
2. Collect SCISSORS (scissors)
3. Collect PAPER (paper)
4. Exit
```

1. Select a gesture by typing the corresponding number (`1`, `2`, or `3`) and press **Enter**.
2. The console will display a **3-second countdown**. Use this time to position your hand in front of the camera.
3. The script will automatically trigger the ESP32 camera and download **100 frames** (taking roughly 15-20 seconds).
4. **Tips for a robust dataset:** While the script is running, slowly rotate your hand, change the distance from the lens, and slightly vary the lighting conditions.

---

## Dataset Structure

The script automatically creates a `dataset/` directory in the same folder where it is executed. Images are stored as lossless `.png` files with unique timestamps under their respective subfolders:

```text
📁 your-project-folder/
│
├── 📄 dataset_collector.py
└── 📁 dataset/
    ├── 📁 rock/
    │   ├── rock_1711785600123.png
    │   └── ...
    ├── 📁 scissors/
    │   ├── scissors_1711785645678.png
    │   └── ...
    └── 📁 paper/
        ├── paper_1711785710294.png
        └── ...
```

Once you have gathered enough images (typically 100-200 per class), this entire `dataset/` folder is ready to be zipped and uploaded directly into **Edge Impulse** or any other TinyML training pipeline!