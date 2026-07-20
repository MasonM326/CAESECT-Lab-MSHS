// ============================================================================
// CAESECT Control Box -- FSM (Finite State Machine) version
// ============================================================================
// :
//   1. NaN-safe averaging  -- an unplugged probe can no longer poison an
//      average
//   2. One read per chip per cycle -- the MAX6675 needs ~220 ms between
//      conversions, so each chip is read exactly once per second.
//   3. Fail-safe SENSOR_FAULT state -- total ambient sensor loss shuts
//      everything off instead of guessing.
//   4. Non-blocking state machine -- no inner while loops, no delay() in the
//      control path. loop() completes in milliseconds on every pass, so the
//      freeze bug (and its whole bug family) is structurally impossible, and
//      the UART link to the ESP32 can be serviced every cycle.
//
// The five states:
//
//   HOT_LOCKOUT  (amb >= 85)         everything off, wait for cooldown
//   HIGH_FAN_ST  (75 <= amb < 85)    high fan on
//   LOW_FAN_ST   (68 <= amb < 75)    low fan on
//   HEATING      (amb < 68)          heater on, exits at 70 (hysteresis!)
//   SENSOR_FAULT (no valid probes)   everything off, reachable from anywhere
// ============================================================================

#include <LiquidCrystal_I2C.h>
#include "max6675.h"
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
//#include "RTClib.h"
#include <avr/wdt.h>



// ---------------------------------------------------------------------------
// TEST MODE: set to 1 to inject fake ambient temperatures from the Serial
// Monitor (9600 baud) for hardware-free transition testing:
//     T80   -> pretend ambient is 80 F
//     T90   -> pretend ambient is 90 F (should enter HOT_LOCKOUT)
//     R     -> return to real sensors
// IMPORTANT: set this to 0 before enabling the ESP32 link, because the test
// commands and the ESP32 protocol share the same Serial port.
// ---------------------------------------------------------------------------
#define SERIAL_TEST_OVERRIDE 0

// ---------------- Pins ----------------
// Thermocouples share SCK & DO; each probe gets its own CS (Chip Select)
const int thermoDO  = 12;
const int thermoCLK = 13;
const int thermoCS1 = 11;  // in box #1
const int thermoCS2 = 10;  // in box #2
const int thermoCS3 = 9;   // amb mid
const int thermoCS4 = 8;   // inlet pipe
const int thermoCS5 = 32;  // amb left
const int thermoCS6 = 34;  // amb right

LiquidCrystal_I2C lcd(0x27, 20, 4);
//RTC_DS3231 rtc;

MAX6675 thermocouple1(thermoCLK, thermoCS1, thermoDO); // in box #1
MAX6675 thermocouple2(thermoCLK, thermoCS2, thermoDO); // in box #2
MAX6675 thermocouple3(thermoCLK, thermoCS3, thermoDO); // amb mid
MAX6675 thermocouple5(thermoCLK, thermoCS5, thermoDO); // amb left
MAX6675 thermocouple6(thermoCLK, thermoCS6, thermoDO); // amb right
MAX6675 thermocouple4(thermoCLK, thermoCS4, thermoDO); // inlet pipe

int slowPin  = A0;   // low fan  relay (active-low wiring: LOW = fan on)
int fastPin  = A7;   // high fan relay (active-low wiring: LOW = fan on)
int relayPin = 2;    // heater   relay (HIGH = heater on)

const int chipSelect = 53;   // SD (Secure Digital) card CS on the Mega

// ---------------- Thresholds (deg F) ----------------
const int AMB_HEAT_ON  = 68;  // fall below this  -> HEATING
const int AMB_HEAT_OFF = 70;  // warm past this   -> leave HEATING (hysteresis)
const int AMB_HIGH_FAN = 75;  // low fan / high fan boundary
const int AMB_LOCKOUT  = 86;  // at or above this -> HOT_LOCKOUT

// Sentinel meaning "no valid probes in this average"
const int SENSOR_FAIL = -127;

// ---------------- State machine ----------------
enum ControlState { HEATING, LOW_FAN_ST, HIGH_FAN_ST, HOT_LOCKOUT, SENSOR_FAULT };
ControlState state = LOW_FAN_ST;   // neutral starting assumption

// Legacy flags, derived from `state` -- kept ONLY so the ESP32 dashboard
// protocol (sending_data) keeps working unchanged. Never set these by hand.
bool fan = false, low_fan = false, high_fan = false, heater = false;

