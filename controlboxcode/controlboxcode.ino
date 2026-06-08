#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>

// ── STUB: thermocouple hardware replaced with random data for testing ──
// MAX6675 includes and instances removed; pins kept as comments for reference
// const int thermoDO  = 12;
// const int thermoCLK = 13;
// const int thermoCS1 = 11;
// const int thermoCS2 = 10;
// const int thermoCS3 = 9;
// const int thermoCS4 = 8;

LiquidCrystal_I2C lcd(0x27, 16, 2);

int slowPin  = A0;
int fastPin  = A7;
int relayPin = 2;

int tempAvg = 0;
int tempAmb = 0;

int amb_cold = 68;
int cold = 75;
int hot  = 90;
int amb_hot = 70;

bool low_fan  = false;
bool high_fan = false;
bool heater   = false;
bool cold_threshold = false;
bool hot_threshold  = false;

const int chipSelect = 53;

unsigned long previousMillis = 0;
const unsigned long interval = 2000;
unsigned long totalRuntime = 0;

struct SensorData {
  float temp1;
  float temp2;
  float temp3;
  float temp4;
  int   tempAvg;
  bool  low_fan;
  bool  heater;
};

// ── Random stub: returns a float in [lo, hi] with one decimal place ──
float fakeTemp(int lo_tenths, int hi_tenths) {
  return random(lo_tenths, hi_tenths + 1) / 10.0;
}

void logDataToSD(unsigned long runtimeMillis, int tempAvg, int tempAmbient) {
  float runtimeSeconds = runtimeMillis / 1000.0;
  File dataFile = SD.open("050526.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.print(runtimeSeconds); dataFile.print(", ");
    dataFile.print(tempAvg);        dataFile.print(", ");
    dataFile.print(tempAmbient);    dataFile.print("\n");
    dataFile.flush();
    dataFile.close();
    //Serial.print("Logged to SD: ");
    //Serial.print(runtimeSeconds); Serial.print(" s, ");
    //Serial.print(tempAvg);        Serial.print(" F, ");
    //Serial.print(tempAmbient);    Serial.println(" F");
  } else {
    //Serial.println("Error opening 050526.txt");
  }
}

SensorData get_all_data(bool current_fan, bool current_heater) {
  SensorData data;
  // ── FAKE DATA: in-box sensors wander 72–92 °F ──
  data.temp1 = fakeTemp(720, 920);
  data.temp2 = fakeTemp(720, 920);
  // ── FAKE DATA: ambient ~64–70 °F, inlet pipe ~60–68 °F ──
  data.temp3 = fakeTemp(640, 700);
  data.temp4 = fakeTemp(600, 680);
  data.tempAvg = (int)((data.temp1 + data.temp2) / 2.0);
  data.low_fan = current_fan;
  data.heater  = current_heater;
  return data;
}

void sending_data(SensorData data) {
  String esp_message = "";
  
  Serial.println(data.temp1);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message =! "1") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.temp2);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message =! "2") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.temp3);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message =! "3") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.temp4);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message =! "4") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.tempAvg);
    if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message =! "5") { esp_message = Serial.readStringUntil('\n'); }
    }

  Serial.println(data.low_fan);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message =! "6") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.heater);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message =! "7") { esp_message = Serial.readStringUntil('\n'); }
  }
}


int temperature_readings() {
  // ── FAKE DATA ──
  float temp1 = fakeTemp(720, 920);
  float temp2 = fakeTemp(720, 920);
  float temp3 = fakeTemp(640, 700);  // ambient
  float temp4 = fakeTemp(600, 680);  // inlet pipe

  int avg = (int)((temp1 + temp2) / 2.0);

  String LCDOutputLine1 = "AVG: " + String(avg) + " AMB: " + String((int)temp3);
  String LCDOutputLine2 = "IN: "  + String((int)temp4);
  lcd.setCursor(0, 0); lcd.print(LCDOutputLine1);
  lcd.setCursor(0, 1); lcd.print(LCDOutputLine2);

  return avg;
}

int amb_temperature_readings() {
  // ── FAKE DATA ──
  float temp3 = fakeTemp(640, 700);  // ambient only

  // Mirror LCD update so display stays consistent
  float temp1 = fakeTemp(720, 920);
  float temp2 = fakeTemp(720, 920);
  float temp4 = fakeTemp(600, 680);
  int avg = (int)((temp1 + temp2) / 2.0);

  String LCDOutputLine1 = "AVG: " + String(avg) + " AMB: " + String((int)temp3);
  String LCDOutputLine2 = "IN: "  + String((int)temp4);
  lcd.setCursor(0, 0); lcd.print(LCDOutputLine1);
  lcd.setCursor(0, 1); lcd.print(LCDOutputLine2);

  return (int)temp3;
}

void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A1));  // seed RNG from floating pin
  //Serial.println("MAX6675 controller start [FAKE DATA MODE]");

  lcd.init();
  lcd.backlight();

  pinMode(slowPin,  OUTPUT); digitalWrite(slowPin,  LOW);
  pinMode(fastPin,  OUTPUT); digitalWrite(fastPin,  LOW);
  pinMode(relayPin, OUTPUT);

  digitalWrite(slowPin,  HIGH);
  digitalWrite(relayPin, HIGH);
  //Serial.println("Ran Heater");
  delay(5000);

  digitalWrite(slowPin,  LOW);
  digitalWrite(relayPin, LOW);
  //Serial.println("Ran high fan");
  delay(5000);

  if (!SD.begin(chipSelect)) {
    //Serial.println("SD card initialization failed!");
  } else {
    //Serial.println("SD card initialized.");
  }

  delay(500);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    totalRuntime += interval;

    tempAvg = temperature_readings();
    tempAmb = amb_temperature_readings();
    sending_data(get_all_data(low_fan, heater));
    delay(2000);

    if (tempAvg < cold) {
      if (tempAmb < amb_cold) {
        heater = true;
        if (low_fan =! false) {
          low_fan = false;
          digitalWrite(slowPin, HIGH);
          digitalWrite(relayPin, HIGH);
          temperature_readings();
          sending_data(get_all_data(low_fan, heater));
        } else {
          digitalWrite(relayPin, HIGH);
          temperature_readings();
          sending_data(get_all_data(low_fan, heater));
        }
      }
      while (tempAvg <= 80) {
        tempAvg = temperature_readings();
        sending_data(get_all_data(low_fan, heater));
        delay(2000);
      }
    } else if (tempAvg < hot && tempAvg > cold) {
      if (tempAmb > amb_hot) {
        low_fan = true;
        if (heater =! false) {
          heater = false;
          digitalWrite(relayPin, LOW);
          digitalWrite(slowPin,  LOW);
          temperature_readings();
          sending_data(get_all_data(low_fan, heater));
        } else {
          digitalWrite(slowPin, LOW);
          temperature_readings();
          sending_data(get_all_data(low_fan, heater));
        }
      }
    } else {
      digitalWrite(slowPin,  LOW);
      digitalWrite(relayPin, LOW);
      temperature_readings();
      sending_data(get_all_data(low_fan, heater));
    }

    logDataToSD(totalRuntime, tempAvg, tempAmb);
  }
}