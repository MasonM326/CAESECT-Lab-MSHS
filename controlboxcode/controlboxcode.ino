#include <LiquidCrystal_I2C.h>
#include "max6675.h"
#include <SPI.h>
#include <SD.h>
#include <Wire.h>

// Thermocouple pins (shared SCK & DO; separate CS per probe)
const int thermoDO  = 12;
const int thermoCLK = 13;
const int thermoCS1 = 11;  // in box #1
const int thermoCS2 = 10;  // in box #2
const int thermoCS3 = 9;  // amb mid
const int thermoCS4 = 8; // NEW: inlet pipe
const int thermoCS5 = 32;  // amb left
const int thermoCS6 = 34;  // amb right


LiquidCrystal_I2C lcd(0x27, 20, 4);

MAX6675 thermocouple1(thermoCLK, thermoCS1, thermoDO); // in box #1
MAX6675 thermocouple2(thermoCLK, thermoCS2, thermoDO); // in box #2
MAX6675 thermocouple3(thermoCLK, thermoCS3, thermoDO); // amb mid
MAX6675 thermocouple5(thermoCLK, thermoCS5, thermoDO); // amb left
MAX6675 thermocouple6(thermoCLK, thermoCS6, thermoDO); // amb right
MAX6675 thermocouple4(thermoCLK, thermoCS4, thermoDO); // inlet pipe (NEW)

int slowPin  = A0;
int fastPin  = A7;
int relayPin = 2;

int tempAvg = 0;
int tempAmb = 0;

int amb_low_cold = 68;
int amb_cold = 75;
int cold = 75;  // Cold threshold (°F)
int hot  = 90;  // Hot threshold (°F)
int amb_hot = 85;


bool fan = false;
bool low_fan  = false;
bool high_fan = false;
bool heater   = false;

bool heater_button = false;
bool fan_button = false;

const int chipSelect = 53;

unsigned long previousMillis = 0;
const unsigned long interval = 1000; // one second
unsigned long totalRuntime = 0;

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

void logDataToSD(unsigned long runtimeMillis, int tempAvg, int tempAmbient) {
  float runtimeSeconds = runtimeMillis / 1000.0;
  File dataFile = SD.open("070226.txt", FILE_WRITE);
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

SensorData get_all_data(bool fan, bool low_fan, bool high_fan, bool heater) {
  SensorData data;
  data.temp1 = thermocouple1.readFahrenheit();
  data.temp2 = thermocouple2.readFahrenheit();
  data.temp3 =  (int)((thermocouple3.readFahrenheit() + thermocouple5.readFahrenheit() + thermocouple6.readFahrenheit()) / 3.0);
  data.temp4 = thermocouple4.readFahrenheit();
  data.tempAvg = (int)((data.temp1 + data.temp2) / 2.0);
  data.fan = fan;
  data.low_fan = low_fan;
  data.high_fan = high_fan;
  data.heater  = heater;
  return data;
}

void sending_data(SensorData data) {
  String esp_message = "";
  Serial.println(data.temp1);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "1") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.temp2);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "2") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.temp3);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "3") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.temp4);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "4") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.tempAvg);
    if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "5") { esp_message = Serial.readStringUntil('\n'); }
    }

  Serial.println(data.fan);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "6") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.low_fan);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "7") { esp_message = Serial.readStringUntil('\n'); }
  }


  Serial.println(data.high_fan);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "8") { esp_message = Serial.readStringUntil('\n'); }
  }

  Serial.println(data.heater);
  if (Serial.available()){
  esp_message = Serial.readStringUntil('\n');
  while (esp_message != "9") { esp_message = Serial.readStringUntil('\n'); }
  }
}


// wadd whiles from webserver code

int temperature_readings() {
  double temp1 = thermocouple1.readFahrenheit();
  double temp2 = thermocouple2.readFahrenheit();
  double temp3 = thermocouple3.readFahrenheit(); // amb mid
  double temp4 = thermocouple4.readFahrenheit(); // inlet pipe (NEW)
  double temp5 = thermocouple5.readFahrenheit(); // amb left
  double temp6 = thermocouple6.readFahrenheit(); // amb right

  if (isnan(temp1) || isnan(temp2) || isnan(temp3) || isnan(temp4)) {
        //Serial.println("Error reading one or more thermocouples!");
        lcd.setCursor(0, 0);
        lcd.print("Error 1");
        //return; //may need to comment this out
      }

  int avg = (int)((temp1 + temp2) / 2.0);
  int amb_avg = (int)((temp3 + temp5 + temp6) / 3.0);

  String LCDOutputLine1 = "AvgAmb: " + String(amb_avg) + "  BoxAvg: ";
  String LCDOutputLine2 = "AMB(L): " + String((int)temp5) + "  " + String((int)avg);
  String LCDOutputLine3 = "AMB(M): "  +  String((int)temp3) + "  Inlet:";
  String LCDOutputLine4 = "AMB(R): " + String((int)temp6) + "  " + String((int)temp4);

  lcd.setCursor(0, 0); lcd.print(LCDOutputLine1);
  lcd.setCursor(0, 1); lcd.print(LCDOutputLine2);
  lcd.setCursor(0, 2); lcd.print(LCDOutputLine3);
  lcd.setCursor(0, 3); lcd.print(LCDOutputLine4);


// could make displaying lcd a function

  return avg;
}

