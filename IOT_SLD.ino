#include <LiquidCrystal.h>
#include <Servo.h>
#include <SoftwareSerial.h>

SoftwareSerial btSerial(5, 4);

#define IR_SENSOR_PIN  2
#define SERVO_PIN      3
#define TRIG1          A0
#define ECHO1          A1
#define TRIG2          A2
#define ECHO2          A3

LiquidCrystal lcd(8, 9, 10, 11, 12, 13);
Servo barricade;

bool slot1Occupied = false;
bool slot2Occupied = false;
bool barrierOpen   = false;

// ── Track last state to avoid unnecessary LCD updates ──
int lastFreeSlots    = -1;
bool lastBarrierOpen = false;

long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  return duration * 0.034 / 2;
}

void updateLCD(int freeSlots) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (freeSlots == 2) {
    lcd.print("Slots: 2/2 Free ");
  } else if (freeSlots == 1) {
    lcd.print("Slots: 1/2 Free ");
  } else {
    lcd.print("Slots: FULL!    ");
  }
  lcd.setCursor(0, 1);
  lcd.print("S1:");
  lcd.print(slot1Occupied ? "OCC" : "FREE");
  lcd.print(" S2:");
  lcd.print(slot2Occupied ? "OCC" : "FREE");
}

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);

  barricade.attach(SERVO_PIN);
  barricade.write(0);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Parking");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();
}

void loop() {

  // ── STEP 1: Read ultrasonic sensors ──
  long dist1 = getDistance(TRIG1, ECHO1);
  long dist2 = getDistance(TRIG2, ECHO2);

  slot1Occupied = (dist1 > 0 && dist1 < 8);
  slot2Occupied = (dist2 > 0 && dist2 < 8);

  int freeSlots = (!slot1Occupied ? 1 : 0) + (!slot2Occupied ? 1 : 0);

  // ── STEP 2: Update LCD only when state changes ──
  if (freeSlots != lastFreeSlots) {
    updateLCD(freeSlots);
    lastFreeSlots = freeSlots;
  }

  // ── STEP 3: Bluetooth status ──
  btSerial.print("FREE:");
  btSerial.print(freeSlots);
  btSerial.print(",S1:");
  btSerial.print(slot1Occupied ? 1 : 0);
  btSerial.print(",S2:");
  btSerial.println(slot2Occupied ? 1 : 0);

  Serial.print("FREE:");
  Serial.print(freeSlots);
  Serial.print(",S1:");
  Serial.print(slot1Occupied ? 1 : 0);
  Serial.print(",S2:");
  Serial.println(slot2Occupied ? 1 : 0);

  // ── STEP 4: IR Sensor + Servo Logic ──
  bool carAtEntry = (digitalRead(IR_SENSOR_PIN) == LOW);

  if (carAtEntry) {
    if (freeSlots > 0) {
      if (!barrierOpen) {
        barricade.write(80);
        barrierOpen = true;

        // Only update LCD if needed
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Welcome!        ");
        lcd.setCursor(0, 1);
        lcd.print("Gate Opening... ");
        lastFreeSlots = -1; // Force LCD refresh after gate closes

        btSerial.println("STATUS:GATE_OPEN");
        Serial.println("STATUS:GATE_OPEN");

        delay(3000);
      }
    } else {
      if (barrierOpen) {
        barricade.write(0);
        barrierOpen = false;
      }
      if (lastFreeSlots != 0) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Parking FULL!   ");
        lcd.setCursor(0, 1);
        lcd.print("No Entry!       ");
        lastFreeSlots = 0;
      }

      btSerial.println("STATUS:FULL_NO_ENTRY");
      Serial.println("STATUS:FULL_NO_ENTRY");

      delay(1500);
    }
  } else {
    if (barrierOpen) {
      barricade.write(0);
      barrierOpen = false;
      lastFreeSlots = -1; // Force LCD refresh
      btSerial.println("STATUS:GATE_CLOSED");
      Serial.println("STATUS:GATE_CLOSED");
      delay(500);
    }
  }

  delay(500);
}
