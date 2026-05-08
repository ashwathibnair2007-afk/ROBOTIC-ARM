# ROBOTIC-ARM
Voice assisted robotic arm 
# ============================================================
# 🤖 JARVIS AI ROBOTIC ARM SYSTEM (USB VERSION)
# YOLO + MEDIAPIPE + VOICE + ARDUINO SERIAL
# ============================================================

import cv2
import time
import serial
import pyttsx3
import speech_recognition as sr
import mediapipe as mp
import numpy as np
import math
from ultralytics import YOLO
from pathlib import Path

# ============================================================
# SERIAL PORT DETECTION AND CONFIGURATION
# ============================================================

def list_available_ports():
    """List all available COM ports"""
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()
    available_ports = []

    print("\n🔍 Available COM Ports:")
    print("=" * 40)

    for port in ports:
        print(f"📡 {port.device} - {port.description}")
        available_ports.append(port.device)

    if not available_ports:
        print("❌ No COM ports found!")
        print("💡 Make sure your Arduino/ESP is connected and drivers are installed")
    else:
        print(f"\n✅ Found {len(available_ports)} port(s)")

    return available_ports

def find_arduino_port():
    """Try to automatically find Arduino port"""
    import serial.tools.list_ports

    ports = serial.tools.list_ports.comports()

    # Look for common Arduino descriptions
    arduino_keywords = ['arduino', 'esp8266', 'esp32', 'ch340', 'cp210x', 'ftdi']

    for port in ports:
        description = port.description.lower()
        if any(keyword in description for keyword in arduino_keywords):
            print(f"🎯 Auto-detected Arduino on: {port.device}")
            return port.device

    # If no Arduino found, return first available port
    if ports:
        print(f"⚠️  No Arduino detected, using first available: {ports[0].device}")
        return ports[0].device

    return None

# Configuration - Change this to your COM port
ARDUINO_PORT = 'COM6'  # Default, will auto-detect if not found
ARDUINO_BAUD = 115200

# ============================================================
# SERIAL SETUP (ESP12E / ARDUINO)
# ============================================================

def setup_serial_connection(port=None, baud=ARDUINO_BAUD):
    """Setup serial connection with auto-detection"""
    global arduino

    if port is None:
        port = ARDUINO_PORT

    try:
        print(f"🔌 Attempting connection to {port} at {baud} baud...")
        arduino = serial.Serial(port, baud, timeout=1)
        time.sleep(2)

        # Flush any residual data
        arduino.reset_input_buffer()
        arduino.reset_output_buffer()

        # Test connection by sending a ping
        arduino.write(b"PING\n")
        time.sleep(0.1)

        print(f"✅ Arduino connected on {port}")
        return True

    except Exception as e:
        print(f"❌ Serial connection failed on {port}: {e}")

        # Try auto-detection
        print("🔍 Attempting auto-detection...")
        available_ports = list_available_ports()

        for test_port in available_ports:
            if test_port != port:  # Don't retry the same port
                try:
                    print(f"🔄 Trying {test_port}...")
                    arduino = serial.Serial(test_port, baud, timeout=1)
                    time.sleep(2)
                    arduino.reset_input_buffer()
                    arduino.reset_output_buffer()
                    arduino.write(b"PING\n")
                    time.sleep(0.1)
                    print(f"✅ Arduino found on {test_port}")
                    return True
                except:
                    continue

        print("❌ Could not connect to Arduino on any port")
        arduino = None
        return False

# Initialize serial connection
setup_serial_connection()

def send_cmd(cmd):
    if arduino:
        try:
            arduino.write((cmd + "\n").encode())
            arduino.flush()  # Ensure data is sent immediately
            time.sleep(0.2)  # Give hardware time to process
            print("📤 Sent:", cmd)
        except Exception as e:
            print("❌ Send failed:", e)
    else:
        print("⚠️  No Arduino connection - command not sent")

