#include "Arduino_RouterBridge.h"
#include <Wire.h>
#include <U8g2lib.h>
#include <Servo.h>

// ---------- Display ----------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

#define MAX_LINES 200
#define LINE_CHARS 21
#define VISIBLE_ROWS 6

String lines[MAX_LINES];
int lineCount = 0;
String incomingBuffer = "";
bool isProcessing = false;
unsigned long lastAnimUpdate = 0;
int animFrame = 0;

// ---------- Pins ----------
int potPin = A0;
int buzzerPin = 8;

int flamePin = 7;
int soundPin = 3;      // touch-trigger, floating, needs debounce
int aerialPin = 12;    // touch module, clean digital
int ldrPin = A1;       // 0 = night, 1 = day
int joyX = A2, joyY = A3, joyBtn = 2;

int redPin = 9, greenPin = 10, bluePin = 11;
int servoPin = 6, ledPin = 5;

Servo myServo;
int baseX, baseY;
int deadZone = 60;
int autoPos = 0, autoDir = 1;
unsigned long lastLedBlink = 0;
bool ledState = false;

int highCount = 0;
int threshold = 3;
bool soundTriggered = false;

unsigned long lastBuzzToggle = 0;
bool buzzState = false;

// ---------- Sprinkler (System-triggered) state ----------
bool sprinklerActive = false;
unsigned long sprinklerStartTime = 0;
const unsigned long SPRINKLER_DURATION = 15000; // 15 sec

// ---------- Global sensor state (shared with get_sensor_state) ----------
bool fireDetected = false;
bool aerialDetected = false;
bool soundDetected = false;
int dayState = 0;   // 0 = night, 1 = day

// ---------- Display helpers ----------
void wrapText(String text) {
  lineCount = 0;
  String currentLine = "";
  int start = 0;
  int len = text.length();

  while (start < len && lineCount < MAX_LINES) {
    int spaceIdx = text.indexOf(' ', start);
    String word = (spaceIdx == -1) ? text.substring(start) : text.substring(start, spaceIdx);

    while (word.length() > LINE_CHARS) {
      if (currentLine.length() > 0) {
        lines[lineCount++] = currentLine;
        currentLine = "";
      }
      lines[lineCount++] = word.substring(0, LINE_CHARS);
      word = word.substring(LINE_CHARS);
    }

    if (currentLine.length() == 0) {
      currentLine = word;
    } else if (currentLine.length() + 1 + word.length() <= LINE_CHARS) {
      currentLine += " " + word;
    } else {
      lines[lineCount++] = currentLine;
      currentLine = word;
    }

    start = (spaceIdx == -1) ? len : spaceIdx + 1;
  }
  if (currentLine.length() > 0 && lineCount < MAX_LINES) {
    lines[lineCount++] = currentLine;
  }
}

bool display_thinking() {
  isProcessing = true;
  animFrame = 0;
  lastAnimUpdate = millis();
  return true;
}

bool display_chunk(String chunk) {
  incomingBuffer += chunk;
  return true;
}

bool display_done() {
  isProcessing = false;
  wrapText(incomingBuffer);
  incomingBuffer = "";
  digitalWrite(buzzerPin, HIGH);
  delay(50);
  digitalWrite(buzzerPin, LOW);
  return true;
}

// ---------- Bridge-exposed sensor state ----------
String get_sensor_state() {
  return "fire:" + String(fireDetected) +
         ",aerial:" + String(aerialDetected) +
         ",sound:" + String(soundDetected) +
         ",day:" + String(dayState);
}

// ---------- Bridge-exposed sprinkler trigger ----------
bool activate_sprinkler() {
  sprinklerActive = true;
  sprinklerStartTime = millis();
  return true;
}

// ---------- Actuator helper ----------
void setColor(int r, int g, int b) {
  analogWrite(redPin, r);
  analogWrite(greenPin, g);
  analogWrite(bluePin, b);
}

