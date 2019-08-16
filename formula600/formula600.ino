/*
 * A-FA 2019 Formula 600cc Arduino Box
 * 
 * By Jisu Sim(rlj1202@gmail.com), Department of Software Engineering,
 *   At Ajou Univ.
 * Copyright 2019. All rights reserved.
 * 
 * Board: Arduino Uno
 * Engine: CBR600RR Honda 2008
 * Features:
 *   - gear up, down quick shifting using injection cut(or fuel cut)
 *   - rpm check (calibrating needed)
 *   - speed check (not implemented)
 *   - gear calculation using gear ratio (not implemented)
 */

/*
 * TODO List
 *   - gear meter using rpm and vs sensor
 */

/*
 * WISH List ?
 *   - launch control (control rpm at start and clutch)
 *       control position of clutch is very difficult (almost impossible).
 *       So rather control position linearly, divide position into 3. Pushed,
 *       not pushed, half-pushed.
 *   - wing control (due to down-force in corner)
 *   - neutral gear detection
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

// engine specific constants
const int ENGINE_CYLINDERS = 4;
const int ENGINE_STROKES = 4;

//   gear ratio = in / out
const char GEAR_PATTERN[7] = {'1', 'N', '2', '3', '4', '5', '6'};
const float GEAR_PRIMARY = 2.111; // 76/36
const float GEAR_FINAL = 2.625; // 42/16
const float GEAR_RATIO[6] = {
  2.750, // 33/12
  2.000, // 32/16
  1.667, // 30/18
  1.444, // 26/18
  1.304, // 30/23
  1.208  // 29/24
};

Button gearUpBtn(PIN_BTN_GEAR_UP, HIGH);
Button gearDownBtn(PIN_BTN_GEAR_DOWN, HIGH);

const unsigned long DEBOUNCE_DELAY = 50; // milli sec

// 아두이노가 전원이 들어온 직후에 약간의 텀을 두어서 오작동을 줄이고자 함.
// to make arduino waits until system gets stabilized
bool startingUp = true;
const int STARTING_UP_DELAY = 800;

// taskGear
//
//        0         50        100       150       200       250       300 (ms)
//        |---------|---------|---------|---------|---------|---------|
// button *--*
// gear   *-----------------------------------------------------------*
// cut              *---------*
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
const unsigned long GEAR_UP_DELAY = 0; // IMPORTANT
const unsigned long GEAR_DOWN_DELAY = 0; // IMPORTANT
const unsigned long GEAR_UP_RETURN_DELAY = 300; // IMPORTANT
const unsigned long GEAR_DOWN_RETURN_DELAY = 300; // IMPORTANT

bool canCut = false;
bool canCutReturn = false;
unsigned long lastCut;
unsigned long lastCutReturn;
const unsigned long CUT_DELAY = 50; // IMPORTANT
const unsigned long CUT_RETURN_DELAY = 50; // IMPORTANT

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
const unsigned long RPM_CHECK_DURATION = 100;

const int MAX_RPM_COUNT = 10; // variables for interpolating rpm over time
float rpms[MAX_RPM_COUNT];
int rpm_ptr;
float rpm_accum;
float rpm;

bool warningMode = false;
unsigned long lastBlink;
bool blinkState;
const unsigned long BLINK_RATE = 200;
const int WARN_RPM = 13000;
const unsigned long WARN_DURATION = 1000;

// taskVSS
int vssPulses;
unsigned long lastVSS;
const unsigned long VSS_CHECK_DURATION = 100;

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
  if (canGearDownReturn
      && millis() - lastGearDownReturn > GEAR_DOWN_RETURN_DELAY) {
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
  if (startingUp) return;
  
  // read rpm
  
  if (millis() - lastRPM > RPM_CHECK_DURATION) {
    lastRPM = millis();

    // calculate new rpm
    float new_rpm = rpmPulses / (float) RPM_CHECK_DURATION * 60000.0; // 1000 * 60
    if (rpm > MAX_RPM) rpm = MAX_RPM;
    rpmPulses = 0;

    // accumulate recent rpms
    rpm_accum -= rpms[rpm_ptr];
    rpm_accum += new_rpm;
    rpms[rpm_ptr] = new_rpm;
    rpm_ptr = (rpm_ptr + 1) % MAX_RPM_COUNT; // move pointer to next

    // update rpm
    rpm = rpm_accum / (float) MAX_RPM_COUNT; // average rpm
  }
  
  // write rpm to indicator

  analogWrite(PIN_RPM_OUT, map(rpm, 0, MAX_RPM, 0, 255));

  // rpm warning function
  
  if (!warningMode) {
    warningMode = rpm >= WARN_RPM;
  } else {
    if (millis() - lastBlink > BLINK_RATE) {
      lastBlink = millis();
      blinkState = !blinkState;
    }

    if (blinkState) analogWrite(PIN_RPM_OUT, 0);

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
  vssPulses++;
}

void taskVSS() {
  if (startingUp) return;

  if (millis() - lastVSS > VSS_CHECK_DURATION) {
    lastVSS = millis();

    // TODO do something with vssPulses
    vssPulses = 0;
  }
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
    lcd.print(vssPulses);
    
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
