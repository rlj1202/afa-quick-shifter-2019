/*
 * A-FA 2019 Formula 600cc Arduino Box
 * 
 * By Jisu Sim(rlj1202@gmail.com), Department of Software Engineering, At Ajou Univ.
 * Copyright 2019. All rights reserved.
 * 
 * Board: Arduino Uno
 * Features:
 *   - gear up, down quick shifting using injection cut(or fuel cut)
 *   - rpm check (not implemented)
 *   - speed check (not implemented)
 *   - gear calculation using gear ratio (not implemented)
 */

/*
 * TODO List
 *   - gear meter using rpm and vs sensor
 *   - interpolate rpm over time
 */

/*
 * WISH List ?
 *   - launch control (control rpm at start and clutch)
 *       control position of clutch is very difficult (almost impossible).
 *       So rather control position linearly, divide position into 3. Pushed,
 *       not pushed, half-pushed.
 *   - wing control (due to down-force in corner)
 */

#include "LiquidCrystal_I2C.h"

//#define TASK_SERIAL

/******************************************************************************
 * class declaration
 ******************************************************************************/
// Button class for debouncing
class Button {
  int pin;
  unsigned long lastDebounceTime;
  bool lastState;
  bool state;
public:
  Button(int pin, int state);
  void update();
  bool getState();
};

/******************************************************************************
 * pins
 ******************************************************************************/
// button
const int PIN_BTN_GEAR_UP = 11; // pull-up
const int PIN_BTN_GEAR_DOWN = 12; // pull-up

// relay
const int PIN_VALVE_UP = 5; // relay pin
const int PIN_VALVE_DOWN = 6; // relay pin
const int PIN_CUT = 7; // relay pin

//
const int PIN_RPM_IN = 2; // interrupt pin
const int PIN_VSS = 3; // interrupt pin

const int PIN_RPM_OUT = 10; // pwm pin

const int PIN_STATUS = 13;

/******************************************************************************
 * global variables
 ******************************************************************************/

Button gearUpBtn(PIN_BTN_GEAR_UP, HIGH);
Button gearDownBtn(PIN_BTN_GEAR_DOWN, HIGH);

const unsigned long DEBOUNCE_DELAY = 50; // milli sec

// 아두이노가 전원이 들어온 직후에 약간의 텀을 두어서 오작동을 줄이고자 함.
bool startingUp = true;
const int STARTING_UP_DELAY = 800;

// taskGear
bool canGearShift = true;
unsigned long lastGearShift;
const unsigned long GEAR_SHIFT_DELAY = 500;

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
unsigned long lastCut;
unsigned long lastCutReturn;
const unsigned long CUT_DELAY = 0;
const unsigned long CUT_RETURN_DELAY = 50;

// taskLCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
unsigned long lastLCDPrint;
const unsigned long LCD_PRINT_DELAY = 100;

// taskSerial
unsigned long lastSerial;
const unsigned long SERIAL_DELAY = 100;

// taskRPM
int rpmPulses;
unsigned long lastRPM;
const unsigned long MAX_RPM = 17000;
const unsigned long RPM_CHECK_DURATION = 123;

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
 * class definitions
 ******************************************************************************/
Button::Button(int pin, int state) : pin(pin), lastState(state), state(state) {
  lastDebounceTime = millis();
  lastState = digitalRead(pin);
  state = lastState;
}

void Button::update() {
  int curState = digitalRead(pin);
  if (curState != lastState)
    lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
    state = curState;
  lastState = curState;
}

bool Button::getState() {
  return state;
}

/******************************************************************************
 * functions
 ******************************************************************************/

// function declarations

void setup();
void loop();

