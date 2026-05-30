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
![ESP32 C3 Mini to ST7735R Wiring Diagram](images/WIRINGDGRM1.png)

#### Reference Pin Map

| Physical Component | Component Pin Label | Wire to ESP32-C3 Mini Pin |  |
| --- | --- | --- | --- |
| **1.8" TFT (ST7735R)** | **VCC** | **`3.3V`** |
|  | **GND** | **`GND`** |
|  | **CS** | **`2`** |
|  | **RESET / RST** | **`1`** |
|  | **DC / RS** | **`3`** |
|  | **SDA / MOSI** | **`6`** |
|  | **SCL / SCK** | **`4`** |
|  | **LED / BL** | **`3.3V`** |
| **HMX Xinhai Switches** | **Prev Switch Pin 1** | **`5`** | 
| *(Orientation doesn't* | **Play Switch Pin 1** | **`7`** | 
| *matter for switches)* | **Next Switch Pin 1** | **`TX` (GPIO 21)** | 
|  | **ALL Switch Pin 2s** | **`GND`** | 
| **Rotary Encoder** | **CLK (or A)** | **`0`** | 
|  | **DT (or B)** | **`10`** |  |
|  | **SW (Button Pin 1)** | **`RX` (GPIO 20)** | 
|  | **GND & Button Pin 2** | **`GND`** | 

---


## Bill of Materials (BOM)

| Name | Purpose | Qty | Total (USD) | Distributor |
| :--- | :--- | :---: | :---: | :--- |
| **Rotary Encoder Module** | FOR VOLUME CONTROL | 1 | $0.45 | ROBOCRAZE |
| **Breadboard Jumper Wires** | For connecting the pin headers of the ESP to the display and the key switches | 1 | $1.33 | ROBOCRAZE |
| **USB C data cable** | For connecting the display to my pc | 1 | $0.71 | Robocraze |
| **Keycaps** | For the keys for playing, pausing etc etc. | 1 | $1.04 | MECKEYS |
| **Key Switches** | For adding the play, pause functionality (HMX Xinhai 63.5g) | 1 | $3.38 | MECKEYS |
| **1.8 inch TFT LCD Module** | The Display for actually showing the now playing status | 1 | $2.92 | Robocraze |
| **ESP32-C3 Mini Development Board - Unsoldered** | Main board for actually making the display work by fetching the data from spotify and also controlling what the display shows. | 1 | $2.65 | Robocraze |
| **SANDPAPER** | FOR SANDING AND SMOOTHING THE CASE FOR BETTER ADHESION. | 1 | $3.63 | Amazon |

---

### Additional Costs Summary

* **Tax (USD):** $1.37
* **Shipping (USD):** $1.56
* **Grand Total (USD):** $19.04
