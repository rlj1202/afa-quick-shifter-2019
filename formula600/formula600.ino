/*
 * A-FA 2019 Formula 600cc Arduino Box
 * 
 * - paddle shift, quick shifter
 * 
 * By Jisu Sim(rlj1202@gmail.com), Department of Software Engineering, At Ajou Univ.
 * Copyright 2019. All rights reserved.
 */

/*
 * TODO List
 * - gear meter using rpm and vs sensor
 */

#include "LiquidCrystal_I2C.h"

/******************************************************************************
 * pins
 ******************************************************************************/
// button
const int PIN_BTN_GEAR_UP = 11;
const int PIN_BTN_GEAR_DOWN = 12;

// relay
const int PIN_VALVE_UP = 5;
const int PIN_VALVE_DOWN = 6;
const int PIN_CUT = 7;

//
const int PIN_RPM = A0;
const int PIN_VSS = 3;

/*******************************************************************************
 * constants
 *******************************************************************************/
const int DEBOUNCE_DELAY = 50; // milli sec

/******************************************************************************
 * classes
 ******************************************************************************/
// Button class for debouncing
class Button {
  int pin;
  unsigned long lastDebounceTime;
  bool lastState;
  bool state;

public:
  Button(int pin) {
    this->pin = pin;
    lastDebounceTime = millis();
    lastState = digitalRead(pin);
    state = lastState;
  }
  void update() {
    int curState = digitalRead(pin);
    if (curState != lastState)
      lastDebounceTime = millis();
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
      state = curState;
    lastState = curState;
  }
  bool getState() {
    return state;
  }
};

/******************************************************************************
 * global variables
 ******************************************************************************/

Button gearUpBtn(PIN_BTN_GEAR_UP);
Button gearDownBtn(PIN_BTN_GEAR_DOWN);

// 아두이노가 전원이 들어온 직후에 약간의 텀을 두어서 오작동을 줄이고자 함.
bool startingUp = true;
const int STARTING_UP_DELAY = 2000;

// taskGear
bool canGearShift = true;
unsigned long lastGearShift;
const unsigned long GEAR_SHIFT_DELAY = 1000;

bool canGearUp = false;
bool canGearDown = false;
bool canGearUpReturn = false;
bool canGearDownReturn = false;
unsigned long lastGearUp;
unsigned long lastGearDown;
unsigned long lastGearUpReturn;
unsigned long lastGearDownReturn;
const unsigned long GEAR_UP_DELAY = 50;
const unsigned long GEAR_DOWN_DELAY = 50;
const unsigned long GEAR_UP_RETURN_DELAY = 300;
const unsigned long GEAR_DOWN_RETURN_DELAY = 300;

bool canCut = false;
bool canCutReturn = false;
unsigned long lastCutReturn;
const unsigned long CUT_RETURN_DELAY = 100;

// taskLCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
unsigned long lastLCDPrint;
const unsigned long LCD_PRINT_DELAY = 350;

// taskRPM
int rpm;
bool warningMode = false;
unsigned long lastBlink;
bool blinkState;
const unsigned long BLINK_RATE = 200;
const int WARN_RPM = 13000;
const unsigned long WARN_DURATION = 1000;

// taskVSS
int pulses;

/******************************************************************************
 * functions
 ******************************************************************************/

// function declarations

void setup();
void loop();

void updateRPM();
void vssInterrupt();

void taskButton();
void taskGear();
void taskRPM();
void taskVSS();
void taskLCD();

// function definitions

void setup() {
  // setup pin modes
  pinMode(PIN_BTN_GEAR_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_GEAR_DOWN, INPUT_PULLUP);
  
  pinMode(PIN_VALVE_UP, OUTPUT); // relay pin
  pinMode(PIN_VALVE_DOWN, OUTPUT); // relay pin
  pinMode(PIN_CUT, OUTPUT); // relay pin
  digitalWrite(PIN_VALVE_UP, HIGH);
  digitalWrite(PIN_VALVE_DOWN, HIGH);
  digitalWrite(PIN_CUT, HIGH);

  pinMode(PIN_VSS, INPUT);

  // init taskGear
  canGearShift = true;
  lastGearShift = millis();

  // init vss
  //attachInterrupt(digitalPinToInterrupt(PIN_VSS), vssInterrupt, RISING);

  // init lcd
  lastLCDPrint = millis();
  lcd.begin();
  lcd.backlight();
}