void setup() {
  Serial.begin(9600);
  Bridge.begin();

  pinMode(buzzerPin, OUTPUT);
  pinMode(flamePin, INPUT);
  pinMode(soundPin, INPUT);
  pinMode(aerialPin, INPUT);
  pinMode(ldrPin, INPUT);
  pinMode(joyBtn, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  analogReadResolution(10);
  myServo.attach(servoPin);
  baseX = analogRead(joyX);
  baseY = analogRead(joyY);

  display.setI2CAddress(0x78);
  display.begin();
  display.setFont(u8g2_font_6x10_tf);

  Bridge.provide("display_thinking", display_thinking);
  Bridge.provide("display_chunk", display_chunk);
  Bridge.provide("display_done", display_done);
  Bridge.provide("get_sensor_state", get_sensor_state);
  Bridge.provide("activate_sprinkler", activate_sprinkler);

  lines[0] = "Waiting for input...";
  lineCount = 1;
}

void loop() {
  // --- Read sensors, update GLOBAL state ---
  int flame = digitalRead(flamePin);       // flame sensor faulty; see hardcode below
  int aerial = digitalRead(aerialPin);     // HIGH = object detected
  dayState = digitalRead(ldrPin);          // 1 = day, 0 = night

  int soundRaw = digitalRead(soundPin);
  if (soundRaw == HIGH) highCount++; else highCount = 0;
  soundTriggered = (highCount >= threshold);

  fireDetected = (flame == HIGH);
  aerialDetected = (aerial == HIGH);
  soundDetected = soundTriggered;

  // --- Joystick override check ---
  int xVal = analogRead(joyX);
  int yVal = analogRead(joyY);
  int btnState = digitalRead(joyBtn);
  bool humanActive = (abs(xVal - baseX) > deadZone) ||
                      (abs(yVal - baseY) > deadZone) ||
                      (btnState == LOW);

  // --- RGB reflex (instant visual, independent of processing delay) ---
  if (fireDetected && aerialDetected) {
    if ((millis() / 300) % 2 == 0) setColor(255, 60, 0);    // red-orange = fire
    else setColor(160, 0, 255);                             // violet = aerial
  } 
  else if (fireDetected) {
    setColor(255, 60, 0);           // red-orange = fire
  } 
  else if (aerialDetected && soundDetected) {
    if ((millis() / 300) % 2 == 0) setColor(160, 0, 255);   // violet = aerial
    else setColor(255, 255, 0);                             // yellow = sound
  } 
  else if (aerialDetected) {
    setColor(160, 0, 255);          // violet = aerial/object detected
  } 
  else if (soundDetected) {
    setColor(255, 255, 0);          // yellow = sound/crow detected
  } 
  else {
    setColor(0, 0, 0);              // idle
  }

  // --- Auto-reset sprinkler after its 15s window ---
  if (sprinklerActive && millis() - sprinklerStartTime > SPRINKLER_DURATION) {
    sprinklerActive = false;
  }
  
  // --- Servo and LED Actuation ---
  if (humanActive) {
    // Manual joystick override takes over the servo position
    int angle = map(xVal, 0, 1023, 0, 180);
    myServo.write(angle);
    
    // Blink LED to show manual steering
    if (millis() - lastLedBlink > 150) {
      ledState = !ledState;
      digitalWrite(ledPin, ledState);
      lastLedBlink = millis();
    }
  } else if (sprinklerActive) {
    // System-decided sprinkler: automatic sweeping
    myServo.write(autoPos);
    autoPos += autoDir * 5;
    if (autoPos >= 180 || autoPos <= 0) autoDir = -autoDir;

    // Blink LED to show system sprinkling
    if (millis() - lastLedBlink > 150) {
      ledState = !ledState;
      digitalWrite(ledPin, ledState);
      lastLedBlink = millis();
    }
  } else {
    // Idle — turn off LED
    digitalWrite(ledPin, LOW);
  }

  // --- Buzzer Actuation (Independent 15-second timer) ---
  if (sprinklerActive) {
    // The siren will sound for the 15s window, regardless of human steering
    if (millis() - lastBuzzToggle > 200) {
      buzzState = !buzzState;
      digitalWrite(buzzerPin, buzzState ? HIGH : LOW);
      lastBuzzToggle = millis();
    }
  } else {
    // Idle - silence buzzer
    digitalWrite(buzzerPin, LOW);
    buzzState = false;
  }
  
  // --- OLED rendering ---
  display.clearBuffer();
  if (isProcessing) {
    if (millis() - lastAnimUpdate > 400) {
      animFrame = (animFrame + 1) % 4;
      lastAnimUpdate = millis();
    }
    String dots = "";
    for (int i = 0; i < animFrame; i++) dots += ".";
    display.drawStr(0, 30, ("Thinking" + dots).c_str());
  } else {
    int potValue = analogRead(potPin);
    int maxScroll = max(0, lineCount - VISIBLE_ROWS);
    int potTopThreshold = 1003;
    int scrollOffset;
    if (potValue >= potTopThreshold) {
      scrollOffset = maxScroll;
    } else {
      scrollOffset = map(potValue, 0, potTopThreshold, 0, maxScroll);
    }
    for (int row = 0; row < VISIBLE_ROWS; row++) {
      int lineIdx = scrollOffset + row;
      if (lineIdx < lineCount) {
        display.drawStr(0, (row + 1) * 10, lines[lineIdx].c_str());
      }
    }
  }
  display.sendBuffer();

  // --- Debug ---
  Serial.print("fire:"); Serial.print(fireDetected);
  Serial.print(" aerial:"); Serial.print(aerialDetected);
  Serial.print(" sound:"); Serial.print(soundDetected);
  Serial.print(" day:"); Serial.print(dayState);
  Serial.print(" override:"); Serial.print(humanActive);
  Serial.print(" sprinkler:"); Serial.println(sprinklerActive);

  delay(50);
}
