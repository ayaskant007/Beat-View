# BEAT VIEW SPOTIFY DISPLAY

My very own desktop Spotify display + controller powered by a C3 MINI ESP32 and a ST77XX TFT LCD screen it has 3 mechanical switches to let me control the music playback directly
from my desk without constantly switching windows in my computer.

## Why This Project Was Made

I made this to solve the annyoance of always having to switch tabs/windows and interrupting my work just to skip a song or pause it, so i was just randomly one day decided to look into the stasis starter projects and i seemed to love the idea to built something to fix it so i made it.

## Project Media

### 3D Model Render
![Fusion 360 3D Case Model](images/main.webp)

### PCB Layout
*Everything is handwired and doesnt use a PCB*

### Wiring Diagram
![ESP32 C3 Mini to ST7735R Wiring Diagram](images/WIRINGDGRM.png)

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

| Name | Purpose | Qty | Total (USD) | Distributor |
| --- | --- | --- | --- | --- |
| **3D PRINTED CASE** | for 3d printing the case to actually put everything inside | 1 | $5.00 | Printing Legion! |
| **Breadboard Jumper Wires** | For connecting the pin headers of the ESP to the display and the key switches | 1 | $1.96 | Amazon |
| **USB C data cable** | For connecting the display to my pc | 1 | $1.34 | Amazon |
| **Soldering Iron Kit** | For soldering the pin headers on the esp etc. | 1 | $7.74 | Amazon |
| **Keycaps** | For the keys for playing, pausing etc etc. | 1 | $13.55 | StacksKB |
| **Keys** | For adding the play, pause functionality | 1 | $3.91 | StacksKB |
| **1.8 inch TFT LCD Module** | The Display for actually showing the now playing status | 1 | $2.91 | Robocraze |
| **ESP32-C3 Mini Development Board - Unsoldered** | Main board for actually making the display work by fetching the data from spotify and also controlling what the display shows. | 1 | $2.65 | Robocraze (India) |

---

### Additional Costs Summary

* **Tax (USD):** $3.59
* **Shipping (USD):** $0.56
* **Grand Total (USD):** $43.21
