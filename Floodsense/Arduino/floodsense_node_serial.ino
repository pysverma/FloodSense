/*
 * ============================================================
 *  FloodSense IoT Node — Arduino UNO/Nano (NO WiFi Required)
 *  Sensors: Rainfall, Water Level (HC-SR04), Soil Moisture,
 *           DHT22 (Temp/Humidity), BMP280 (Barometric Pressure)
 *
 *  HOW IT WORKS:
 *    Arduino reads all sensors every 30 seconds, formats the
 *    data as a JSON string, and sends it over USB Serial to
 *    your PC. The PC-side Python script (serial_bridge.py)
 *    reads that JSON and forwards it to the cloud ML API.
 *
 * ============================================================
 *  WIRING DIAGRAM
 * ============================================================
 *
 *  ┌─────────────────────────────────────────────────────────┐
 *  │                   ARDUINO UNO / NANO                    │
 *  │                                                         │
 *  │  RAINFALL SENSOR (YL-83 / FC-37):                       │
 *  │    VCC  → 3.3V (or 5V — check your module)              │
 *  │    GND  → GND                                           │
 *  │    AO   → A0   (analog output)                          │
 *  │    DO   → D7   (digital threshold output, optional)     │
 *  │                                                         │
 *  │  WATER LEVEL SENSOR (Ultrasonic HC-SR04):               │
 *  │    VCC  → 5V                                            │
 *  │    GND  → GND                                           │
 *  │    TRIG → D5                                            │
 *  │    ECHO → D6                                            │
 *  │    ⚠️  Arduino Uno ECHO pin is 5V — use a              │
 *  │       voltage divider (1kΩ + 2kΩ) to bring to ~3.3V    │
 *  │       before D6 if your sensor needs it. Most HC-SR04   │
 *  │       modules are 5V tolerant on the Uno directly.      │
 *  │                                                         │
 *  │  SOIL MOISTURE SENSOR (Capacitive v1.2):                │
 *  │    VCC  → 3.3V                                          │
 *  │    GND  → GND                                           │
 *  │    AOUT → A1                                            │
 *  │    ⚠️  Some modules are 5V only. Check your module.     │
 *  │                                                         │
 *  │  DHT22 (Temperature + Humidity):                        │
 *  │    Pin 1 (VCC)  → 5V                                    │
 *  │    Pin 2 (DATA) → D4  + 10kΩ resistor to 5V            │
 *  │    Pin 4 (GND)  → GND                                   │
 *  │                                                         │
 *  │  BMP280 (Barometric Pressure) via I2C:                  │
 *  │    VCC  → 3.3V  ← IMPORTANT: 3.3V only, not 5V!        │
 *  │    GND  → GND                                           │
 *  │    SCL  → A5   (I2C Clock on Uno/Nano)                  │
 *  │    SDA  → A4   (I2C Data  on Uno/Nano)                  │
 *  │    SDO  → GND  (sets I2C address to 0x76)               │
 *  │    CSB  → 3.3V (enables I2C mode)                       │
 *  │                                                         │
 *  │  USB CABLE → Computer (for Serial data + power)         │
 *  └─────────────────────────────────────────────────────────┘
 *
 *  PIN SUMMARY:
 *    A0  — Rainfall Sensor (AO analog)
 *    A1  — Soil Moisture Sensor (AOUT analog)
 *    A4  — BMP280 SDA (I2C)
 *    A5  — BMP280 SCL (I2C)
 *    D4  — DHT22 DATA
 *    D5  — HC-SR04 TRIG
 *    D6  — HC-SR04 ECHO
 *    D7  — Rainfall Sensor DO (digital, optional)
 *
 *  LIBRARIES (install via Arduino IDE → Library Manager):
 *    - DHT sensor library        by Adafruit
 *    - Adafruit BMP280 Library   by Adafruit
 *    - Adafruit Unified Sensor   by Adafruit
 *    - ArduinoJson               by Benoit Blanchon (v6.x)
 *
 * ============================================================
 */

