#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// =====================================================
// PINS
// =====================================================

#define FAN_PIN     6
#define RELAY_PIN   5
#define BUZZER_PIN  9
#define BUT1_PIN    7
#define BUT2_PIN    11
#define BUT3_PIN    10
#define BUT4_PIN    8
#define LEVER_PIN   12

#define THERMISTOR_PIN A1

// OLED 0.91" -> 128x32
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32

#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// THERMISTOR (NTC) CONFIG
// =====================================================

const float SERIES_RESISTOR      = 100000.0; // Ohms
const float NOMINAL_RESISTANCE   = 75000.0; // Ohms at 30C
const float NOMINAL_TEMPERATURE  = 20.0;    // C
const float B_COEFFICIENT        = 4092.0;
const int   ADC_MAX              = 1023;

// =====================================================
// GENERAL CONFIG
// =====================================================

const unsigned long DEBOUNCE_DELAY   = 30;    // ms
const unsigned long BUZZER_TIME      = 100;   // ms beep length
const unsigned long WARNING_TIME     = 5000;  // ms lever-on warning before heating

const int MIN_TARGET_TEMP = 30;
const int MAX_TARGET_TEMP = 230;

// =====================================================
// TIME-PROPORTIONING & RAMP CONTROL (OVERSHOOT FIX)
// =====================================================

const unsigned long HEAT_WINDOW_MS = 4000;  // length of one PWM cycle
const float HEAT_DUTY_MAX  = 0.35f;         // REDUCED: hard cap on ON-fraction due to huge thermal mass
const float HEAT_PROP_BAND = 30.0f;         // INCREASED: tapers off heat earlier to prevent overshoot

// Ramping restricts how fast the target temperature climbs so the thermistor can keep up
const float RAMP_RATE_C_PER_SEC = 1.0f;     // Complies with the <1.8 C/Sec limit on the graph

unsigned long heatWindowStart = 0;
unsigned long heatOnTime      = 0;
unsigned long lastRampTime    = 0;
float dynamicSetpoint         = 25.0f;      // Slowly climbs to the actual target

// =====================================================
// REFLOW PROFILE DATA
// =====================================================

struct ProfileStage {
  int targetTemp; // C
  int duration;   // seconds, 0 = hold indefinitely
};

#define MAX_STAGES 3

bool isReflowProfile = false;
int currentProfileStages = 1;
int currentStageIndex = 0;
ProfileStage activeProfile[MAX_STAGES];

// Based on the handwritten notes and Kester graph
const ProfileStage KESTER_PROFILE[MAX_STAGES] = {
  { 150, 60 },  // Preheat / Soak Start
  { 180, 60 },  // Soak End (Notes say ~58s)
  { 210, 30 }   // Reflow Peak
};

// =====================================================
// EEPROM DATA
// =====================================================

struct Settings {
  uint8_t  magic;
  int      targetTemp; // Saved manual bake temperature
};

const uint8_t SETTINGS_MAGIC = 0xA6; // Changed to force reset with new struct
Settings settings;

// =====================================================
// INPUT DEBOUNCING (buttons + lever)
// =====================================================

struct DebouncedInput {
  uint8_t pin;
  bool activeLow;
  bool stableState;
  bool lastReading;
  unsigned long lastChangeTime;
};

DebouncedInput but1  = { BUT1_PIN,  true,  false, false, 0 };
DebouncedInput but2  = { BUT2_PIN,  true,  false, false, 0 };
DebouncedInput but3  = { BUT3_PIN,  true,  false, false, 0 };
DebouncedInput but4  = { BUT4_PIN,  true,  false, false, 0 };
DebouncedInput lever = { LEVER_PIN, false, false, false, 0 };

bool updateInput(DebouncedInput &in) {
  bool raw = digitalRead(in.pin);
  bool reading = in.activeLow ? !raw : raw;

  if (reading != in.lastReading) {
    in.lastChangeTime = millis();
  }

  bool changed = false;
  if ((millis() - in.lastChangeTime) > DEBOUNCE_DELAY) {
    if (reading != in.stableState) {
      in.stableState = reading;
      changed = true;
    }
  }

  in.lastReading = reading;
  return changed;
}

