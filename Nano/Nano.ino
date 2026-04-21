#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Wire.h>


// Ikoner (Upp/Ner pilar)
byte upArrow[8] = {0b00100, 0b01110, 0b11111, 0b00100,
                   0b00100, 0b00100, 0b00100, 0b00000};

byte downArrow[8] = {0b00100, 0b00100, 0b00100, 0b00100,
                     0b11111, 0b01110, 0b00100, 0b00000};

// Inställningar för displayen (0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// PINS
const int pinCLK = 4;
const int pinDT = 5;
const int pinSW = 6;
const int pinBuzzer = 8;
const int pinServo = 10;
const int pinFaceDetect = 3;

struct PomodoroProgram {
  int workMin;
  int workSec;
  int breakMin;
  int breakSec;
  int totalCycles;
  char name[16];
};

PomodoroProgram programs[] = {
    {25, 0, 5, 0, 4, "Standard (4x)"},
    {50, 0, 10, 0, 2, "Intensiv (2x)"},
    {90, 0, 20, 0, 1, "Deep Work (1x)"},
    {0, 10, 0, 5, 2, "Testlage (10s)"}
};

int currentProgram = 0;
int lastCLK;
bool running = false;
unsigned long startTime;
unsigned long timeLeft;
unsigned long lastRemainingSecs = 999999;
int currentCycle = 1;
bool isBreak = false;
Servo servo;

bool cymbalActive = false;
unsigned long cymbalStartTime = 0;
unsigned long cymbalDuration = 0;

// --- Servo oscillation state ---
bool servoOscillating = false;
unsigned long lastServoMoveTime = 0;
const int SERVO_STEP_DELAY = 4;
int servoCurrentAngle = 0;
int servoDirection = 1;

// --- Motiverande meddelanden ---
const char* messages[] = {
  "Kor hart!     ",
  "Du fixar det! ",
  "Fokus!        ",
  "Snart rast!   ",
  "Bra jobbat!   ",
  "Halva vagen!  ",
  "Du ar bast!   "
};
const int numMessages = 7;
unsigned long lastMessageSwitch = 0;
bool showingMessage = false;
const unsigned long PLUGGA_DURATION = 8000;
const unsigned long MESSAGE_DURATION = 3000;

void setup() {
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinFaceDetect, INPUT);
  pinMode(13, OUTPUT);

  servo.attach(10);

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, upArrow);
  lcd.createChar(1, downArrow);

  lastCLK = digitalRead(pinCLK);
  lcd.clear();
  displayMenu();
}

void loop() {
  updateCymbal();
  handleFaceDetector();
  updateMessage();       // Växla mellan PLUGGA! och motivationsmeddelande
  if (!running) {
    handleEncoder();
    if (digitalRead(pinSW) == LOW) {
      delay(400);
      startPomodoro();
    }
  } else {
    updateTimer();
    if (digitalRead(pinSW) == LOW) {
      delay(400);
      running = false;
      lcd.clear();
      displayMenu();
    }
  }
}

// Växlar rad 0 mellan "PLUGGA!" och motiverande meddelande
void updateMessage() {
  if (!running || isBreak) return;

  unsigned long now = millis();
  unsigned long duration = showingMessage ? MESSAGE_DURATION : PLUGGA_DURATION;

  if (now - lastMessageSwitch >= duration) {
    lastMessageSwitch = now;
    showingMessage = !showingMessage;
    lcd.setCursor(0, 0);
    if (showingMessage) {
      int idx = random(numMessages);
      lcd.print(messages[idx]);
    } else {
      lcd.print("PLUGGA!      ");
    }
  }
}

void handleFaceDetector() {
  if (!running || isBreak) {
    if (servoOscillating) {
      stopCymbal();
      digitalWrite(13, LOW);
    }
    return;
  }

  int faceSignal = digitalRead(pinFaceDetect);
  if (faceSignal == LOW && !servoOscillating) {
    digitalWrite(13, HIGH);
    startCymbal();
  } else if (faceSignal == HIGH && servoOscillating) {
    stopCymbal();
    digitalWrite(13, LOW);
  }
}

void startCymbal() {
  servoOscillating = true;
  servoCurrentAngle = 0;
  servoDirection = 1;
  lastServoMoveTime = millis();
  servo.write(servoCurrentAngle);
  digitalWrite(pinBuzzer, HIGH);
}

