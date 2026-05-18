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

| Component Pin | ESP32-C3 Mini GPIO | KiCad Net Name | Configuration / Notes |
| :--- | :---: | :--- | :--- |
| **TFT LCD VCC** | **3.3V** | `+3V3` | Main logic power |
| **TFT LCD GND** | **GND** | `GND` | Ground reference |
| **TFT LCD CS** | **GPIO 1** | `TFT_CS` | Chip Select |
| **TFT LCD RESET** | **GPIO 2** | `TFT_RST` | Hardware Reset |
| **TFT LCD DC** | **GPIO 3** | `TFT_DC` | Data / Command Select |
| **TFT LCD SCK (SCLK)**| **GPIO 4** | `SPI_SCK` | Software/Hardware SPI Clock |
| **TFT LCD SDA (MOSI)**| **GPIO 5** | `SPI_MOSI` | Software/Hardware SPI Data |
| **TFT LCD LED** | **3.3V** | `+3V3` | Backlight (Always-on, max brightness) |
| | | | |
| **Left Switch (Prev)**| **GPIO 6** | `BTN_PREV` | Connects pin to GND when pressed |
| **Middle Switch (Play)**| **GPIO 7** | `BTN_PLAY` | Connects pin to GND when pressed |
| **Right Switch (Next)**| **GPIO 8** | `BTN_NEXT` | Connects pin to GND when pressed |
| | | | |
| **Encoder Module CLK**| **GPIO 0** | `ENC_CLK` | Quadrature Output A |
| **Encoder Module DT** | **GPIO 10** | `ENC_DT` | Quadrature Output B |
| **Encoder Module SW** | **GPIO 20** | `ENC_SW` | Push Button Click |
| **Encoder Module +** | **3.3V** | `+3V3` | Powers onboard pull-up resistors |
| **Encoder Module GND**| **GND** | `GND` | Shared ground reference |


## Bill of Materials (BOM)

| Name | Purpose | Qty | Total (USD) | Distributor |
| :--- | :--- | :---: | :---: | :--- |
| **Rotary Encoder Module** | FOR VOLUME CONTROL | 1 | $0.45 | ROBOCRAZE |
| **3D PRINTED CASE** | for 3d printing the case to actually put everything inside | 1 | $5.00 | Printing Legion! |
| **Breadboard Jumper Wires** | For connecting the pin headers of the ESP to the display and the key switches | 1 | $1.33 | ROBOCRAZE |
| **USB C data cable** | For connecting the display to my pc | 1 | $0.71 | Robocraze |
| **Keycaps** | For the keys for playing, pausing etc etc. | 1 | $1.04 | MECKEYS |
| **Key Switches** | For adding the play, pause functionality (HMX Xinhai 63.5g) | 1 | $3.38 | MECKEYS |
| **1.8 inch TFT LCD Module** | The Display for actually showing the now playing status | 1 | $2.92 | Robocraze |
| **ESP32-C3 Mini Development Board - Unsoldered** | Main board for actually making the display work by fetching the data from spotify and also controlling what the display shows. | 1 | $2.65 | Robocraze |

---

### Additional Costs Summary

* **Tax (USD):** $3.59
* **Shipping (USD):** $0.56
* **Grand Total (USD):** $21.630