// =====================================================
// UI STATE MACHINE
// =====================================================

enum UIState {
  UI_TOP,
  UI_FAN_EDIT,
  UI_TEMP_EDIT
};

enum TopScreen {
  SCREEN_HOME = 0,
  SCREEN_FAN,
  SCREEN_TEMP,
  SCREEN_REFLOW,
  SCREEN_COUNT
};

UIState  uiState    = UI_TOP;
int      screenIndex = SCREEN_HOME;

enum FanMode { FAN_AUTO = 0, FAN_FORCED_ON, FAN_FORCED_OFF };
const char *fanModeName(FanMode m);

FanMode fanMode = FAN_AUTO;
FanMode fanModeEditBuffer = FAN_AUTO;
int tempEditBuffer = 50;

// =====================================================
// BED (RELAY) CONTROL STATE MACHINE
// =====================================================

enum BedState { BED_IDLE, BED_WARNING, BED_RAMPING, BED_SOAKING, BED_COOLING, BED_DONE };
BedState bedState = BED_IDLE;

unsigned long warningStartTime = 0;
unsigned long soakStartTime    = 0;
int  lastWarningBeepSecond = -1;

bool fanOnState   = false;
bool relayOnState = false;
float currentTempC = 25.0;

// =====================================================
// NON-BLOCKING BUZZER
// =====================================================

bool buzzerActive = false;
unsigned long buzzerOffTime = 0;

void buzzerStart(unsigned long durationMs) {
  analogWrite(BUZZER_PIN, 128);
  buzzerActive = true;
  buzzerOffTime = millis() + durationMs;
}

void buzzerUpdate() {
  if (buzzerActive && millis() >= buzzerOffTime) {
    analogWrite(BUZZER_PIN, 0);
    buzzerActive = false;
  }
}

// =====================================================
// EEPROM HELPERS
// =====================================================

void loadDefaultSettings() {
  settings.magic = SETTINGS_MAGIC;
  settings.targetTemp = 60; // Safe default for manual mode
}

void loadSettings() {
  EEPROM.get(0, settings);
  if (settings.magic != SETTINGS_MAGIC ||
      settings.targetTemp < MIN_TARGET_TEMP ||
      settings.targetTemp > MAX_TARGET_TEMP) {
    loadDefaultSettings();
    EEPROM.put(0, settings);
  }
}

void saveSettings() {
  EEPROM.put(0, settings);
}

// =====================================================
// THERMISTOR READING
// =====================================================

float readTemperatureC() {
  int raw = analogRead(THERMISTOR_PIN);
  if (raw <= 0) raw = 1;
  if (raw >= ADC_MAX) raw = ADC_MAX - 1;

  float resistance = SERIES_RESISTOR / ((float)ADC_MAX / raw - 1.0);
  float steinhart = resistance / NOMINAL_RESISTANCE;
  steinhart = log(steinhart);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  return steinhart;
}

// =====================================================
// ACTUATORS
// =====================================================

void setFan(bool on) {
  fanOnState = on;
  digitalWrite(FAN_PIN, on ? HIGH : LOW);
}

void setRelay(bool on) {
  relayOnState = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
}

void updateFanControl() {
  switch (fanMode) {
    case FAN_FORCED_ON:  setFan(true); break;
    case FAN_FORCED_OFF: setFan(false); break;
    case FAN_AUTO:
    default:
      // Auto fan logic during cooling or if current temp is highly over target
      if (bedState == BED_COOLING) {
          setFan(true);
      } else if (bedState == BED_IDLE || bedState == BED_DONE) {
          setFan(currentTempC > 50.0);
      } else {
          setFan(false); // keep fan off while actively heating
      }
      break;
  }
}

// =====================================================
// PROFILE ACTIVATION
// =====================================================

void applyManualTarget(int newTargetTemp) {
  currentProfileStages = 1;
  activeProfile[0] = { newTargetTemp, 0 }; // 0 duration = hold indefinitely
  isReflowProfile = false;
  bedState = BED_IDLE;
}