#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <ArduinoJson.h>

// ── Pin Definitions ──────────────────────────────────────────
#define RAINFALL_ANALOG_PIN    A0
#define RAINFALL_DIGITAL_PIN    7   // D7
#define SOIL_MOISTURE_PIN      A1
#define DHT_PIN                 4   // D4
#define DHT_TYPE             DHT22
#define ULTRASONIC_TRIG_PIN     5   // D5
#define ULTRASONIC_ECHO_PIN     6   // D6

// ── Sensor Objects ────────────────────────────────────────────
DHT            dht(DHT_PIN, DHT_TYPE);
Adafruit_BMP280 bmp;

// ── Site Configuration ────────────────────────────────────────
// Update these for your deployment location
const char* LOCATION         = "Dehradun, Uttarakhand";
const char* LAND_COVER       = "Agricultural";   // Agricultural/Forest/Urban/Water Body/Desert
const char* SOIL_TYPE        = "Clay";           // Clay/Loam/Sandy/Silt/Peat
const int   INFRASTRUCTURE   = 1;               // 1=present, 0=absent
const int   HISTORICAL_FLOODS = 0;              // 1=yes, 0=no
const float ELEVATION_M      = 300.0;

// ── Calibration ───────────────────────────────────────────────
// Calibrate these by testing your actual sensors:
// RAINFALL: put sensor in air (dry) → record value → RAINFALL_DRY
//           put sensor in water     → record value → RAINFALL_WET
const int   RAINFALL_DRY    = 1023;  // Sensor in dry air
const int   RAINFALL_WET    = 250;   // Sensor submerged in water

// SOIL MOISTURE: put in dry soil → SOIL_DRY; in wet soil → SOIL_WET
const int   SOIL_DRY        = 890;
const int   SOIL_WET        = 380;

// HC-SR04: height from sensor face down to river/tank bottom (cm)
// When water = 0, distance = SENSOR_HEIGHT_CM
const float SENSOR_HEIGHT_CM = 200.0;  // Set to actual mounting height

// ── Timing ────────────────────────────────────────────────────
const unsigned long READ_INTERVAL_MS = 30000; // 30 seconds
unsigned long lastReadTime = 0;

bool bmpOK = false;

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  while (!Serial) { ; } // Wait for Serial on Leonardo/Mega (skip on Uno)

  Serial.println(F("# FloodSense Arduino Node Starting..."));

  // DHT22
  dht.begin();
  Serial.println(F("# DHT22 initialized"));

  // BMP280 via I2C
  Wire.begin();
  if (bmp.begin(0x76)) {
    bmpOK = true;
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
    Serial.println(F("# BMP280 initialized at 0x76"));
  } else if (bmp.begin(0x77)) {
    bmpOK = true;
    Serial.println(F("# BMP280 initialized at 0x77"));
  } else {
    Serial.println(F("# WARNING: BMP280 not found. Using default pressure 1013.25 hPa"));
  }

  // Pin modes
  pinMode(RAINFALL_DIGITAL_PIN, INPUT);
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  Serial.println(F("# All sensors ready. Sending data every 30 seconds."));
  Serial.println(F("# Lines starting with # are status messages, not data."));
  Serial.println(F("# JSON lines are the sensor readings — read by serial_bridge.py"));
  Serial.println(F("# -------------------------------------------------------"));

  // Send first reading immediately
  sendSensorReading();
  lastReadTime = millis();
}

// ─────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();
  if (now - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = now;
    sendSensorReading();
  }
}

// ─────────────────────────────────────────────────────────────
float readRainfallMM() {
  int raw = analogRead(RAINFALL_ANALOG_PIN);
  // Map: lower raw → more water → higher rainfall
  float pct = map(raw, RAINFALL_WET, RAINFALL_DRY, 100, 0);
  pct = constrain(pct, 0, 100);
  // Convert to rough mm (0–300 mm range, adjust multiplier if needed)
  return pct * 3.0;
}

