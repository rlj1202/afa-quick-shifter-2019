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
const int PIN_BTN_GEAR_UP = 12;
const int PIN_BTN_GEAR_DOWN = 13;

// relay
const int PIN_VALVE_UP = 9;
const int PIN_VALVE_DOWN = 10;
const int PIN_CUT = 11;

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

/******************************************************************************
 * functions
 ******************************************************************************/

void setup() {
  // setup pin modes
  pinMode(PIN_BTN_GEAR_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_GEAR_DOWN, INPUT_PULLUP);
  
  pinMode(PIN_VALVE_UP, OUTPUT);
  pinMode(PIN_VALVE_DOWN, OUTPUT);
  pinMode(PIN_CUT, OUTPUT);

  pinMode(PIN_VSS, INPUT);

  // init taskGear
  digitalWrite(PIN_VALVE_UP, HIGH);
  digitalWrite(PIN_VALVE_DOWN, HIGH);
  digitalWrite(PIN_CUT, HIGH);

  canGearShift = true;
  lastGearShift = millis();

  // init vss
  //attachInterrupt(digitalPinToInterrupt(PIN_VSS), vssInterrupt, RISING);

  // init lcd
  lastLCDPrint = millis();
  lcd.begin();

  lcd.backlight();
  lcd.print("Hello, world!");
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

void taskRPM() {
  int rpm = analogRead(PIN_RPM);
}

/*
 * taskVSS
 */

int pulses;

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

    int rpm = analogRead(PIN_RPM);
    rpm = map(rpm, 0, 1023, 0, 17000);
    
    lcd.clear();
    lcd.print("up ");
    lcd.print(gearUpBtn.getState());
    lcd.print(" down ");
    lcd.print(gearDownBtn.getState());
    lcd.setCursor(0, 1);
    lcd.print("rpm ");
    lcd.print(rpm);
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
  taskButton();
  taskGear();
  // taskRPM();
  // taskVSS();
  taskLCD();
}
