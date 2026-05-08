#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ======================================================
// PCA9685 SETUP
// ======================================================
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Servo channels
#define FINGER1 0
#define FINGER2 1
#define FINGER3 2
#define FINGER4 3
#define FINGER5 4
#define WRIST   5
#define ELBOW   6

// ======================================================
// CURRENT ANGLES (for tracking if needed later)
// ======================================================
int currentAngle[7] = {90,90,90,90,90,90,90};

// ======================================================
// SERVO CONTROL FUNCTION
// ======================================================
void setServo(int channel, int angle) {

  angle = constrain(angle, 0, 180);

  // Safe PCA9685 range (adjust if needed for your servos)
  int pulse = map(angle, 0, 180, 150, 600);

  pwm.setPWM(channel, 0, pulse);

  currentAngle[channel] = angle;

  delay(10);  // small smooth delay
}

// ======================================================
// HOME / RESET POSITION (IMPORTANT)
// ======================================================
void homePosition() {

  Serial.println("RESET → HOME POSITION");

  setServo(FINGER1, 90);
  setServo(FINGER2, 90);
  setServo(FINGER3, 90);
  setServo(FINGER4, 90);
  setServo(FINGER5, 90);

  setServo(WRIST, 90);
  setServo(ELBOW, 90);

  delay(500);

  Serial.println("ARM AT HOME POSITION");
}

// ======================================================
// HAND ACTIONS
// ======================================================
void openHand() {

  Serial.println("OPEN HAND");

  for (int i = 0; i < 5; i++) {
    setServo(i, 180);
  }

  delay(1000);
  homePosition();   // return to neutral after action
}

void closeHand() {

  Serial.println("CLOSE HAND");

  for (int i = 0; i < 5; i++) {
    setServo(i, 0);
  }

  delay(300);
  homePosition();   // return to neutral after action
}

// ======================================================
// PICK OBJECT SEQUENCE
// ======================================================
void pickObject() {

  Serial.println("PICK SEQUENCE START");

  openHand();
  delay(500);

  setServo(ELBOW, 60);
  delay(500);

  setServo(WRIST, 120);
  delay(500);

  closeHand();
  delay(500);

  setServo(ELBOW, 90);
  setServo(WRIST, 90);

  delay(500);

  homePosition();   // FINAL RESET

  Serial.println("PICK COMPLETE");
}

// ======================================================
// DROP OBJECT SEQUENCE
// ======================================================
void dropObject() {

  Serial.println("DROP SEQUENCE START");

  setServo(ELBOW, 60);
  setServo(WRIST, 120);
  delay(500);

  openHand();
  delay(500);

  setServo(ELBOW, 90);
  setServo(WRIST, 90);

  delay(500);

  homePosition();   // FINAL RESET

  Serial.println("DROP COMPLETE");
}

// ======================================================
// SETUP
// ======================================================
void setup() {

  Serial.begin(115200);

  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(50);   // standard servo frequency

  delay(1000);

  Serial.println("🤖 Robotic Arm Ready");

  homePosition();   // safe startup position
}

// ======================================================
// MAIN LOOP (COMMAND LISTENER)
// ======================================================
void loop() {

  if (Serial.available()) {

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    Serial.print("CMD RECEIVED: ");
    Serial.println(cmd);

    // ==================================================
    // BASIC COMMANDS (Python control)
    // ==================================================

    if (cmd == "GRAB") {
      closeHand();
    }

    else if (cmd == "RELEASE") {
      openHand();
    }

    else if (cmd == "PICK") {
      pickObject();
    }

    else if (cmd == "DROP") {
      dropObject();
    }

    else if (cmd == "RESET") {
      homePosition();
    }

    else {
      Serial.println("UNKNOWN COMMAND");
    }
  }
}