void stopCymbal() {
  servoOscillating = false;
  servoCurrentAngle = 0;
  servo.write(0);
  digitalWrite(pinBuzzer, LOW);
}

void updateCymbal() {
  if (!servoOscillating) return;
  
  unsigned long now = millis();
  if (now - lastServoMoveTime >= SERVO_STEP_DELAY) {
    lastServoMoveTime = now;
    servoCurrentAngle += servoDirection;
    if (servoCurrentAngle >= 60) {
      servoCurrentAngle = 60;
      servoDirection = -1;
    } else if (servoCurrentAngle <= 0) {
      servoCurrentAngle = 0;
      servoDirection = 1;
    }
    servo.write(servoCurrentAngle);
  }
}

void makeBeep(int count, int ms) {
  for (int i = 0; i < count; i++) {
    digitalWrite(pinBuzzer, HIGH);
    delay(ms);
    digitalWrite(pinBuzzer, LOW);
    if (count > 1)
      delay(100);
  }
}

void handleEncoder() {
  int currentCLK = digitalRead(pinCLK);
  int currentDT  = digitalRead(pinDT);

  if (currentCLK != lastCLK && currentCLK == LOW) {
    if (currentDT != currentCLK) {
      currentProgram--;
    } else {
      currentProgram++;
    }
    if (currentProgram > 3) currentProgram = 0;
    if (currentProgram < 0) currentProgram = 3;
    displayMenu();
  }
  lastCLK = currentCLK;
}

void displayMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(programs[currentProgram].name);

  for (int i = strlen(programs[currentProgram].name); i < 14; i++) {
    lcd.print(" ");
  }

  lcd.setCursor(0, 1);
  if (currentProgram == 3) {
    lcd.print("10s-5s TEST    ");
  } else {
    lcd.print(programs[currentProgram].workMin);
    lcd.print("-");
    lcd.print(programs[currentProgram].breakMin);
    lcd.print("m [Klicka]  ");
  }

  lcd.setCursor(15, 0);
  lcd.write(0);
  lcd.setCursor(15, 1);
  lcd.write(1);
}

void startPomodoro() {
  running = true;
  currentCycle = 1;
  isBreak = false;
  makeBeep(1, 400);
  lcd.clear();
  setTimer(programs[currentProgram].workMin, programs[currentProgram].workSec);
}

void setTimer(int minutes, int seconds) {
  startTime = millis();
  timeLeft = ((unsigned long)minutes * 60 + seconds) * 1000;
  lastRemainingSecs = 999999;
  lastMessageSwitch = millis(); // Återställ meddelandetimer
  showingMessage = false;       // Börja alltid med "PLUGGA!" eller "RAST!"

  lcd.setCursor(0, 0);
  if (isBreak)
    lcd.print("RAST!        ");
  else
    lcd.print("PLUGGA!      ");
}

void updateTimer() {
  unsigned long elapsed = millis() - startTime;

  if (elapsed >= timeLeft) {
    nextStep();
  } else {
    unsigned long remaining = (timeLeft - elapsed) / 1000;

    if (remaining != lastRemainingSecs) {
      lastRemainingSecs = remaining;

      int mins = remaining / 60;
      int secs = remaining % 60;

      lcd.setCursor(0, 1);
      lcd.print(isBreak ? "V:" : "J:");
      if (mins < 10)
        lcd.print("0");
      lcd.print(mins);
      lcd.print(":");
      if (secs < 10)
        lcd.print("0");
      lcd.print(secs);

      lcd.print(" Set:");
      lcd.print(currentCycle);
      lcd.print("/");
      lcd.print(programs[currentProgram].totalCycles);
      lcd.print(" ");
    }
  }
}

void nextStep() {
  if (!isBreak) {
    isBreak = true;
    stopCymbal();
    makeBeep(3, 150);
    setTimer(programs[currentProgram].breakMin,
             programs[currentProgram].breakSec);
  } else {
    currentCycle++;
    if (currentCycle > programs[currentProgram].totalCycles) {
      finishAll();
    } else {
      isBreak = false;
      makeBeep(2, 300);
      setTimer(programs[currentProgram].workMin,
               programs[currentProgram].workSec);
    }
  }
}

void finishAll() {
  running = false;
  makeBeep(1, 1500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PASS KLART!");
  lcd.setCursor(0, 1);
  lcd.print("Bra jobbat! :)");
  delay(5000);
  lcd.clear();
  displayMenu();
}
