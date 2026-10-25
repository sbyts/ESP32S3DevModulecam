# How to Automate PlatformIO Pipeline in VS Code

This guide explains how to configure and run a full **Clean ➡️ Build ➡️ Upload ➡️ Monitor** routine in a single action within VS Code, bypassing Windows `CommandNotFoundException` errors.

---

## 1. Configure the Custom Task

To avoid typing long commands and bypass system path limitations, create a custom VS Code build task.

1. Inside your PlatformIO project root, open or create the `.vscode` folder.
2. Inside that folder, create or open the `tasks.json` file.
3. Paste the following configuration, which automatically targets the correct absolute path of `pio.exe` across different operating systems:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "type": "shell",
            "label": "PlatformIO: Clean, Build, Upload & Monitor",
            "command": "~/.platformio/penv/Scripts/pio run -t clean && ~/.platformio/penv/Scripts/pio run && ~/.platformio/penv/Scripts/pio run -t upload && ~/.platformio/penv/Scripts/pio device monitor",
            "windows": {
                "command": "env:HOMEDRIVE{env:HOMEPATH}\\.platformio\\penv\\Scripts\\pio.exe run -t clean ; env:HOMEDRIVE{env:HOMEPATH}\\.platformio\\penv\\Scripts\\pio.exe run ; env:HOMEDRIVE{env:HOMEPATH}\\.platformio\\penv\\Scripts\\pio.exe run -t upload ; env:HOMEDRIVE{env:HOMEPATH}\\.platformio\\penv\\Scripts\\pio.exe device monitor"
            },
            "problemMatcher": [
                "\$platformio"
            ],
            "presentation": {
                "focus": true,
                "panel": "new"
            },
            "group": {
                "kind": "build",
                "isDefault": true
            }
        }
    ]
}
```

---

## 2. Run the Full Automation Pipeline

Once the file is saved, you no longer need to trigger tasks manually from the PlatformIO menu.

* Simply press **`Ctrl + Shift + B`** (or `Cmd + Shift + B` on macOS).
* VS Code will instantly open a new terminal panel and execute all 4 steps sequentially:
  1. **Clean:** Wipes build artifacts and cache.
  2. **Build:** Compiles your project from scratch.
  3. **Upload:** Flashes the newly generated binary onto your microcontroller.
  4. **Monitor:** Starts the serial monitor immediately to capture live logs.