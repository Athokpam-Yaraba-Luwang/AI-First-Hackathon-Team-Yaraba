# Yageno AI: Private Edge Intelligence for IoT

**Affordable, 100% private Edge AI brains tailored to any client's robotics and IoT workflows.**

## 🚨 The Problem
Integrating conversational AI and smart automation into physical IoT devices currently forces users to compromise. "Smart" devices are either:
1. **Cloud-Reliant:** Completely dependent on continuous internet and expensive cloud APIs (OpenAI), exposing proprietary industrial or private home data to massive security risks and subscription fatigue.
2. **Rigid & Dumb:** Based on simple "If/Then" logic that cannot understand natural context or adapt to complex anomalies.

## 💡 The Solution: Yageno AI
A fully localized, multimodal AI agent powered by an ultra-affordable hybrid SBC + Microcontroller architecture (Arduino UNO Q). 
By running heavily optimized, quantized Small Language Models (SLMs) locally on the edge, Yageno AI acts as an offline, central "Brain" that delivers localized Generative AI intelligence—ensuring **absolute data privacy and zero API burn**.

### Innovation: The Guarded Agent Workflow
To prevent dangerous physical AI "hallucinations," Yageno AI utilizes a deterministic safety layer. The local LLM does not have raw access to GPIO pins. It analyzes telemetry and outputs structured decisions. A hard-coded safety layer verifies these decisions before the microcontroller is allowed to execute physical actions.

---

## 🚜 Proof of Concept (MVP): Autonomous Farm Security
To demonstrate the capabilities of Yageno AI, this repository contains the source code for our first hardware implementation: an **Autonomous Farm Security & Deterrent System**. 

While the Yageno AI architecture is infinitely modular for any client demand, this specific MVP demonstrates real-time hazard detection (fire, acoustic anomalies, aerial threats) and automated mitigation in offline, remote environments.

### 🧠 Dual-Brain Architecture Setup
This project leverages the Arduino UNO Q's unique hardware:
*   **Layer 1 (Real-Time MCU - STM32):** Handles instant reflexes. Polling sensors, controlling the RGB LED alerts, and processing absolute hardware interrupts (like a manual Joystick override) with zero latency.
*   **Layer 2 (Cognitive SBC - Qualcomm Linux):** Runs the quantized LLM. It ingests 5-second windows of compressed sensor telemetry via a Serial/MQTT bridge, analyzes the context, and dictates high-level actions (activating the servo-sprinkler and siren).

### 🛠️ Hardware Stack
*   **Core Board:** Arduino UNO Q (Qualcomm Linux + STM32)
*   **Sensors:** Flame Sensor, Button as sound sensor Sound (Acoustic Anomaly), Touch imitating radar for Aerial Object Sensor, LDR (Day/Night state) - Some sensor are replaced with simpler sensors for prototyping
*   **Actuators:** 180° Servo (Sprinkler targeting), Piezo Buzzer (Siren), RGB LED (Visual alert state), I2C OLED (Explainable AI reasoning output)
*   **Manual Override:** Analog Joystick

### 💻 Software Stack
*   **AI Engine:** Quantized LLM hosted locally via standard REST API (`http://127.0.0.1:8080`).
*   **Bridge Layer:** Python (`main.py`) monitoring telemetry, prompting the LLM, sanitizing outputs, and enforcing the Guarded Agent Workflow.
*   **Microcontroller:** C++ (`sketch.ino`) managing GPIO, hardware interrupts, and OLED display rendering.
*   **Interface:** Lightweight HTML Web UI for manual system queries and status checks.

---

## 🚀 Installation & Usage

**1. MCU Setup**
Upload the C++ Sketch to the STM32 layer of the Arduino UNO Q using the Arduino App Lab or IDE.

**2. SBC Setup**
Ensure your local LLM server is running. Then, SSH into the Qualcomm Linux layer and run the bridge:
```bash
python3 main.py