void applyReflowProfile() {
  currentProfileStages = MAX_STAGES;
  for(int i=0; i<MAX_STAGES; i++) {
    activeProfile[i] = KESTER_PROFILE[i];
  }
  isReflowProfile = true;
  bedState = BED_IDLE;
}

// =====================================================
// CONTROL LOGIC (WITH SETPOINT RAMPING)
// =====================================================

void updateHeatOutput(float targetTemp) {
  unsigned long now = millis();
  unsigned long elapsedInWindow = now - heatWindowStart;

  if (elapsedInWindow >= HEAT_WINDOW_MS) {
    float error = targetTemp - currentTempC;
    // Duty cycle calculation
    float duty = constrain(error / HEAT_PROP_BAND, 0.0f, 1.0f) * HEAT_DUTY_MAX;
    heatOnTime = (unsigned long)(duty * HEAT_WINDOW_MS);
    heatWindowStart = now;
    elapsedInWindow = 0;
  }

  setRelay(elapsedInWindow < heatOnTime);
}

void updateBedControl() {
  bool leverChanged = updateInput(lever);
  (void)leverChanged;

  if (!lever.stableState) {
    if (bedState != BED_IDLE) {
      setRelay(false);
      bedState = BED_IDLE;
    }
    return;
  }

  switch (bedState) {

    case BED_IDLE:
      bedState = BED_WARNING;
      warningStartTime = millis();
      lastWarningBeepSecond = -1;
      break;

    case BED_WARNING: {
      unsigned long elapsed = millis() - warningStartTime;
      int currentSecond = elapsed / 1000;
      if (currentSecond != lastWarningBeepSecond) {
        lastWarningBeepSecond = currentSecond;
        buzzerStart(BUZZER_TIME);
      }
      if (elapsed >= WARNING_TIME) {
        bedState = BED_RAMPING;
        currentStageIndex = 0;
        dynamicSetpoint = currentTempC; // Initialize soft-start at current room temp
        lastRampTime = millis();
        heatWindowStart = millis();
      }
      break;
    }

    case BED_RAMPING: {
      // 1. Advance the dynamic setpoint (Soft Start)
      unsigned long now = millis();
      float dt_sec = (now - lastRampTime) / 1000.0f;
      lastRampTime = now;
      
      dynamicSetpoint += RAMP_RATE_C_PER_SEC * dt_sec;
      
      int stageTarget = activeProfile[currentStageIndex].targetTemp;
      if (dynamicSetpoint > stageTarget) {
        dynamicSetpoint = stageTarget; // Cap at the target for this stage
      }

      // 2. Control heat based on the *dynamic* setpoint, not the final target
      updateHeatOutput(dynamicSetpoint);

      // 3. Move to soak when actual temperature reaches the stage target
      if (currentTempC >= stageTarget) {
        if (activeProfile[currentStageIndex].duration > 0) {
          bedState = BED_SOAKING;
          soakStartTime = millis();
        } else {
          // If duration is 0 (Manual mode), stay in ramping/holding indefinitely
          dynamicSetpoint = stageTarget; 
        }
      }
      break;
    }

    case BED_SOAKING:
      // Hold exactly at the stage target
      updateHeatOutput(activeProfile[currentStageIndex].targetTemp);

      if (millis() - soakStartTime >= (unsigned long)activeProfile[currentStageIndex].duration * 1000UL) {
        currentStageIndex++;
        if (currentStageIndex >= currentProfileStages) {
          bedState = BED_COOLING; // All stages complete
        } else {
          bedState = BED_RAMPING; // Move to next heating stage
          lastRampTime = millis();
        }
      }
      break;

    case BED_COOLING:
      setRelay(false);
      if (currentTempC <= 50.0) {
        bedState = BED_DONE;
        buzzerStart(500); // Long beep to indicate done
      }
      break;

    case BED_DONE:
      setRelay(false);
      break;
  }
}

// =====================================================
// BUTTON HANDLING / NAVIGATION
// =====================================================

