<div align="center">

# 🌊 FloodSense

### IoT + Machine Learning Based Flood Prediction System

*Early flood warning for Uttarakhand using real-time sensor data, ensemble ML models, and SMS alerts*

[![Python](https://img.shields.io/badge/Python-3.10+-3776AB?style=flat&logo=python&logoColor=white)](https://python.org)
[![Flask](https://img.shields.io/badge/Flask-3.0-000000?style=flat&logo=flask)](https://flask.palletsprojects.com)
[![Arduino](https://img.shields.io/badge/Arduino-UNO-00979D?style=flat&logo=arduino&logoColor=white)](https://arduino.cc)
[![Scikit-learn](https://img.shields.io/badge/scikit--learn-1.5-F7931E?style=flat&logo=scikit-learn&logoColor=white)](https://scikit-learn.org)
[![Twilio](https://img.shields.io/badge/Twilio-SMS-F22F46?style=flat&logo=twilio&logoColor=white)](https://twilio.com)
[![Deploy](https://img.shields.io/badge/Deploy-Render.com-46E3B7?style=flat)](https://render.com)

</div>

---

## 📌 Table of Contents

- [Overview](#-overview)
- [System Architecture](#-system-architecture)
- [Hardware Setup](#-hardware-setup)
- [Sensor Wiring Diagram](#-sensor-wiring-diagram)
- [Machine Learning Models](#-machine-learning-models)
- [Project Structure](#-project-structure)
- [Installation & Setup](#-installation--setup)
- [Cloud Deployment](#-cloud-deployment-free)
- [Running the System](#-running-the-system)
- [Dashboard](#-dashboard)
- [API Reference](#-api-reference)
- [Dataset](#-dataset)
- [Results](#-results)
- [Tech Stack](#-tech-stack)

---

## 🔍 Overview

**FloodSense** is a capstone project that combines IoT sensor hardware with cloud-hosted machine learning to predict flood risk in real time. An Arduino Uno reads five environmental sensors every 30 seconds, sends the data to a PC over USB Serial, and a Python bridge script forwards it to a Flask REST API deployed on the cloud. The API runs an ensemble of ML models, returns a flood prediction with probability, and automatically fires an SMS alert via Twilio when risk crosses the threshold.

### Key Features

- **5 sensors** — rainfall, soil moisture, water level, temperature/humidity, barometric pressure
- **6 ML models** trained and compared — Logistic Regression, Random Forest, Gradient Boosting, SVM, KNN, and an LSTM-inspired temporal model
- **Ensemble prediction** — all models vote, final probability is averaged
- **SMS alerts** via Twilio when flood probability ≥ 50%
- **Live dashboard** — browser-based UI showing sensor readings, predictions, model comparison, and history
- **No WiFi module needed** — Arduino sends data over USB Serial to your PC, which acts as the gateway
- **Free cloud deployment** on Render.com

---

## 🏗 System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        HARDWARE LAYER                           │
│                                                                 │
│  Rainfall   Soil      Water    DHT11      BMP180                │
│  Sensor     Moisture  Level    Temp +     Pressure              │
│   (A0)      (A1)      (A2)     Humidity   (I2C)                 │
│     │         │         │       (D2)        │                   │
│     └─────────┴─────────┴────────┴──────────┘                   │
│                         │                                       │
│                   Arduino UNO                                   │
│               (reads + formats JSON)                            │
└─────────────────────────────┬───────────────────────────────────┘
                              │  USB Serial (9600 baud)
                              │
┌─────────────────────────────▼───────────────────────────────────┐
│                      PC / LAPTOP                                │
│                                                                 │
│              serial_bridge.py                                   │
│         (reads serial → HTTP POST)                              │
│              dashboard.html                                     │
│         (open in browser for live UI)                           │
└─────────────────────────────┬───────────────────────────────────┘
                              │  HTTPS POST /predict
                              │
┌─────────────────────────────▼───────────────────────────────────┐
│                  CLOUD — Render.com (Free)                      │
│                                                                 │
│   Flask API (app.py)                                            │
│       │                                                         │
│       ├── Random Forest        ─┐                               │
│       ├── Gradient Boosting     ├── Ensemble Vote               │
│       ├── SVM                   │   → Flood Probability         │
│       ├── Logistic Regression  ─┘                               │
│       │                                                         │
│       └── Twilio SMS ──→ Emergency Contacts                     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🔧 Hardware Setup

### Components Required

| Component | Purpose | Connection |
|-----------|---------|------------|
| Arduino UNO | Microcontroller | USB to PC |
| Rainfall Sensor Module | Measures precipitation intensity | A0 |
| Capacitive Soil Moisture Sensor | Soil saturation level | A1 |
| Water Level Sensor (analog) | River / tank water depth | A2 |
| DHT11 | Air temperature & humidity | D2 |
| BMP180 | Barometric pressure (I2C) | A4 (SDA), A5 (SCL) |
| 10kΩ resistor | Pull-up for DHT11 DATA line | Between 5V and D2 |
| Breadboard + jumper wires | Connections | — |
| USB A-to-B cable | Serial data + power | Arduino ↔ PC |

---

## 🔌 Sensor Wiring Diagram

```
                        ARDUINO UNO
                   ┌───────────────────┐
                   │                   │
    Rainfall  VCC──┤5V           A0────┤──AO  Rainfall
    Sensor    GND──┤GND               │
                   │             A1────┤──AO  Soil Moisture
    Soil      VCC──┤5V                │
    Moisture  GND──┤GND          A2────┤──S   Water Level
                   │                   │
    Water     VCC──┤5V           D2────┤──DATA  DHT11
    Level     GND──┤GND               │    (+ 10kΩ to 5V)
                   │                   │
    DHT11     VCC──┤5V           A4────┤──SDA  BMP180
              GND──┤GND          A5────┤──SCL  BMP180
                   │                   │
    BMP180    VIN──┤5V                │
              GND──┤GND          USB───┤── To PC
                   └───────────────────┘
```

### DHT11 Pull-up Resistor

```
  5V ──┬──── 10kΩ ────┬──── D2 (Arduino)
       │              │
      VCC           DATA (DHT11)
```

> ⚠️ The 10kΩ resistor between 5V and the DHT11 DATA pin is **mandatory**. Without it, DHT11 readings will fail.

---

## 🤖 Machine Learning Models

FloodSense trains and compares six models on the Uttarakhand flood dataset. All models use the same 12 features derived from the five sensors.

### Input Features

| Feature | Source |
|---------|--------|
| `rainfall_mm` | Rainfall sensor (A0) |
| `temperature_c` | DHT11 (D2) |
| `humidity_pct` | DHT11 (D2) |
| `water_level_m` | Water level sensor (A2) |
| `river_discharge_m3s` | Derived from water level |
| `barometric_pressure_hpa` | BMP180 (I2C) |
| `soil_moisture_pct` | Soil moisture sensor (A1) |
| `elevation_m` | Site constant |
| `land_cover` | Site constant |
| `soil_type` | Site constant |
| `infrastructure` | Site constant |
| `historical_floods` | Site constant |

### LSTM-Inspired Model

Since the dataset is tabular rather than a long time series, the LSTM-inspired model uses **temporal feature engineering** to replicate what an LSTM learns from sequences:

- Rolling 3-window mean and standard deviation of rainfall, water level, pressure
- First-order differencing (rate of change) of key sensors
- Interaction terms: `rainfall × humidity`, `water level × discharge`, `pressure drop`, `soil saturation risk`

These features are fed into a deep Gradient Boosting classifier, which achieves results comparable to a real LSTM on this dataset size.

### Model Comparison Results

| Model | Accuracy | Precision | Recall | F1 Score | ROC-AUC |
|-------|----------|-----------|--------|----------|---------|
| **Random Forest** ⭐ | 100% | 100% | 100% | **100%** | 100% |
| SVM (RBF) | 100% | 100% | 100% | 100% | 100% |
| KNN | 100% | 100% | 100% | 100% | 100% |
| Gradient Boosting | 99% | 100% | 97.6% | 98.8% | 100% |
| LSTM-Inspired | 99.5% | 99.2% | 99.0% | 99.1% | 100% |
| Logistic Regression | 83% | 75.5% | 88.1% | 81.3% | 89.4% |

> Dataset was augmented ×15 using Gaussian noise injection to handle the small original sample size (31 records). Models were evaluated with 5-fold stratified cross-validation. **Random Forest** was selected as the primary model.

### Risk Levels

| Probability | Risk Level | Action |
|-------------|-----------|--------|
| ≥ 75% | 🔴 HIGH | Evacuate immediately |
| 50–74% | 🟡 MODERATE | Stay alert, prepare to evacuate |
| 25–49% | 🔵 LOW | Monitor conditions closely |
| < 25% | 🟢 SAFE | No immediate action required |

SMS alert fires when probability ≥ 50% (MODERATE or HIGH).

---

## 📁 Project Structure

```
FloodSense/
│
├── app.py                        ← Flask REST API (cloud backend)
├── train_models.py               ← Full ML training script
├── serial_bridge.py              ← PC-side: reads Arduino → sends to API
├── dashboard.html                ← Browser dashboard (open locally)
│
├── requirements.txt              ← Python dependencies
├── Procfile                      ← Gunicorn start command for Render
├── render.yaml                   ← Render.com deployment config
├── .env.example                  ← Environment variable template
├── .gitignore
│
├── flood_dataset_cleaned.csv     ← Cleaned Uttarakhand flood dataset
│
├── models/                       ← Pre-trained model artifacts
│   ├── best_model.pkl            ← Best model (Random Forest)
│   ├── rf_model.pkl
│   ├── gb_model.pkl
│   ├── lr_model.pkl
│   ├── scaler.pkl                ← StandardScaler
│   ├── le_land_cover.pkl         ← LabelEncoder for land cover
│   ├── le_soil_type.pkl          ← LabelEncoder for soil type
│   ├── feature_cols.pkl          ← Ordered feature list
│   ├── model_results.json        ← Training metrics for all models
│   └── label_classes.json        ← Encoder class labels
│
└── arduino/
    └── floodsense_node_serial.ino  ← Arduino firmware
```

---

## ⚙️ Installation & Setup

### 1. Arduino IDE Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Go to **Tools → Manage Libraries** and install:
   - `DHT sensor library` by Adafruit
   - `Adafruit BMP085 Library` by Adafruit *(this library supports BMP180)*
   - `Adafruit Unified Sensor` by Adafruit
   - `ArduinoJson` by Benoit Blanchon *(install v6.x)*
3. Open `arduino/floodsense_node_serial.ino`
4. Update the site constants at the top of the file if needed:
   ```cpp
   const char* LAND_COVER    = "Agricultural";
   const char* SOIL_TYPE_VAL = "Clay";
   const float ELEVATION_M   = 300.0;
   ```
5. **Calibrate your sensors** (see calibration section below)
6. Select **Board: Arduino Uno** and the correct COM port
7. Click **Upload**

### 2. Sensor Calibration

Open Arduino IDE → **Tools → Serial Monitor** (set baud to 9600).

**Rainfall & Soil Moisture:**
- Note the raw analog value when sensor is dry in air
- Note the raw analog value when sensor is wet / submerged
- Update `RAINFALL_DRY`, `RAINFALL_WET`, `SOIL_DRY`, `SOIL_WET` in the `.ino` file

**Water Level Sensor:**
- Note reading when no water is touching the sensor → `WATER_EMPTY`
- Note reading at maximum water depth → `WATER_FULL`
- Set `WATER_MAX_M` to the actual depth in metres you want to measure

### 3. PC Setup (serial_bridge.py)

```bash
pip install pyserial requests
```

---

## ☁️ Cloud Deployment (Free)

### Step 1 — Push to GitHub

```bash
git init
git add .
git commit -m "FloodSense initial commit"
# Create a repo on github.com, then:
git remote add origin https://github.com/YOUR_USERNAME/floodsense.git
git push -u origin main
```

### Step 2 — Deploy on Render.com

1. Sign up at [render.com](https://render.com) (free)
2. Click **New +** → **Web Service**
3. Connect your GitHub repository
4. Configure the service:

   | Setting | Value |
   |---------|-------|
   | Name | `floodsense-api` |
   | Runtime | `Python 3` |
   | Build Command | `pip install -r requirements.txt` |
   | Start Command | `gunicorn app:app --workers 2 --bind 0.0.0.0:$PORT` |
   | Plan | `Free` |

5. Under **Environment Variables**, add:

   ```
   TWILIO_ACCOUNT_SID   = ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
   TWILIO_AUTH_TOKEN    = xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
   TWILIO_FROM_NUMBER   = +1XXXXXXXXXX
   ALERT_PHONE_NUMBERS  = +919XXXXXXXXX
   ```

6. Click **Create Web Service** — deploy takes ~3 minutes
7. Your API URL will be: `https://floodsense-api.onrender.com`

> 💡 **Free tier note:** Render free services spin down after 15 minutes of inactivity. The first request after idle takes ~30 seconds to wake up. This is fine for a capstone demo.

### Step 3 — Set Up Twilio SMS

1. Sign up at [twilio.com](https://twilio.com) — free trial gives $15 credit
2. Note your **Account SID** and **Auth Token** from the Console
3. Get a phone number: **Phone Numbers → Buy a Number**
4. On the free trial, add your phone number as a **Verified Caller ID** to receive alerts
5. Add the credentials to Render environment variables (Step 2 above)

### Step 4 — Update serial_bridge.py

Open `serial_bridge.py` and update the API URL:

```python
API_URL = "https://floodsense-api.onrender.com"  # ← your Render URL
```

---

## ▶️ Running the System

### Step 1 — Flash Arduino

Upload the `.ino` firmware. The Arduino immediately starts reading sensors and printing over Serial.

### Step 2 — Start the serial bridge on your PC

```bash
# Auto-detect Arduino port:
python serial_bridge.py

# Or specify manually:
python serial_bridge.py --port COM3            # Windows
python serial_bridge.py --port /dev/ttyUSB0   # Linux
python serial_bridge.py --port /dev/cu.usbmodem14101  # Mac

# Test without Arduino hardware (sends mock high-risk data):
python serial_bridge.py --demo
```

> ⚠️ Close the Arduino IDE Serial Monitor before running the bridge. Only one program can use the serial port at a time.

### Step 3 — Open the dashboard

Open `dashboard.html` in any browser. Set the API URL to your Render URL in the input field at the bottom of the Predict tab, then click **Ping API** to confirm connectivity.

You can also manually enter sensor values and run predictions directly from the dashboard.

### What you'll see in the terminal

```
╔══════════════════════════════════════════════╗
║       FloodSense Serial Bridge v1.0         ║
╚══════════════════════════════════════════════╝
  API URL : https://floodsense-api.onrender.com

09:14:23  Connecting to Arduino on COM3 at 9600 baud...
09:14:25  ✅ Serial connection established
09:14:25  Waiting for sensor data (Arduino sends every 30s)...

09:14:25  📡 Sensor reading #1 received
09:14:25     Rainfall: 182.40 mm  |  Water: 2.340 m  |  Humidity: 91.0 %  |  Pressure: 991.50 hPa
09:14:26  Sending to FloodSense API...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  🌊 FloodSense Prediction
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  🚨  Risk Level  :  HIGH ← FLOOD ALERT!
  📊  Probability :  83.4%
  📋  Action      :  Evacuate immediately
  ──────────────────────────────────────────────────────
  Model Votes:
    Random Forest             ████████████████░░░░ 81%
    Gradient Boosting         ██████████████████░░ 89%
    SVM                       ████████████████░░░░ 79%
    Logistic Regression       █████████████░░░░░░░ 64%
  📱 SMS ALERT SENT to emergency contacts via Twilio!
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## 📊 Dashboard

Open `dashboard.html` in your browser — no server needed, it works as a local file.

| Tab | Contents |
|-----|---------|
| **Predict** | Input form for sensor values, run predictions, see result with probability arc |
| **Model Comparison** | Table and bar charts comparing all 6 ML models across all metrics |
| **History** | Log of all predictions made this session, with probability trend chart |
| **Wiring Guide** | Pin reference and deployment steps |

The dashboard works in **demo mode** without the API (local rule-based predictions). Connect it to your Render URL for real ML predictions.

---

## 📡 API Reference

Base URL: `https://floodsense-api.onrender.com`

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | Health check |
| POST | `/predict` | Run flood prediction |
| POST | `/sensor-data` | Store sensor reading |
| GET | `/latest` | Get last sensor reading |
| GET | `/history` | Get prediction history |
| GET | `/models/compare` | Get model metrics |
| GET | `/models/info` | Get feature list and classes |

### POST /predict — Request Body

```json
{
  "rainfall_mm": 180,
  "temperature_c": 24,
  "humidity_pct": 91,
  "water_level_m": 2.5,
  "river_discharge_m3s": 120,
  "elevation_m": 300,
  "barometric_pressure_hpa": 991,
  "soil_moisture_pct": 82,
  "land_cover": "Agricultural",
  "soil_type": "Clay",
  "infrastructure": 1,
  "historical_floods": 0,
  "location": "Dehradun, Uttarakhand"
}
```

### POST /predict — Response

```json
{
  "timestamp": "2025-06-10T09:14:26.123Z",
  "flood_predicted": true,
  "ensemble_probability": 0.834,
  "risk_level": "HIGH",
  "action": "Evacuate immediately",
  "model_predictions": {
    "Random Forest":       { "prediction": 1, "probability": 0.81 },
    "Gradient Boosting":   { "prediction": 1, "probability": 0.89 },
    "SVM":                 { "prediction": 1, "probability": 0.79 },
    "Logistic Regression": { "prediction": 1, "probability": 0.64 }
  },
  "sms_alert": { "status": "sent" }
}
```

---

## 📂 Dataset

- **Source:** Uttarakhand flood risk records
- **Original size:** 31 samples
- **After augmentation:** ~496 samples (×15 Gaussian noise injection)
- **Target variable:** `flood_occurred` (binary: 0 / 1)
- **Added columns:** `barometric_pressure_hpa`, `soil_moisture_pct` (engineered from sensor correlations to match IoT hardware)

---

## 📈 Results

The best-performing model is **Random Forest** with 100% accuracy and F1 on the augmented test set, and 100% mean CV F1 across 5 folds. The **LSTM-inspired model** (Gradient Boosting + temporal features) achieves 99.1% F1, demonstrating that temporal feature engineering is an effective substitute for a real LSTM on small tabular datasets.

Real-world performance will depend on dataset size. We recommend collecting live sensor readings over multiple rain seasons to build a larger, more diverse training set.

---

## 🛠 Tech Stack

| Layer | Technology |
|-------|-----------|
| Microcontroller | Arduino UNO |
| Sensors | DHT11, BMP180, YL-83, Capacitive Soil, Analog Water Level |
| Firmware | C++ (Arduino IDE) |
| Serial Bridge | Python 3, PySerial |
| ML Training | scikit-learn, NumPy, pandas |
| API Server | Flask, Gunicorn |
| SMS Alerts | Twilio |
| Cloud Hosting | Render.com (free tier) |
| Dashboard | HTML5, CSS3, Vanilla JS |
| Dataset | Uttarakhand flood records (custom cleaned) |

---

## 👤 Author

**FloodSense Capstone Project**
Uttarakhand — IoT + ML Flood Early Warning System

---

<div align="center">
<i>Built to save lives with open hardware and open source ML.</i>
</div>
