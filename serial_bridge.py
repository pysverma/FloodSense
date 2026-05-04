"""
FloodSense Serial Bridge
========================
Runs on your PC. Reads sensor JSON from Arduino over USB Serial,
sends it to the FloodSense cloud API, and shows predictions + SMS alerts.

Usage:
    pip install pyserial requests
    python serial_bridge.py

    # Or specify port manually:
    python serial_bridge.py --port COM3          (Windows)
    python serial_bridge.py --port /dev/ttyUSB0  (Linux)
    python serial_bridge.py --port /dev/cu.usbmodem14101 (Mac)
"""

import serial
import serial.tools.list_ports
import requests
import json
import time
import argparse
import sys
from datetime import datetime

# ── Configuration ──────────────────────────────────────────────────────────────
# Update this with your deployed Render URL after deployment.
# For local testing, run app.py on the same PC: http://localhost:5000
API_URL = "https://floodsense-api.onrender.com"   # ← change to your Render URL
BAUD_RATE = 9600
TIMEOUT_S = 35          # Seconds to wait for Arduino data before warning
REQUEST_TIMEOUT = 12    # HTTP request timeout in seconds

COLORS = {
    "reset":   "\033[0m",
    "cyan":    "\033[96m",
    "green":   "\033[92m",
    "yellow":  "\033[93m",
    "red":     "\033[91m",
    "bold":    "\033[1m",
    "magenta": "\033[95m",
    "dim":     "\033[2m",
}

def c(text, color): return f"{COLORS.get(color,'')}{text}{COLORS['reset']}"
def ts(): return datetime.now().strftime("%H:%M:%S")
def log(msg): print(f"{c(ts(), 'dim')}  {msg}")


# ── Auto-detect Arduino port ───────────────────────────────────────────────────
def find_arduino_port():
    """Scan serial ports and return the most likely Arduino port."""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return None

    # Keywords that commonly appear in Arduino port descriptions
    arduino_keywords = ['arduino', 'uno', 'nano', 'ch340', 'ch341',
                        'ftdi', 'usb serial', 'usbserial', 'cp210']

    for port in ports:
        desc = port.description.lower()
        if any(kw in desc for kw in arduino_keywords):
            return port.device

    # Fallback: return first available port
    return ports[0].device


# ── Parse DATA: line from Arduino ─────────────────────────────────────────────
def parse_arduino_line(line: str):
    """
    Arduino sends two types of lines:
      # status messages  → log and skip
      DATA:{json...}     → parse and return dict
    """
    line = line.strip()
    if not line:
        return None

    if line.startswith('#'):
        log(c(f"Arduino: {line[2:]}", 'dim'))
        return None

    if line.startswith('DATA:'):
        json_str = line[5:]  # strip 'DATA:' prefix
        try:
            return json.loads(json_str)
        except json.JSONDecodeError as e:
            log(c(f"JSON parse error: {e} | Raw: {json_str[:80]}", 'yellow'))
            return None

    # Unknown line — log for debugging
    log(c(f"Unknown line: {line[:80]}", 'dim'))
    return None


# ── Send to Cloud API ──────────────────────────────────────────────────────────
def send_to_api(data: dict) -> dict | None:
    """POST sensor data to FloodSense API, return prediction response."""
    url = f"{API_URL}/predict"
    try:
        resp = requests.post(url, json=data, timeout=REQUEST_TIMEOUT)
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ConnectionError:
        log(c(f"Cannot reach API at {url}. Is the server running?", 'red'))
    except requests.exceptions.Timeout:
        log(c(f"API request timed out after {REQUEST_TIMEOUT}s", 'yellow'))
    except requests.exceptions.HTTPError as e:
        log(c(f"API HTTP error: {e}", 'red'))
    except Exception as e:
        log(c(f"Unexpected error: {e}", 'red'))
    return None


# ── Display prediction result ──────────────────────────────────────────────────
RISK_COLOR = {"HIGH": "red", "MODERATE": "yellow", "LOW": "cyan", "SAFE": "green"}
RISK_ICON  = {"HIGH": "🚨", "MODERATE": "⚠️ ", "LOW": "🔵", "SAFE": "✅"}