def configure_port():
    """Manually configure COM port"""
    print("\n🔧 COM Port Configuration")
    print("=" * 30)

    available_ports = list_available_ports()

    if not available_ports:
        print("❌ No ports available!")
        return False

    print("\nEnter the COM port to use (e.g., COM6):")
    port = input("Port: ").strip().upper()

    if port in available_ports:
        global ARDUINO_PORT
        ARDUINO_PORT = port
        print(f"✅ Port set to {port}")
        return setup_serial_connection(port)
    else:
        print(f"❌ Port {port} not found in available ports")
        return False

def show_connection_status():
    """Show current connection status"""
    print("\n📊 Connection Status")
    print("=" * 20)

    if arduino and arduino.is_open:
        print(f"✅ Connected: {arduino.port} at {arduino.baudrate} baud")
        print(f"📡 Port open: {arduino.is_open}")
    else:
        print("❌ Not connected")

    print("\n🔍 Available ports:")
    list_available_ports()

# ============================================================
# TTS + VOICE
# ============================================================

engine = pyttsx3.init()
recognizer = sr.Recognizer()

def speak(text):
    print("🤖:", text)
    engine.say(text)
    engine.runAndWait()

def listen(prompt="Speak now"):
    with sr.Microphone() as source:
        print("🎤", prompt)
        speak(prompt)
        recognizer.adjust_for_ambient_noise(source, duration=1)

        try:
            audio = recognizer.listen(source, timeout=8, phrase_time_limit=5)
            text = recognizer.recognize_google(audio).lower()
            print("🧠 You said:", text)
            return text
        except:
            return None

# ============================================================
# YOLO MODEL
# ============================================================

model = YOLO("yolov8n.pt")

# ============================================================
# MEDIAPIPE HANDS
# ============================================================

mp_hands = mp.solutions.hands
hands = mp_hands.Hands(max_num_hands=1)
mp_draw = mp.solutions.drawing_utils

# ============================================================
# ANGLE FUNCTION
# ============================================================

def angle(a, b, c):
    a = np.array(a)
    b = np.array(b)
    c = np.array(c)

    ab = a - b
    cb = c - b

    cos = np.dot(ab, cb) / (np.linalg.norm(ab) * np.linalg.norm(cb) + 1e-6)
    return np.degrees(np.arccos(np.clip(cos, -1.0, 1.0)))

# ============================================================
# HAND STATE
# ============================================================

def open_hand(angles):
    # At least three fingers should be mostly extended for open hand
    return sum(1 for a in angles if a > 140) >= 3

def closed_hand(angles):
    # At least three fingers should be mostly curled for closed hand
    return sum(1 for a in angles if a < 80) >= 3

# ============================================================
# OBJECT DETECTION
# ============================================================

def detect_object(target):
    cap = cv2.VideoCapture(0)
    speak("Searching object")

    start = time.time()

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        frame = cv2.flip(frame, 1)
        results = model.predict(frame, conf=0.4, verbose=False)

        found = False

        for r in results:
            for box in r.boxes:
                cls = int(box.cls[0])
                label = model.names[cls].lower()

                if target in label:
                    found = True

                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    cv2.rectangle(frame, (x1, y1), (x2, y2), (0,255,0), 2)
                    cv2.putText(frame, label, (x1, y1-10),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0,255,0), 2)

                    send_cmd("pick")
                    time.sleep(1.5)  # Wait for arm to complete object pickup
                    speak("Object detected")
                    cap.release()
                    cv2.destroyAllWindows()
                    return True

        cv2.imshow("YOLO", frame)

        if cv2.waitKey(1) & 0xFF == 27:
            break

        if time.time() - start > 25:
            break

    cap.release()
    cv2.destroyAllWindows()
    return False

# ============================================================
# GESTURE CONTROL
# ============================================================

