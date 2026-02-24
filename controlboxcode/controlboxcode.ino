#include "max6675.h"
#include <SPI.h>
#include <SD.h>


// Thermocouple pins (shared SCK & DO; separate CS per probe)
const int thermoDO  = 12;
const int thermoCLK = 13;
const int thermoCS1 = 11;  // in box #1
const int thermoCS2 = 10;  // in box #2
const int thermoCS3 = 9;  // ambient (outside box)
const int thermoCS4 = 8; // NEW: inlet pipe


// Thermocouple instances
MAX6675 thermocouple1(thermoCLK, thermoCS1, thermoDO); // in box #1
MAX6675 thermocouple2(thermoCLK, thermoCS2, thermoDO); // in box #2
MAX6675 thermocouple3(thermoCLK, thermoCS3, thermoDO); // ambient
MAX6675 thermocouple4(thermoCLK, thermoCS4, thermoDO); // inlet pipe (NEW)


int slowPin  = 9;  // low fan output pin
int fastPin  = 3;  // high fan output pin
int relayPin = 2;  // relay control pin


int cold = 0;  // Cold threshold (°F)
int hot  = 1;  // Hot threshold (°F)


const int chipSelect = 10; // CS pin for SD card module


unsigned long previousMillis = 0;
const unsigned long interval = 5000; // 5 seconds
unsigned long totalRuntime = 0;


void setup() {
  Serial.begin(9600);
  Serial.println("MAX6675 controller start");


  pinMode(slowPin, OUTPUT);
  digitalWrite(slowPin, LOW);
  pinMode(fastPin, OUTPUT);
  digitalWrite(fastPin, LOW);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);


  // Initialize SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    // Continue running even if SD fails so control still works
  } else {
    Serial.println("SD card initialized.");
  }


  delay(500);
}


void loop() {
  unsigned long currentMillis = millis();


  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;


    // Accumulate total runtime
    totalRuntime += interval;


    // Read all thermocouples (use double for isnan checks)
    double temp1 = thermocouple1.readFahrenheit();
    double temp2 = thermocouple2.readFahrenheit();
    double temp3 = thermocouple3.readFahrenheit(); // ambient
    double temp4 = thermocouple4.readFahrenheit(); // inlet pipe (NEW)


    // Validate readings
    if (isnan(temp1) || isnan(temp2) || isnan(temp3) || isnan(temp4)) {
      Serial.println("Error reading one or more thermocouples!");
      return;
    }


    int tempAvg = (int)((temp1 + temp2) / 2.0);


    // Serial output (you’ll hook up your new TFT later)
    Serial.print("Temp1 (in box #1): "); Serial.print(temp1); Serial.println(" F");
    Serial.print("Temp2 (in box #2): "); Serial.print(temp2); Serial.println(" F");
    Serial.print("Ambient (TC3):     "); Serial.print(temp3); Serial.println(" F");
    Serial.print("Inlet Pipe (TC4):  "); Serial.print(temp4); Serial.println(" F");
    Serial.print("Average (1&2):     "); Serial.print(tempAvg); Serial.println(" F");


    // Control logic based on average of in-box sensors
    if (tempAvg < cold) {
      digitalWrite(relayPin, LOW);
      digitalWrite(slowPin, LOW);
      digitalWrite(fastPin, LOW);
      Serial.println("FAN OFF");
    } else if (tempAvg < hot) {
      digitalWrite(relayPin, HIGH);
      digitalWrite(slowPin, HIGH); // 5V
      digitalWrite(fastPin, LOW);
      Serial.println("FAN LOW");
    } else { // tempAvg >= hot
      digitalWrite(relayPin, HIGH);
      digitalWrite(slowPin, LOW);
      digitalWrite(fastPin, HIGH); // 12V
      Serial.println("FAN HIGH");
    }


    // Log runtime, average temp, ambient temp, and inlet pipe temp to SD
    logDataToSD(totalRuntime, tempAvg, (int)temp3, (int)temp4);
  }
}


void logDataToSD(unsigned long runtimeMillis, int tempAvg, int tempAmbient, int tempInlet) {
  float runtimeSeconds = runtimeMillis / 1000.0;


  File dataFile = SD.open("data_log.txt", FILE_WRITE);


  if (dataFile) {
    // CSV: runtime_seconds, avg_inbox_F, ambient_F, inletPipe_F
    dataFile.print(runtimeSeconds);
    dataFile.print(", ");
    dataFile.print(tempAvg);
    dataFile.print(", ");
    dataFile.print(tempAmbient);
    dataFile.print(", ");
    dataFile.println(tempInlet);
    dataFile.close();


    Serial.print("Logged to SD: ");
    Serial.print(runtimeSeconds); Serial.print(" s, ");
    Serial.print(tempAvg);        Serial.print(" F, ");
    Serial.print(tempAmbient);    Serial.print(" F, ");
    Serial.print(tempInlet);      Serial.println(" F");
  } else {
    Serial.println("Error opening data_log.txt");
  }
}



