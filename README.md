# BEAT VIEW SPOTIFY DISPLAY

My very own desktop Spotify display + controller powered by a C3 MINI ESP32 and a ST77XX TFT LCD screen it has 3 mechanical switches to let me control the music playback directly
from my desk without constantly switching windows in my computer.

## Why This Project Was Made

I made this to solve the annyoance of always having to switch tabs/windows and interrupting my work just to skip a song or pause it, so i was just randomly one day decided to look into the stasis starter projects and i seemed to love the idea to built something to fix it so i made it.

## Project Media

### Finished Project Pictures
![Physical Device Front View](path/to/your/finished_project_front.jpg)
![Physical Device Desk Setup](path/to/your/finished_project_desk.jpg)

### 3D Model Render
![Fusion 360 3D Case Model](path/to/your/3d_model_screenshot.png)

### PCB Layout
*Everything is handwired and doesnt use a PCB*

### Wiring Diagram
![ESP32 C3 Mini to ST7735R Wiring Diagram](path/to/your/wiring_diagram.png)

#### Reference Pin Map:
* **TFT LCD VCC** -> ESP32 3.3V / 5V
* **TFT LCD GND** -> ESP32 GND
* **TFT LCD CS** -> ESP32 GPIO 1
* **TFT LCD RESET** -> ESP32 GPIO 2
* **TFT LCD DC** -> ESP32 GPIO 3
* **TFT LCD SDA (MOSI)** -> ESP32 GPIO 5
* **TFT LCD SCK (SCLK)** -> ESP32 GPIO 4
* **TFT LCD LED** -> ESP32 3.3V
* **Left Switch (Prev)** -> ESP32 GPIO 6 & GND
* **Middle Switch (Play/Pause)** -> ESP32 GPIO 7 & GND
* **Right Switch (Next)** -> ESP32 GPIO 8 & GND

---

## Bill of Materials (BOM)

| Item | Component | Qty | Estimated Price (INR) | Primary Source |
| :--- | :--- | :--- | :--- | :--- |
| **Brain** | ESP32 C3 Mini Development Board | 1 | ₹400 | Robocraze |
| **Display** | 1.8" TFT LCD Module (ST7735R Driver) | 1 | ₹294 | Robocraze |
| **Switches** | Akko V3 Creamy Blue Pro (or equivalent) | 3 | ₹350 (Pack of 10) | StacksKB |
| **Caps** | MOG / XDA Profile Novelty Keycaps | 3 | ₹300 | StacksKB |
| **Enclosure** | Custom 3D Printed Vertical Case (PLA/PETG) | 1 | ₹200 | Self-printed / Local Hub |
| **Power** | USB-C Data & Power Cable | 1 | ₹200 | Local Electronics Shop |
| **Wiring** | 28AWG / 30AWG Silicone Hookup Wire | 1m | ₹50 | Local Electronics Shop |
| **Total** | | | **~₹1,794** | |
