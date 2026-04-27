# VirtuKey: Wearable BLE Spatial Keyboard & Air Mouse 🧤💻

VirtuKey is a fully wireless, dual-mode smart glove powered by an ESP32. It completely replaces a traditional physical keyboard and mouse by tracking the precise strikes, spreads, and tilts of your fingers in mid-air. By pressing a physical toggle button, the system seamlessly switches between a spatial QWERTY keyboard and a zero-drift precision Air Mouse using a single Bluetooth Combo profile.

## 📸 Overall Project (The Hackpad)
Here is what the fully assembled VirtuKey looks like in action:
![Virtukey_Typing](https://github.com/user-attachments/assets/43e0db0e-4812-446f-810b-2d02bc3ffca6)

*The completed wearable setup showing the sensor placement and the real-time OLED wrist dashboard.*

## 🗺️ Schematic
The wiring logic behind the I2C multiplexing and analog sensor routing:
![Virtukey_Schematic](https://github.com/user-attachments/assets/021aa8fe-47a9-4015-84fd-92982ba097ec)

*Note: Because the MPU6050s share static I2C addresses, a TCA9548A multiplexer is used to isolate them on the bus, while the OLED screen sits on a dedicated multiplexer channel.*

## 🔌 PCB Layout
The PCB Design:
<img width="1192" height="715" alt="PCB Design" src="https://github.com/user-attachments/assets/353abc23-67ee-4cb6-ba6d-deb206b1ceb8" />

## 📦 Case & Assembly
I'm 3D printing a custom enclosure to tame the wire spaghetti


## 📋 Bill of Materials (BOM)
Here are all the components required to build VirtuKey:

| Component | Quantity | Purpose |
| :--- | :---: | :--- |
| **ESP32 Development Board** | 1 | The main brain handling local sensor math and BLE Combo broadcasting. |
| **MPU6050 (GY-521)** | 6 | 6-axis IMUs (Accelerometer + Gyroscope) for tracking finger tilt, velocity, and downward strikes. |
| **TCA9548A Module** | 1 | 8-Channel I2C Multiplexer to handle the conflicting MPU6050 addresses. |
| **0.96" OLED Display (SSD1306)** | 1 | Real-time I2C UI dashboard to display the active mode and finger mapping. |
| **Flex Sensors / Potentiometers** | 5 | Analog bend detection to map finger curl to specific QWERTY rows. |
| **Push Button** | 1 | Mode toggle switch (Keyboard Mode vs. Air Mouse Mode). |
| **10kΩ Resistor** | 1 | Pull-up resistor for the toggle button (if not using internal pull-ups). |
| **5V Power Bank / 3.7V LiPo** | 1 | Power source (connected via USB or directly to the 5V/VIN pin). |
| **Jumper Wires / Ribbon Cable** | Lots | Connecting the finger sensors to the central wrist hub. |

## 🛠️ Software Stack
* **Framework:** Arduino core via PlatformIO.
* **Libraries:** `Adafruit MPU6050`, `Adafruit SSD1306`, and `BlynkGO ESP32-BLE-Combo`.
* **Signal Processing:** Implements a Complementary Filter for spatial awareness and a 90/10 Low-Pass Filter on raw gyroscope velocity to eliminate cursor drift.