def display_result(result: dict):
    risk    = result.get("risk_level", "UNKNOWN")
    prob    = result.get("ensemble_probability", 0) * 100
    flood   = result.get("flood_predicted", False)
    action  = result.get("action", "—")
    col     = RISK_COLOR.get(risk, "cyan")
    icon    = RISK_ICON.get(risk, "❓")

    print()
    print(c("━" * 56, 'cyan'))
    print(c("  🌊 FloodSense Prediction", 'bold'))
    print(c("━" * 56, 'cyan'))
    print(f"  {icon}  Risk Level  :  {c(risk, col)} {c('← FLOOD ALERT!' if flood else '', 'red')}")
    print(f"  📊  Probability :  {c(f'{prob:.1f}%', col)}")
    print(f"  📋  Action      :  {action}")

    # Per-model votes
    preds = result.get("model_predictions", {})
    if preds:
        print(f"  {'─'*50}")
        print(f"  {c('Model Votes:', 'dim')}")
        for name, pred in preds.items():
            p = pred.get('probability', 0) if isinstance(pred, dict) else pred
            bar_len = int(p * 20)
            bar = "█" * bar_len + "░" * (20 - bar_len)
            color = "red" if p >= 0.5 else "green"
            print(f"    {name:<25} {c(bar, color)} {p*100:.0f}%")

    # SMS status
    sms = result.get("sms_alert", {})
    if sms:
        status = sms.get("status", "")
        if status == "sent":
            print(f"  {c('📱 SMS ALERT SENT to emergency contacts via Twilio!', 'green')}")
        elif status == "demo":
            print(f"  {c('📱 SMS would fire — configure Twilio env vars on Render', 'yellow')}")

    print(c("━" * 56, 'cyan'))
    print()


# ── Main loop ─────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="FloodSense Serial Bridge")
    parser.add_argument('--port',   type=str, default=None,
                        help="Serial port (e.g. COM3, /dev/ttyUSB0). Auto-detects if omitted.")
    parser.add_argument('--baud',   type=int, default=BAUD_RATE,
                        help=f"Baud rate (default {BAUD_RATE})")
    parser.add_argument('--api',    type=str, default=API_URL,
                        help="FloodSense API base URL")
    parser.add_argument('--demo',   action='store_true',
                        help="Demo mode: skip serial, send mock data to API")
    args = parser.parse_args()

    global API_URL
    API_URL = args.api.rstrip('/')

    print()
    print(c("╔══════════════════════════════════════════════╗", 'cyan'))
    print(c("║       FloodSense Serial Bridge v1.0         ║", 'cyan'))
    print(c("╚══════════════════════════════════════════════╝", 'cyan'))
    print(f"  API URL : {c(API_URL, 'cyan')}")
    print()

    # ── Demo mode: send mock high-risk data ──
    if args.demo:
        log(c("DEMO MODE — sending mock sensor data", 'yellow'))
        mock_data = {
            "rainfall_mm": 275, "temperature_c": 21.5, "humidity_pct": 96,
            "river_discharge_m3s": 480, "water_level_m": 3.8, "elevation_m": 300,
            "barometric_pressure_hpa": 981.5, "soil_moisture_pct": 89,
            "land_cover": "Agricultural", "soil_type": "Clay",
            "infrastructure": 1, "historical_floods": 1,
            "location": "Dehradun, Uttarakhand",
        }
        log(f"Sending mock data to API...")
        result = send_to_api(mock_data)
        if result:
            display_result(result)
        else:
            log(c("API unreachable in demo mode. Start app.py locally first.", 'red'))
        return

    # ── Find serial port ──
    port = args.port or find_arduino_port()
    if not port:
        log(c("No serial port found. Plug in Arduino and try again.", 'red'))
        log("Available ports:")
        for p in serial.tools.list_ports.comports():
            log(f"  {p.device} — {p.description}")
        sys.exit(1)

    log(f"Connecting to Arduino on {c(port, 'cyan')} at {args.baud} baud...")

    try:
        ser = serial.Serial(port, args.baud, timeout=2)
        time.sleep(2)  # Wait for Arduino to reset after serial connect
        ser.reset_input_buffer()
        log(c("✅ Serial connection established", 'green'))
        log("Waiting for sensor data (Arduino sends every 30s)...\n")
    except serial.SerialException as e:
        log(c(f"Cannot open serial port {port}: {e}", 'red'))
        log("Tips:")
        log("  • Close Arduino IDE Serial Monitor if it's open")
        log("  • Check the port name (use --port to specify manually)")
        log("  • On Linux: sudo chmod 666 /dev/ttyUSB0")
        sys.exit(1)

    reading_count = 0

    try:
        while True:
            try:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode('utf-8', errors='replace').strip()
            except serial.SerialException as e:
                log(c(f"Serial read error: {e}", 'red'))
                break

            data = parse_arduino_line(line)
            if data is None:
                continue

            reading_count += 1
            log(c(f"📡 Sensor reading #{reading_count} received", 'green'))

            # Show key values inline
            log(f"   Rainfall: {data.get('rainfall_mm','?')} mm  |  "
                f"Water: {data.get('water_level_m','?')} m  |  "
                f"Humidity: {data.get('humidity_pct','?')} %  |  "
                f"Pressure: {data.get('barometric_pressure_hpa','?')} hPa")

            log("Sending to FloodSense API...")
            result = send_to_api(data)

            if result:
                display_result(result)
            else:
                log(c("API call failed. Retrying next cycle.", 'yellow'))

    except KeyboardInterrupt:
        print()
        log(c("Stopped by user. Goodbye! 👋", 'cyan'))
    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