void rpmInterrupt();
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

  pinMode(PIN_RPM_IN, INPUT); // interrupt pin
  pinMode(PIN_VSS, INPUT); // interrupt pin

  pinMode(PIN_RPM_OUT, OUTPUT); // pwm pin

  pinMode(PIN_STATUS, OUTPUT);
  digitalWrite(PIN_STATUS, LOW);

  // init taskGear
  canGearShift = true;
  lastGearShift = millis();

  // init rpm and vss
  attachInterrupt(digitalPinToInterrupt(PIN_RPM_IN), rpmInterrupt, FALLING);
  //attachInterrupt(digitalPinToInterrupt(PIN_VSS), vssInterrupt, FALLING);

  // init lcd
  lastLCDPrint = millis();
  lcd.begin();
  lcd.backlight();

#ifdef TASK_SERIAL
  Serial.begin(9600);
#endif
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
    lastCut = millis();

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
  if (canCut && millis() - lastCut > CUT_DELAY) {
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

void rpmInterrupt() {
  rpmPulses++;
}

void taskRPM() {
  if (startingUp) {
    // analogWrite(PIN_RPM_OUT, millis() / (float) STARTING_UP_DELAY * 255.0);
    return;
  }
  
  // read rpm
  
  if (millis() - lastRPM > RPM_CHECK_DURATION) {
    lastRPM = millis();

    rpm = rpmPulses / (float) RPM_CHECK_DURATION * 60000.0; // 1000 * 60
    if (rpm > MAX_RPM) rpm = MAX_RPM;
    rpmPulses = 0;
  }
  
  // write rpm
  
  /// TEST
  /*
  int wave = (sin(millis() / 100.0) + 1.0) / 2.0 * 160.0 + 256.0 - 160.0;
  if (wave > 255) {
    digitalWrite(PIN_RPM_OUT, HIGH);
  } else {
    //digitalWrite(PIN_RPM_OUT, LOW);
    analogWrite(PIN_RPM_OUT, wave);
  }
  */
  /// TEST

  analogWrite(PIN_RPM_OUT, map(rpm, 0, MAX_RPM, 0, 255));
  
  if (!warningMode) {
    warningMode = rpm >= WARN_RPM;
  } else {
    if (millis() - lastBlink > BLINK_RATE) {
      lastBlink = millis();
      blinkState = !blinkState;
    }

    if (blinkState) {
      
    } else {
      
    }

    if (rpm < WARN_RPM) {
      warningMode = false;
      blinkState = false;
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
  if (startingUp) {
    // print credit (?)
    lcd.setCursor(0, 0);
    lcd.print("A-FA formula 600");
    lcd.setCursor(0, 1);
    lcd.print("quick-shifter");
    lcd.print("                      ");
  } else if (millis() - lastLCDPrint > LCD_PRINT_DELAY) {
    lastLCDPrint = millis();
    
    lcd.setCursor(0, 0);
    lcd.print("up ");
    lcd.print(gearUpBtn.getState());
    lcd.print(" down ");
    lcd.print(gearDownBtn.getState());
    lcd.print("               ");
    
    lcd.setCursor(0, 1);
    lcd.print("rpm ");
    lcd.print(rpm);
    lcd.print(" vss ");
    lcd.print(pulses);
    
    lcd.print("               ");
    /*
    lcd.print("pulses : ");
    lcd.print(pulses);
    */
  }
}

#ifdef TASK_SERIAL
void taskSerial() {
  if (millis() - lastSerial > SERIAL_DELAY) {
    lastSerial = millis();
    
    Serial.print("gearUp ");
    Serial.print(gearUpBtn.getState());
    Serial.print(" gearDown ");
    Serial.print(gearDownBtn.getState());
    Serial.println();
  }
}
#endif

/******************************************************************************
 * loop
 ******************************************************************************/

/// TEST
unsigned long asdf;
bool asdfMode;
/// TEST

void loop() {
  // starting up delay
  if (startingUp && millis() > STARTING_UP_DELAY) startingUp = false;

  // tasks
  taskButton();
  taskGear();
  taskRPM();
  taskVSS();
  taskLCD();
#ifdef TASK_SERIAL
  taskSerial();
#endif

  /// TEST
  if (millis() - asdf > 500) {
    asdf = millis();
    asdfMode = !asdfMode;
  }
  digitalWrite(PIN_STATUS, asdfMode);
  /// TEST
}
