# Security Detection System Prototype Using ESP32 and IoT

This repository contains an embedded **security detection system prototype** that monitors a protected area using multiple sensors and sends real-time alerts via **LINE Notify**.

The system combines:

- **STM32 NUCLEO-F411RE** – edge controller for sensor reading and alarm logic  
- **NodeMCU ESP32-S3** – IoT node for Wi-Fi and LINE notifications  
- **KY-038 microphone sound sensor** – abnormal sound detection  
- **IR-08H obstacle sensor** – human/object proximity detection  
- **TMB12A05 active buzzer + LED** – local audible and visual alarms  

When a person comes within a predefined distance or when the sound level is abnormally high, the STM32 raises an alarm, drives the LED/buzzer, and sends the alarm status to the ESP32. The ESP32 then pushes a notification to the user’s LINE account.

---

## Features

- 🔊 **Sound-based intrusion detection** using KY-038 + ADC (amplitude / range based)  
- 📡 **Proximity detection** using IR-08H obstacle sensor  
- 🔔 **Local alarms** via active buzzer and status LED  
- 🌐 **IoT layer with ESP32-S3** for Wi-Fi connectivity  
- 💬 **LINE Notify integration** for real-time alerts (`ALARM_OBSTACLE`, `ALARM_NOISE`, `ALARM_BOTH`, `ALARM_CLEARED`)  
- 🧩 Modular design: STM32 = sensing & logic, ESP32 = networking & notifications  

---

## System Architecture

The system is organized into three layers:

1. **Sensing Layer**
   - KY-038 microphone sound sensor  
   - IR-08H obstacle sensor  
   - TMB12A05 active buzzer  
   - On-board LED (LD2) on STM32  

2. **Edge Processing Layer – STM32 NUCLEO-F411RE**
   - Samples analog audio from KY-038 (AO) via ADC1  
   - Reads obstacle detection from IR-08H (digital input)  
   - Uses thresholds and time-window filtering to detect **obstacle** and **noise** events  
   - Controls LED and buzzer according to the alarm state  
   - Sends alarm status messages over UART to the ESP32  

3. **IoT Communication Layer – NodeMCU ESP32-S3**
   - Receives alarm status strings from STM32 via UART  
   - Manages Wi-Fi connection  
   - Sends LINE Notify messages to the user’s LINE account  

---

## Hardware

### Main Components

- STM32 NUCLEO-F411RE  
- NodeMCU ESP32-S3  
- KY-038 microphone sound sensor  
- IR-08H obstacle sensor  
- TMB12A05 active buzzer  
- On-board LED2 (LD2)  
- Breadboard, jumper wires, USB cables  

### STM32 Pin Usage (summary)

- **PC13** – on-board user button B1 (optional, for testing)  
- **PA0** – digital input from KY-038 `DO` (threshold-based sound detection)  
- **PA1 (ADC1_IN1)** – analog input from KY-038 `AO` (sound-level measurement)  
- **Infrared_Pin (e.g., PBx)** – digital input from IR-08H `D0` (obstacle / proximity)  
- **PA2 / PA3** – `USART2_TX` / `USART2_RX` (ST-LINK virtual COM for debugging)  
- **PA5** – on-board LED2 (system status / alarm indicator)  
- **PA9** – `USART1_TX` to ESP32 RX (alarm status messages)  
- **PA10** – `USART1_RX` from ESP32 TX (reserved for future use)  
- **PB0** – `outputBuzzer` to TMB12A05 active buzzer  

> All modules share a common GND. Power is supplied from the NUCLEO board via USB (check total current consumption).

### STM32 ↔ ESP32 UART Wiring

- STM32 **PA9 (USART1_TX)** → ESP32 **RX**  
- (Optional) STM32 **PA10 (USART1_RX)** ← ESP32 **TX**  
- STM32 **GND** ↔ ESP32 **GND**  

---

## Firmware Overview

### STM32 Firmware (NUCLEO-F411RE)

Implemented using STM32 HAL (STM32CubeIDE).

Main loop:

1. **IR obstacle detection**  
   - Read `Infrared_Pin` (active-low).  
   - If low, update `last_object_seen_time`.  
   - `is_obstacle = 1` if an obstacle has been seen within the last 200 ms.

2. **Sound measurement (KY-038)**  
   - Sample ADC1 (PA1) 500 times.  
   - Compute `min`, `max`, `avg`, and `range = max - min`.  
   - If `range > 1000` (adjustable threshold), update `last_noise_seen_time`.  
   - `is_noise = 1` if loud noise was detected within the last 2000 ms.

3. **Alarm state**  
   - `current_alarm_state = is_obstacle || is_noise`.  
   - On state change:  
     - Turn LED on/off.  
     - Turn buzzer off when alarm is cleared.  
     - Send a status string to ESP32 via `USART1`:  
       - `ALARM_BOTH`, `ALARM_OBSTACLE`, `ALARM_NOISE`, or `ALARM_CLEARED`.  
     - Print debug information (sound range, avg, state) via `USART2` to PC.

4. **Buzzer control**  
   - While alarm is active: toggle PB0 every 1 ms to generate a tone.  
   - When no alarm: small delay to reduce CPU load.

Debug output over `USART2` helps tune the sound threshold.

### ESP32 Firmware (NodeMCU ESP32-S3)

Implemented using the Arduino framework (or PlatformIO).

Responsibilities:

- Configure UART to receive alarm status lines from STM32.  
- Connect to Wi-Fi using configured SSID and password.  
- Store LINE Notify access token.  
- Map alarm strings to LINE message texts (and optional stickers).  
- Send HTTP(S) `POST` requests to LINE Notify API whenever a new alarm status is received.

---

## LINE Notify Setup

1. Create a **LINE Notify access token** from the LINE Notify website.  
2. In the ESP32 source code, set:

   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

   const char* LINE_TOKEN    = "YOUR_LINE_NOTIFY_TOKEN";