/*
 * taskButton
 */

void taskButton() {
  gearUpBtn.update();
  gearDownBtn.update();
}

/*
 * taskGear
 */

void taskGear() {
  if (startingUp) return;
  
  bool gearUp = gearUpBtn.getState();
  bool gearDown = gearDownBtn.getState();
  
  if (canGearShift && (gearUp == false || gearDown == false)
          && millis() - lastGearShift > GEAR_SHIFT_DELAY) {
    canGearShift = false;

    canCut = true;

    if (gearUp == false) {
      canGearUp = true;
      lastGearUp = millis();
    } else if (gearDown == false) {
      canGearDown = true;
      lastGearDown = millis();
    }
  }

  // gearUp and gearUpReturn
  if (canGearUp && millis() - lastGearUp > GEAR_UP_DELAY) {
    canGearUp = false;
    canGearUpReturn = true;
    lastGearUpReturn = millis();
    digitalWrite(PIN_VALVE_UP, LOW);
  }
  if (canGearUpReturn && millis() - lastGearUpReturn > GEAR_UP_RETURN_DELAY) {
    canGearUpReturn = false;
    canGearShift = true;
    lastGearShift = millis();
    digitalWrite(PIN_VALVE_UP, HIGH);
  }

  // gearDown and gearDownReturn
  if (canGearDown && millis() - lastGearDown > GEAR_DOWN_DELAY) {
    canGearDown = false;
    canGearDownReturn = true;
    lastGearDownReturn = millis();
    digitalWrite(PIN_VALVE_DOWN, LOW);
  }
  if (canGearDownReturn && millis() - lastGearDownReturn > GEAR_DOWN_RETURN_DELAY) {
    canGearDownReturn = false;
    canGearShift = true;
    lastGearShift = millis();
    digitalWrite(PIN_VALVE_DOWN, HIGH);
  }

  // cut and cutReturn
  if (canCut) {
    canCut = false;
    canCutReturn = true;
    lastCutReturn = millis();
    digitalWrite(PIN_CUT, LOW);
  }

  if (canCutReturn && millis() - lastCutReturn > CUT_RETURN_DELAY) {
    canCutReturn = false;
    digitalWrite(PIN_CUT, HIGH);
  }
}

/*
 * taskRPM
 */

void updateRPM() {
  rpm = analogRead(PIN_RPM);
  rpm = map(rpm, 0, 1023, 0, 17000);
}

void taskRPM() {
  if (!warningMode) {
    updateRPM();
    warningMode = rpm >= WARN_RPM;
  } else {
    if (millis() - lastBlink > BLINK_RATE) {
      lastBlink = millis();
      blinkState = !blinkState;
    }

    if (blinkState) {
      pinMode(PIN_RPM, INPUT);
      updateRPM();
    } else {
      pinMode(PIN_RPM, OUTPUT);
      digitalWrite(PIN_RPM, LOW);
    }

    if (rpm < WARN_RPM) {
      warningMode = false;
      pinMode(PIN_RPM, INPUT);
    }
  }
}

/*
 * taskVSS
 */

void vssInterrupt() {
  pulses++;
}

void taskVSS() {
  
}

/*
 * taskLCD
 */

void taskLCD() {
  if (millis() - lastLCDPrint > LCD_PRINT_DELAY) {
    lastLCDPrint = millis();
    
    lcd.setCursor(0, 0);
    lcd.print("up ");
    lcd.print(gearUpBtn.getState());
    lcd.print(" down ");
    lcd.print(gearDownBtn.getState());
    
    lcd.setCursor(0, 1);
    lcd.print("rpm ");
    lcd.print(rpm);
    lcd.print(" start ");
    lcd.print(startingUp);
    
    lcd.print("               ");
    /*
    lcd.print("pulses : ");
    lcd.print(pulses);
    */
  }
}

/******************************************************************************
 * loop
 ******************************************************************************/

void loop() {
  if (startingUp && millis() > STARTING_UP_DELAY) startingUp = false;
  
  taskButton();
  taskGear();
  taskRPM();
  taskVSS();
  taskLCD();
}