void handleTopNavigation(bool p1, bool p2, bool p3, bool p4) {
  if (p1) screenIndex = (screenIndex + SCREEN_COUNT - 1) % SCREEN_COUNT;
  if (p2) screenIndex = (screenIndex + 1) % SCREEN_COUNT;

  if (p3) {
    switch (screenIndex) {
      case SCREEN_FAN:
        fanModeEditBuffer = fanMode;
        uiState = UI_FAN_EDIT;
        break;
      case SCREEN_TEMP:
        tempEditBuffer = settings.targetTemp;
        uiState = UI_TEMP_EDIT;
        break;
      case SCREEN_REFLOW:
        applyReflowProfile(); // Instantly selects the reflow profile
        buzzerStart(BUZZER_TIME);
        screenIndex = SCREEN_HOME;
        break;
      case SCREEN_HOME:
      default:
        break; 
    }
  }

  if (p4) {
    screenIndex = SCREEN_HOME;
  }
}

void handleFanEdit(bool p1, bool p2, bool p3, bool p4) {
  if (p1) fanModeEditBuffer = (FanMode)((fanModeEditBuffer + 1) % 3);
  if (p2) fanModeEditBuffer = (FanMode)((fanModeEditBuffer + 2) % 3);
  if (p3) {
    fanMode = fanModeEditBuffer;
    buzzerStart(BUZZER_TIME);
    uiState = UI_TOP;
  }
  if (p4) uiState = UI_TOP;
}

void handleTempEdit(bool p1, bool p2, bool p3, bool p4) {
  if (p1) tempEditBuffer = min(tempEditBuffer + 1, MAX_TARGET_TEMP);
  if (p2) tempEditBuffer = max(tempEditBuffer - 1, MIN_TARGET_TEMP);
  if (p3) {
    settings.targetTemp = tempEditBuffer;
    saveSettings();
    applyManualTarget(settings.targetTemp); // Sets manual mode
    buzzerStart(BUZZER_TIME);
    uiState = UI_TOP;
  }
  if (p4) uiState = UI_TOP;
}

void handleButtons() {
  bool c1 = updateInput(but1);
  bool c2 = updateInput(but2);
  bool c3 = updateInput(but3);
  bool c4 = updateInput(but4);

  bool p1 = c1 && but1.stableState;
  bool p2 = c2 && but2.stableState;
  bool p3 = c3 && but3.stableState;
  bool p4 = c4 && but4.stableState;

  if (!(p1 || p2 || p3 || p4)) return;

  switch (uiState) {
    case UI_TOP:       handleTopNavigation(p1, p2, p3, p4); break;
    case UI_FAN_EDIT:  handleFanEdit(p1, p2, p3, p4);       break;
    case UI_TEMP_EDIT: handleTempEdit(p1, p2, p3, p4);      break;
  }
}

// =====================================================
// DISPLAY
// =====================================================

const char *fanModeName(FanMode m) {
  switch (m) {
    case FAN_FORCED_ON:  return "ON";
    case FAN_FORCED_OFF: return "OFF";
    case FAN_AUTO:
    default:              return "AUTO";
  }
}

void drawLogo() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(14, 4);
  display.print("PCB HEATED BED");
  display.setCursor(30, 18);
  display.print("v3  booting...");
  display.display();
}

void drawHome() {
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(8, 3);
  
  // Show Current Mode and Status
  if (isReflowProfile) {
    if (bedState == BED_RAMPING)        display.print("REFLOW: RAMPING");
    else if (bedState == BED_SOAKING)   display.print("REFLOW: SOAKING");
    else if (bedState == BED_COOLING)   display.print("REFLOW: COOLING");
    else if (bedState == BED_DONE)      display.print("REFLOW: DONE");
    else                                display.print("READY: REFLOW SOLDER");
  } else {
    display.print("READY: MANUAL BAKE");
  }
  display.drawFastHLine(6, 12, SCREEN_WIDTH - 12, SSD1306_WHITE);

  char buf[8];
  dtostrf(currentTempC, 4, 1, buf);
  display.setTextSize(2);
  display.setCursor(28, 16);
  display.print(buf);
  display.print("C");
}