float readWaterLevelM() {
  // Trigger ultrasonic pulse
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000UL); // 30ms timeout
  if (duration == 0) {
    Serial.println(F("# WARNING: HC-SR04 no echo — check wiring"));
    return 0.0;
  }

  float distance_cm = (duration * 0.0343) / 2.0;
  float water_cm = SENSOR_HEIGHT_CM - distance_cm;
  water_cm = constrain(water_cm, 0, SENSOR_HEIGHT_CM);
  return water_cm / 100.0; // Convert to metres
}

float readSoilMoisturePct() {
  int raw = analogRead(SOIL_MOISTURE_PIN);
  float pct = map(raw, SOIL_DRY, SOIL_WET, 0, 100);
  return constrain(pct, 0, 100);
}

void sendSensorReading() {
  // ── Read all sensors ──
  float rainfall_mm    = readRainfallMM();
  float water_level_m  = readWaterLevelM();
  float soil_moisture  = readSoilMoisturePct();

  float temperature_c  = dht.readTemperature();
  float humidity_pct   = dht.readHumidity();
  if (isnan(temperature_c)) { temperature_c = 25.0; Serial.println(F("# WARNING: DHT22 read failed")); }
  if (isnan(humidity_pct))  { humidity_pct  = 60.0; }

  float pressure_hpa = bmpOK ? (bmp.readPressure() / 100.0F) : 1013.25;

  // Derived: simple power-law river discharge estimate
  // Q = 5.2 * H^1.67  (rough approximation — replace with gauging data)
  float river_discharge = (water_level_m > 0.0) ? (5.2 * pow(water_level_m, 1.67)) : 0.0;

  // ── Build JSON ──
  // Using a compact StaticJsonDocument (fits in Uno's 2KB SRAM)
  StaticJsonDocument<384> doc;
  doc["rainfall_mm"]              = serialized(String(rainfall_mm, 2));
  doc["temperature_c"]            = serialized(String(temperature_c, 2));
  doc["humidity_pct"]             = serialized(String(humidity_pct, 2));
  doc["river_discharge_m3s"]      = serialized(String(river_discharge, 2));
  doc["water_level_m"]            = serialized(String(water_level_m, 3));
  doc["elevation_m"]              = ELEVATION_M;
  doc["barometric_pressure_hpa"]  = serialized(String(pressure_hpa, 2));
  doc["soil_moisture_pct"]        = serialized(String(soil_moisture, 2));
  doc["land_cover"]               = LAND_COVER;
  doc["soil_type"]                = SOIL_TYPE;
  doc["infrastructure"]           = INFRASTRUCTURE;
  doc["historical_floods"]        = HISTORICAL_FLOODS;
  doc["location"]                 = LOCATION;

  // ── Print human-readable summary first ──
  Serial.println(F("# --- Sensor Reading ---"));
  Serial.print(F("# Rainfall:   ")); Serial.print(rainfall_mm,   2); Serial.println(F(" mm"));
  Serial.print(F("# Temp:       ")); Serial.print(temperature_c, 2); Serial.println(F(" C"));
  Serial.print(F("# Humidity:   ")); Serial.print(humidity_pct,  2); Serial.println(F(" %"));
  Serial.print(F("# Water Lvl:  ")); Serial.print(water_level_m, 3); Serial.println(F(" m"));
  Serial.print(F("# Soil Moist: ")); Serial.print(soil_moisture,  2); Serial.println(F(" %"));
  Serial.print(F("# Pressure:   ")); Serial.print(pressure_hpa,  2); Serial.println(F(" hPa"));
  Serial.print(F("# Discharge:  ")); Serial.print(river_discharge,2); Serial.println(F(" m3/s"));

  // ── Send JSON on its own line (bridge script reads this) ──
  Serial.print(F("DATA:"));
  serializeJson(doc, Serial);
  Serial.println(); // newline — crucial for readline() in Python
}
