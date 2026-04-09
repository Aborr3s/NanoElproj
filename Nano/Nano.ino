#include <Wire.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>

// Inställningar för displayen (0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// PINS
const int pinCLK = 4;   
const int pinDT = 3;    
const int pinSW = 2;    
const int pinBuzzer = 8; // (+) benet på KPEG251 till D8, (-) till GND
const int pinServo = 9;

struct PomodoroProgram {
  int workMin;
  int workSec;      // Lagt till sekunder för testläge
  int breakMin;
  int breakSec;     // Lagt till sekunder för testläge
  int totalCycles;
  char name[16]; 
};

// Programmen inklusive det nya testläget längst ner
PomodoroProgram programs[] = {
  {25, 0, 5, 0, 4, "Standard (4x)"},
  {50, 0, 10, 0, 2, "Intensiv (2x)"},
  {90, 0, 20, 0, 1, "Deep Work (1x)"},
  {0, 10, 0, 5, 2, "Testläge (10s)"} // 10 sek jobb, 5 sek vila, 2 varv
};

int currentProgram = 0;
int lastCLK;
bool running = false;
unsigned long startTime;
unsigned long timeLeft;
unsigned long lastRemainingSecs = 999999; // Håller koll på senast utskrivna sekund
int currentCycle = 1;
bool isBreak = false;
Servo servo;

bool cymbalActive = false;
unsigned long cymbalStartTime = 0;
unsigned long cymbalDuration = 0;

// --- Servo oscillation state ---
bool servoOscillating = false;      // true while oscillating
unsigned long lastServoMoveTime = 0; // last time servo was moved
const int SERVO_STEP_DELAY = 10;     // ms between each degree step
int servoCurrentAngle = 0;           // current angle
int servoDirection = 1;              // 1 = moving to 45, -1 = moving to 0

void setup() {
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(pinBuzzer, OUTPUT);

  servo.attach(9);

  lcd.init();
  lcd.backlight();
  
  lastCLK = digitalRead(pinCLK);
  lcd.clear();
  displayMenu();
}

void loop() {
  updateCymbal(); // advance servo oscillation each iteration
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

// Call once to start continuous 0–45° oscillation
void startCymbal() {
  servoOscillating = true;
  servoCurrentAngle = 0;
  servoDirection = 1;
  lastServoMoveTime = millis();
  servo.write(servoCurrentAngle);
}

// Call once to stop oscillation and return servo to 0°
void stopCymbal() {
  servoOscillating = false;
  servoCurrentAngle = 0;
  servo.write(0);
}

// Must be called every loop() iteration to advance the sweep
void updateCymbal() {
  if (!servoOscillating) return;
  unsigned long now = millis();
  if (now - lastServoMoveTime >= SERVO_STEP_DELAY) {
    lastServoMoveTime = now;
    servoCurrentAngle += servoDirection;
    if (servoCurrentAngle >= 45) {
      servoCurrentAngle = 45;
      servoDirection = -1;   // reverse: sweep back to 0
    } else if (servoCurrentAngle <= 0) {
      servoCurrentAngle = 0;
      servoDirection = 1;    // reverse: sweep to 45
    }
    servo.write(servoCurrentAngle);
  }
}

// Ljudfunktion för KPEG251
void makeBeep(int count, int ms) {
  startCymbal();
  for (int i = 0; i < count; i++) {
    digitalWrite(pinBuzzer, HIGH);
    delay(ms);
    digitalWrite(pinBuzzer, LOW);
    if (count > 1) delay(100); 
  }
}

void handleEncoder() {
  int currentCLK = digitalRead(pinCLK);
  if (currentCLK != lastCLK && currentCLK == 0) {
    if (digitalRead(pinDT) != currentCLK) {
      currentProgram--; 
    } else {
      currentProgram++; 
    }
    // Nu 4 program totalt (index 0 till 3)
    if (currentProgram > 3) currentProgram = 0;
    if (currentProgram < 0) currentProgram = 3;
    displayMenu();
  }
  lastCLK = currentCLK;
}

void displayMenu() {
  lcd.setCursor(0, 0);
  lcd.print(programs[currentProgram].name);
  lcd.print("     ");
  
  lcd.setCursor(0, 1);
  if (currentProgram == 3) {
    lcd.print("10s-5s TEST    ");
  } else {
    lcd.print(programs[currentProgram].workMin);
    lcd.print("-");
    lcd.print(programs[currentProgram].breakMin);
    lcd.print("m [Klicka]  ");
  }
}

void startPomodoro() {
  running = true;
  currentCycle = 1;
  isBreak = false;
  makeBeep(1, 400);
  lcd.clear();
  // Räkna ut total tid i millisekunder (minuter + sekunder)
  setTimer(programs[currentProgram].workMin, programs[currentProgram].workSec);
}

void setTimer(int minutes, int seconds) {
  startTime = millis();
  timeLeft = ((unsigned long)minutes * 60 + seconds) * 1000;
  lastRemainingSecs = 999999; // Tvinga skärmuppdatering direkt
  
  lcd.setCursor(0, 0);
  if (isBreak) lcd.print("RAST!        ");
  else lcd.print("PLUGGA!      ");
}

void updateTimer() {
  unsigned long elapsed = millis() - startTime;
  
  if (elapsed >= timeLeft) {
    nextStep();
  } else {
    unsigned long remaining = (timeLeft - elapsed) / 1000;
    
    // Uppdatera ENDAST displayen om sekunden har ändrats
    if (remaining != lastRemainingSecs) {
      lastRemainingSecs = remaining;
      
      int mins = remaining / 60;
      int secs = remaining % 60;

      lcd.setCursor(0, 1);
      lcd.print(isBreak ? "V:" : "J:"); 
      if (mins < 10) lcd.print("0");
      lcd.print(mins);
      lcd.print(":");
      if (secs < 10) lcd.print("0");
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
    makeBeep(3, 150); // Rast-pip
    setTimer(programs[currentProgram].breakMin, programs[currentProgram].breakSec);
  } else {
    currentCycle++;
    if (currentCycle > programs[currentProgram].totalCycles) {
      finishAll();
    } else {
      isBreak = false;
      makeBeep(2, 300); // Jobb-pip
      setTimer(programs[currentProgram].workMin, programs[currentProgram].workSec);
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