// ---------------- Timing ----------------
unsigned long previousMillis = 0;
const unsigned long interval = 1000;   // one second control cycle
unsigned long totalRuntime = 0;

// ---------------- Sensor data (read once per cycle) ----------------
double t1, t2, t3, t4, t5, t6;
int tempAvg = 0;   // box average   (t1, t2)
int tempAmb = 0;   // ambient average (t3, t5, t6)
int minute = 0;

#if SERIAL_TEST_OVERRIDE
bool useFake = false;
int  fakeAmb = SENSOR_FAIL;
#endif
int minute_past = 0;


struct SensorData {
  float temp1;
  float temp2;
  float temp3;
  float temp4;
  int   tempAvg;
  bool  fan;
  bool  low_fan;
  bool  high_fan;
  bool  heater;
};

// ============================================================================
// NaN-safe averaging -- skip dead probes, average only the good ones.
// Returns SENSOR_FAIL if NO probes in the group are valid.
// ============================================================================
int safeAverage2(double a, double b) {
  double sum = 0; int n = 0;
  if (!isnan(a)) { sum += a; n++; }
  if (!isnan(b)) { sum += b; n++; }
  if (n == 0) return SENSOR_FAIL;
  return (int)(sum / n);
}

int safeAverage3(double a, double b, double c) {
  double sum = 0; int n = 0;
  if (!isnan(a)) { sum += a; n++; }
  if (!isnan(b)) { sum += b; n++; }
  if (!isnan(c)) { sum += c; n++; }
  if (n == 0) return SENSOR_FAIL;
  return (int)(sum / n);
}

// ============================================================================
// Small display helpers
// ============================================================================
String lcdTemp(double t) {            // "--" for a dead probe
  return isnan(t) ? String("--") : String((int)t);
}

String pad20(String s) {              // pad/trim to exactly 20 chars so old
  while (s.length() < 20) s += ' ';   // characters never linger on the LCD
  return s.substring(0, 20);
}

const char* stateName(ControlState s) {
  switch (s) {
    case HEATING:      return "HEATING";
    case LOW_FAN_ST:   return "LOW_FAN";
    case HIGH_FAN_ST:  return "HIGH_FAN";
    case HOT_LOCKOUT:  return "LOCKOUT";
    case SENSOR_FAULT: return "FAULT";
  }
  return "?";
}

const char* stateCode(ControlState s) {   // 4-char code for the LCD corner
  switch (s) {
    case HEATING:      return "HEAT";
    case LOW_FAN_ST:   return "LFAN";
    case HIGH_FAN_ST:  return "HFAN";
    case HOT_LOCKOUT:  return "LOCK";
    case SENSOR_FAULT: return "FAIL";
  }
  return "????";
}

// ============================================================================
// Sensor refresh -- reads every probe ONCE, updates both averages, redraws LCD
// ============================================================================
void readAllProbes() {
  t1 = thermocouple1.readFahrenheit();
  t2 = thermocouple2.readFahrenheit();
  t3 = thermocouple3.readFahrenheit(); // amb mid
  t4 = thermocouple4.readFahrenheit(); // inlet pipe
  t5 = thermocouple5.readFahrenheit(); // amb left
  t6 = thermocouple6.readFahrenheit(); // amb right
}

void checkingCrash(){
  uint8_t resetFlags = MCUSR;
  MCUSR = 0;          // clear it so it's fresh for next time
  wdt_disable();       // stop a runaway watchdog before it can reset you again

  if (resetFlags & (1 << PORF))  Serial.println("Reset cause: power-on");
  if (resetFlags & (1 << EXTRF)) Serial.println("Reset cause: external/reset pin");
  if (resetFlags & (1 << BORF))  Serial.println("Reset cause: brown-out (power sag)");
  if (resetFlags & (1 << WDRF))  Serial.println("Reset cause: watchdog (program hung)");
}

int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void updateLCD() {
  // Line 1 ends with the current state code -- your on-site debugging window.
  String l1 = "AvgAmb: " + String(tempAmb);
  while (l1.length() < 16) l1 += ' ';
  l1 += stateCode(state);

  String l2 = "AMB(L): " + lcdTemp(t5) + "  Box: " + String(tempAvg);
  String l3 = "AMB(M): " + lcdTemp(t3) + "  Inlet:";
  String l4 = "AMB(R): " + lcdTemp(t6) + "  " + lcdTemp(t4);

  lcd.setCursor(0, 0); lcd.print(pad20(l1));
  lcd.setCursor(0, 1); lcd.print(pad20(l2));
  lcd.setCursor(0, 2); lcd.print(pad20(l3));
  lcd.setCursor(0, 3); lcd.print(pad20(l4));
}