int amb_temperature_readings() {
  double temp1 = thermocouple1.readFahrenheit();
  double temp2 = thermocouple2.readFahrenheit();
  double temp3 = thermocouple3.readFahrenheit(); // amb mid
  double temp4 = thermocouple4.readFahrenheit(); // inlet pipe (NEW)
  double temp5 = thermocouple5.readFahrenheit(); // amb left
  double temp6 = thermocouple6.readFahrenheit(); // amb right

  if (isnan(temp1) || isnan(temp2) || isnan(temp3) || isnan(temp4) || isnan(temp5) || isnan(temp6) {
        //Serial.println("Error reading one or more thermocouples!");
        lcd.setCursor(0, 0);
        lcd.print("Error 1");
        //return; //may need to comment this out
      }

  int avg = (int)((temp1 + temp2) / 2.0);
  int amb_avg = (int)((temp3 + temp5 + temp6) / 3.0);

  String LCDOutputLine1 = "AvgAmb: " + String(amb_avg) + "  BoxAvg: ";
  String LCDOutputLine2 = "AMB(L): " + String((int)temp5) + "  " + String((int)avg);
  String LCDOutputLine3 = "AMB(M): "  +  String((int)temp3) + "  Inlet:";
  String LCDOutputLine4 = "AMB(R): " + String((int)temp6) + "  " + String((int)temp4);

  lcd.setCursor(0, 0); lcd.print(LCDOutputLine1);
  lcd.setCursor(0, 1); lcd.print(LCDOutputLine2);
  lcd.setCursor(0, 2); lcd.print(LCDOutputLine3);
  lcd.setCursor(0, 3); lcd.print(LCDOutputLine4);


  return amb_avg;
}

void setup() {
  Serial.begin(9600);
  //Serial.println("MAX6675 controller start");

  lcd.init();
  lcd.clear();
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
  delay(2500);
   digitalWrite(slowPin,  HIGH);

  if (!SD.begin(chipSelect)) {
    //Serial.println("SD card initialization failed!");
  } else {
    //Serial.println("SD card initialized.");
  }

  //delay(500);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    totalRuntime += interval;


    tempAvg = temperature_readings();
    tempAmb = amb_temperature_readings();
    //sending_data(get_all_data(fan, low_fan, high_fan, heater));
    //delay(2000);

    //if (heater_button = true){
      //digitalWrite(relayPin, HIGH);
      // check for updates from heater button
    //}

  while (tempAmb >= amb_hot){ // run nothing if amb is greater than 80
      if (heater != false){
        heater = false;
        digitalWrite(relayPin, LOW);
        temperature_readings();
        //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        tempAmb = amb_temperature_readings();
        delay(1000);

      }else if (low_fan != false){
        low_fan = false;
        fan = false;
        digitalWrite(slowPin, HIGH);
        temperature_readings();
        //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        tempAmb = amb_temperature_readings();
        delay(1000);

      }else if(high_fan != false){
        high_fan = false;
        fan = false;
        digitalWrite(fastPin, HIGH);
        temperature_readings();
        //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        tempAmb = amb_temperature_readings();
        delay(1000);
      }
    }


    if (tempAmb < amb_low_cold) {
        heater = true;
        if (low_fan != false) {
          low_fan = false;
          fan = false;
          digitalWrite(slowPin, HIGH);
          digitalWrite(relayPin, HIGH);
          temperature_readings();
          //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        } else {
          high_fan = false;
          fan = false;
          digitalWrite(fastPin, HIGH);
          digitalWrite(relayPin, HIGH);
          temperature_readings();
          //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        }
      
      while (tempAmb < 70) {
        tempAmb = amb_temperature_readings();
        //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        delay(1000);
      }
    } else if (tempAmb >= amb_low_cold && tempAmb < amb_cold) {
        low_fan = true;
        fan = true;
        if (heater != false) {
          heater = false;
          digitalWrite(relayPin, LOW);
          digitalWrite(slowPin,  LOW);
          temperature_readings();
          //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        } else {
          high_fan = false;
          digitalWrite(fastPin, HIGH);
          digitalWrite(slowPin, LOW);
          temperature_readings();
          //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        }
      } 
    else if (tempAmb >= amb_cold && tempAmb < amb_hot) {
      high_fan = true;
      fan = true;
      if (heater != false) {
          heater = false;
          fan = false;
          digitalWrite(relayPin, LOW);
          digitalWrite(fastPin,  LOW);
          temperature_readings();
          //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        } else {
          low_fan = false;
          digitalWrite(slowPin, HIGH);
          digitalWrite(fastPin, LOW);
          temperature_readings();
          //sending_data(get_all_data(fan, low_fan, high_fan, heater));
        }
    }

    logDataToSD(totalRuntime, tempAvg, tempAmb);
  }
}