void drawFanScreen() {
  display.setCursor(0, 0);
  display.print("MANUAL FAN CONTROL");
  display.setCursor(0, 8);
  display.setTextSize(2);
  display.print(fanModeName(fanMode));
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print("BUT3=edit BUT4=back");
}

void drawFanEditScreen() {
  display.setCursor(0, 0);
  display.print("SET FAN MODE");
  display.setCursor(0, 8);
  display.setTextSize(2);
  display.print(fanModeName(fanModeEditBuffer));
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print("3=OK  4=cancel");
}

void drawTempScreen() {
  display.setCursor(0, 0);
  display.print("MANUAL TARGET TEMP");
  display.setCursor(0, 12);
  display.setTextSize(2);
  display.print(settings.targetTemp);
  display.print("C");
  display.setTextSize(1);
  display.setCursor(70, 20);
  display.print("Now:");
  display.print((int)currentTempC);
}

void drawTempEditScreen() {
  display.setCursor(0, 0);
  display.print("SET MANUAL TEMP");
  display.setCursor(0, 8);
  display.setTextSize(2);
  display.print(tempEditBuffer);
  display.print("C");
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print("1=+ 2=- 3=OK 4=X");
}

void drawReflowScreen() {
  display.setCursor(0, 0);
  display.print("REFLOW PROFILE");
  display.setCursor(0, 10);
  display.print("Kester Sn63Pb37");
  display.setCursor(0, 24);
  display.print("BUT3=Select BUT4=Back");
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  if (uiState == UI_TOP) {
    switch (screenIndex) {
      case SCREEN_HOME:   drawHome();        break;
      case SCREEN_FAN:    drawFanScreen();   break;
      case SCREEN_TEMP:   drawTempScreen();  break;
      case SCREEN_REFLOW: drawReflowScreen();break;
    }
  } else {
    switch (uiState) {
      case UI_FAN_EDIT:  drawFanEditScreen();  break;
      case UI_TEMP_EDIT: drawTempEditScreen(); break;
      default: break;
    }
  }

  if (bedState == BED_WARNING) {
    unsigned long remaining = WARNING_TIME - (millis() - warningStartTime);
    unsigned long secondsLeft = (remaining / 1000) + 1;
    char buf[16];
    snprintf(buf, sizeof(buf), "BED ON IN %lus", secondsLeft);

    const int boxX = 4;
    const int boxY = 1;
    const int boxW = SCREEN_WIDTH - (boxX * 2);
    const int boxH = 12;

    display.fillRect(boxX, boxY, boxW, boxH, SSD1306_BLACK);
    display.drawRect(boxX, boxY, boxW, boxH, SSD1306_WHITE);

    int16_t x1, y1;
    uint16_t textW, textH;
    display.setTextSize(1);
    display.getTextBounds(buf, 0, 0, &x1, &y1, &textW, &textH);
    int textX = boxX + (boxW - (int)textW) / 2;
    int textY = boxY + (boxH - (int)textH) / 2 + 1;

    display.setCursor(textX, textY);
    display.print(buf);
  }

  display.display();
}

// =====================================================
// SETUP & LOOP
// =====================================================

void setup() {
  pinMode(FAN_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(BUT1_PIN, INPUT_PULLUP);
  pinMode(BUT2_PIN, INPUT_PULLUP);
  pinMode(BUT3_PIN, INPUT_PULLUP);
  pinMode(BUT4_PIN, INPUT_PULLUP);
  pinMode(LEVER_PIN, INPUT_PULLUP);

  digitalWrite(FAN_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  analogWrite(BUZZER_PIN, 0);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while (true) { digitalWrite(RELAY_PIN, LOW); delay(1000); }
  }

  drawLogo();
  delay(2000);

  loadSettings();
  
  // Default to manual bake state on boot
  applyManualTarget(settings.targetTemp); 
  currentTempC = readTemperatureC();
  updateDisplay();
}

void loop() {
  currentTempC = readTemperatureC();

  handleButtons();
  updateFanControl();
  updateBedControl();
  buzzerUpdate();
  updateDisplay();

  delay(20);
}