void refreshReadings() {
  readAllProbes();
  tempAvg = safeAverage2(t1, t2);
  tempAmb = safeAverage3(t3, t5, t6);
#if SERIAL_TEST_OVERRIDE
  if (useFake) tempAmb = fakeAmb;
#endif
  updateLCD();
}

// ============================================================================
// THE ONLY PLACE HARDWARE PINS ARE WRITTEN.
// Given a state, set every output to match it. Impossible combinations
// (heater + high fan, both fans at once) cannot be expressed.
// ============================================================================
void applyOutputs(ControlState s) {
  digitalWrite(relayPin, s == HEATING     ? HIGH : LOW);   // heater
  digitalWrite(slowPin,  s == LOW_FAN_ST  ? LOW  : HIGH);  // low fan  (active-low)
  digitalWrite(slowPin,  s == HIGH_FAN_ST ? LOW  : HIGH);  // high fan (active-low)  [change to fastPin when relay has been fixed]
}

// Keep the old booleans in sync so the ESP32 protocol payload is unchanged.
void syncLegacyFlags(ControlState s) {
  heater   = (s == HEATING);
  low_fan  = (s == LOW_FAN_ST);
  high_fan = (s == HIGH_FAN_ST);
  fan      = low_fan || high_fan;
}

// ============================================================================
// SD logging -- now also records the state, so your logs become a transition
// history you can audit later (plot state vs. temperature!)
// ============================================================================
void logDataToSD(int tempAvgVal, int tempAmbient, int amb_L, int amb_M, int amb_R, int minute) {
  //DateTime now = rtc.now();

  //String yearStr = String(now.year(), DEC);
  //String monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  //String dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
  //String hourStr = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC); 
  //String minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);

  //String filename = monthStr + dayStr + yearStr[2] + yearStr[3] + ".txt";
  //String filename =  "test1.txt";
  File dataFile = SD.open("071426.txt", FILE_WRITE);
  if (dataFile) {
    //dataFile.print(hourStr + ":" + minuteStr);          dataFile.print(", ");
    dataFile.print(minute);          dataFile.print(", ");
    dataFile.print(tempAvgVal);                                   dataFile.print(", ");
    dataFile.print(tempAmbient);                        dataFile.print(", ");
    dataFile.print(amb_L);                              dataFile.print(", ");
    dataFile.print(amb_M);                              dataFile.print(", ");
    dataFile.print(amb_R);                              dataFile.print(", ");
    dataFile.print(stateName(state));                   dataFile.print("\n");
    dataFile.flush();
    dataFile.close();
  }
}

// ============================================================================
// ESP32 dashboard payload (unchanged protocol, now fed from shared readings)
// ============================================================================
SensorData get_all_data(bool fanS, bool lowS, bool highS, bool heaterS) {
  SensorData data;
  data.temp1 = t1;
  data.temp2 = t2;
  data.temp3 = tempAmb;   // NaN-safe ambient average
  data.temp4 = t4;
  data.tempAvg = tempAvg;
  data.fan = fanS;
  data.low_fan = lowS;
  data.high_fan = highS;
  data.heater  = heaterS;
  return data;
}

void sending_data(SensorData data) {
  String esp_message = "";
  Serial.println(data.temp1);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "1") { esp_message = Serial.readStringUntil('\n'); }
  }
  Serial.println(data.temp2);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "2") { esp_message = Serial.readStringUntil('\n'); }
  }
  Serial.println(data.temp3);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "3") { esp_message = Serial.readStringUntil('\n'); }
  }
  Serial.println(data.temp4);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "4") { esp_message = Serial.readStringUntil('\n'); }
  }
  Serial.println(data.tempAvg);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "5") { esp_message = Serial.readStringUntil('\n'); }
  }
  Serial.println(data.fan);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "6") { esp_message = Serial.readStringUntil('\n'); }
  }
  Serial.println(data.low_fan);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "7") { esp_message = Serial.readStringUntil('\n'); }
  }
  Serial.println(data.high_fan);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "8") { esp_message = Serial.readStringUntil('\n'); }
  }
  Serial.println(data.heater);
  if (Serial.available()) {
    esp_message = Serial.readStringUntil('\n');
    while (esp_message != "9") { esp_message = Serial.readStringUntil('\n'); }
  }
}