def gesture_mode():
    cap = cv2.VideoCapture(0)
    speak("Gesture mode activated. Show open or closed hand.")

    last = ""
    gesture_text = ""
    gesture_color = (255, 255, 255)

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        frame = cv2.flip(frame, 1)
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        res = hands.process(rgb)

        current_gesture = ""

        if res.multi_hand_landmarks:
            for hand in res.multi_hand_landmarks:
                mp_draw.draw_landmarks(frame, hand, mp_hands.HAND_CONNECTIONS)

                lm = hand.landmark

                idx = angle((lm[5].x,lm[5].y),(lm[6].x,lm[6].y),(lm[8].x,lm[8].y))
                mid = angle((lm[9].x,lm[9].y),(lm[10].x,lm[10].y),(lm[12].x,lm[12].y))
                ring = angle((lm[13].x,lm[13].y),(lm[14].x,lm[14].y),(lm[16].x,lm[16].y))
                pink = angle((lm[17].x,lm[17].y),(lm[18].x,lm[18].y),(lm[20].x,lm[20].y))

                angles = [idx, mid, ring, pink]

                if open_hand(angles):
                    current_gesture = "open"
                    gesture_text = "OPEN HAND"
                    gesture_color = (0, 255, 0)  # Green
                elif closed_hand(angles):
                    current_gesture = "close"
                    gesture_text = "CLOSED HAND"
                    gesture_color = (0, 0, 255)  # Red
                else:
                    gesture_text = "UNKNOWN GESTURE"
                    gesture_color = (255, 255, 0)  # Yellow

                # Draw gesture text on frame
                cv2.putText(frame, gesture_text, (50, 50),
                           cv2.FONT_HERSHEY_SIMPLEX, 1.5, gesture_color, 3)

                # Only send command if gesture changed
                if current_gesture and current_gesture != last:
                    print("Detected gesture:", current_gesture)
                    if current_gesture == "open":
                        send_cmd("open")
                        time.sleep(0.5)  # Wait for arm to release
                        speak("Open hand detected")
                    elif current_gesture == "close":
                        send_cmd("close")
                        time.sleep(0.5)  # Wait for arm to grab
                        speak("Closed hand detected")

                    last = current_gesture

        else:
            # No hand detected
            gesture_text = "NO HAND DETECTED"
            gesture_color = (128, 128, 128)  # Gray
            cv2.putText(frame, gesture_text, (50, 50),
                       cv2.FONT_HERSHEY_SIMPLEX, 1.5, gesture_color, 3)

        # Add instructions
        cv2.putText(frame, "Press ESC to exit | O=open | C=close", (50, frame.shape[0] - 50),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        cv2.imshow("Gesture Control", frame)

        key = cv2.waitKey(1) & 0xFF
        if key == 27:
            break
        elif key == ord('o'):
            send_cmd("open")
            speak("Manual open command")
        elif key == ord('c'):
            send_cmd("close")
            speak("Manual close command")


    cap.release()
    cv2.destroyAllWindows()

# ============================================================
# 🛑 SAFE RELEASE / EMERGENCY UNLOCK FUNCTION
# ============================================================

def safe_release():
    """
    Emergency release to prevent robotic arm from getting stuck
    Sends multiple release signals + small delay for reliability
    """

    if not arduino:
        print("⚠️ No Arduino connected - cannot release")
        return

    try:
        print("🛑 EMERGENCY RELEASE ACTIVATED")

        # Send multiple times for reliability (very important for ESP/serial issues)
        for i in range(3):
            send_cmd("open")
            time.sleep(0.2)

        # Optional neutral reset command (if supported in Arduino code)
        arduino.write(b"RESET\n")
        arduino.flush()

        print("✅ Safe release sent successfully")

    except Exception as e:
        print("❌ Safe release failed:", e)
# ============================================================
# MAIN
# ============================================================

def main():
    speak("System ready")

    # Show connection status on startup
    show_connection_status()

    while True:
        cmd = listen("Say automatic, gesture, ports, or configure")

        if cmd is None:
            continue

        if "exit" in cmd or "quit" in cmd:
            break

        elif "automatic" in cmd:
            obj = listen("bottle or book?")
            if obj:
                detect_object(obj)

        elif "gesture" in cmd:
            gesture_mode()

        elif "ports" in cmd or "com" in cmd or "status" in cmd:
            show_connection_status()

        elif "configure" in cmd or "config" in cmd:
            if configure_port():
                speak("Port configured successfully")
            else:
                speak("Port configuration failed")

        else:
            speak("Say automatic, gesture, ports, or configure")

# ============================================================
# RUN
# ============================================================

if __name__ == "__main__":
    main()