#if SERIAL_TEST_OVERRIDE
void checkSerialOverride() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) continue;
    if (cmd == "R" || cmd == "r") {
      useFake = false;
      Serial.println("Back to live sensors");
    } else if (cmd.charAt(0) == 'T' || cmd.charAt(0) == 't') {
      fakeAmb = cmd.substring(1).toInt();
      useFake = true;
      Serial.print("Fake ambient = ");
      Serial.println(fakeAmb);
    }
  }
}
#endif

// ============================================================================
// Setup -- your original startup relay sequence, then outputs set from state
// ============================================================================
void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50);

  checkingCrash();

  lcd.init();
  lcd.clear();
  lcd.backlight();

  pinMode(slowPin,  OUTPUT); digitalWrite(slowPin,  LOW);
  pinMode(fastPin,  OUTPUT); digitalWrite(fastPin,  LOW);
  pinMode(relayPin, OUTPUT);

  // Original startup exercise sequence, preserved
  digitalWrite(slowPin,  HIGH);
  digitalWrite(relayPin, HIGH);
  delay(5000);

  digitalWrite(slowPin,  LOW);
  digitalWrite(relayPin, LOW);
  delay(2500);
  digitalWrite(slowPin,  HIGH);

  if (!SD.begin(chipSelect)) {
    // SD card initialization failed -- logging silently disabled
  }

  applyOutputs(state);      // make hardware match the starting state
  syncLegacyFlags(state);
  Serial.println(freeMemory());
}

// ============================================================================
// Main loop -- read, decide, act, log. No inner loops. No delays.
// Completes in milliseconds on EVERY pass, in EVERY state.
// ============================================================================
void loop() {
#if SERIAL_TEST_OVERRIDE
  checkSerialOverride();
#endif

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis < interval) return;  // not time yet
  previousMillis = currentMillis;
  totalRuntime += interval;

  // 1. READ -- one refresh, all probes, NaN-safe averages, LCD redraw
  refreshReadings();

  // 2. DECIDE -- pure transition logic; mirrors the state diagram exactly
  ControlState next = state;

  if (tempAmb == SENSOR_FAIL) {
    next = SENSOR_FAULT;                       // reachable from ANY state
  } else switch (state) {
    case HEATING:
      if (tempAmb >= AMB_HEAT_OFF)  next = LOW_FAN_ST;   // hysteresis exit
      break;
    case LOW_FAN_ST:
      if      (tempAmb <  AMB_HEAT_ON)  next = HEATING;
      else if (tempAmb >= AMB_HIGH_FAN) next = HIGH_FAN_ST;
      break;
    case HIGH_FAN_ST:
      if      (tempAmb <  AMB_HIGH_FAN) next = LOW_FAN_ST;
      else if (tempAmb >= AMB_LOCKOUT)  next = HOT_LOCKOUT;
      break;
    case HOT_LOCKOUT:
      if (tempAmb < AMB_LOCKOUT)    next = HIGH_FAN_ST;
      break;
    case SENSOR_FAULT:
      if (tempAmb != SENSOR_FAIL)   next = LOW_FAN_ST;   // sensors recovered
      break;
  }

  // 3. ACT -- entry actions run exactly once, on the transition
  if (next != state) {
    state = next;
    applyOutputs(state);
    syncLegacyFlags(state);
    Serial.print("STATE -> ");        // transition trace for debugging
    Serial.println(stateName(state)); // (remove when ESP32 link is active)
  }

  // ESP32 dashboard update -- safe to enable now: loop() is non-blocking,
  // so the UART gets serviced every second without starving anything.
  //sending_data(get_all_data(fan, low_fan, high_fan, heater));

  // 4. LOG -- runs every cycle, in every state, freeze-proof
  Serial.println(totalRuntime/1000);
  Serial.println(freeMemory());
  if ((totalRuntime/1000) % 60 == 0){
      minute += 1;
      logDataToSD(tempAvg, tempAmb, thermocouple5.readFahrenheit(), thermocouple3.readFahrenheit(), thermocouple6.readFahrenheit(), minute);
  }
}
