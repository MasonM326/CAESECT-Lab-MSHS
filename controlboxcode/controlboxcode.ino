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
const int thermoCS3 = 9;  // ambient (outside box)
const int thermoCS4 = 8; // NEW: inlet pipe

LiquidCrystal_I2C lcd(0x27, 16, 2);


// Thermocouple instances
MAX6675 thermocouple1(thermoCLK, thermoCS1, thermoDO); // in box #1
MAX6675 thermocouple2(thermoCLK, thermoCS2, thermoDO); // in box #2
MAX6675 thermocouple3(thermoCLK, thermoCS3, thermoDO); // ambient
MAX6675 thermocouple4(thermoCLK, thermoCS4, thermoDO); // inlet pipe (NEW)


int slowPin  = A0;  // low fan output pin was pin 4
int fastPin  = A7;  // high fan output pin was pin 3
int relayPin = 2;  // relay control pin (heater pin)

int tempAvg = 0;

int cold = 75;  // Cold threshold (°F)
int hot  = 90;  // Hot threshold (°F)

bool low_fan = false;
bool high_fan = false;
bool heater = false;
bool cold_threshold = false;
bool hot_threshold = false;

const int chipSelect = 10; // CS pin for SD card module


unsigned long previousMillis = 0;
const unsigned long interval = 5000; // 5 seconds
unsigned long totalRuntime = 0;

struct SensorData {
  float temp1;
  float temp2;
  float temp3;
  float temp4;
  int tempAvg;
  bool low_fan;
  bool heater;
};

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

SensorData get_all_data(bool current_fan, bool current_heater) {
  SensorData data;
  data.temp1 = thermocouple1.readFahrenheit();
  data.temp2 = thermocouple2.readFahrenheit();
  data.temp3 = thermocouple3.readFahrenheit();
  data.temp4 = thermocouple4.readFahrenheit();
  data.tempAvg = (int)((data.temp1 + data.temp2) / 2.0);
  data.low_fan = current_fan;
  data.heater = current_heater;
  return data;
}

void sending_data(SensorData data) {
  if (Serial.available()) {
    String esp_message = Serial.readStringUntil('\n');
    esp_message.trim();
    
    if (esp_message == "I am") {
      Serial.println(data.temp1);
      Serial.println(data.temp2);
      Serial.println(data.temp3);
      Serial.println(data.temp4);
      Serial.println(data.tempAvg);
      Serial.println(data.low_fan);
      Serial.println(data.heater);
    }
  }
}

int temperature_readings(){

        double temp1 = thermocouple1.readFahrenheit();
        double temp2 = thermocouple2.readFahrenheit();
        double temp3 = thermocouple3.readFahrenheit(); // ambient
        double temp4 = thermocouple4.readFahrenheit(); // inlet pipe (NEW)


      // Validate readings
      if (isnan(temp1) || isnan(temp2) || isnan(temp3) || isnan(temp4)) {
        Serial.println("Error reading one or more thermocouples!");
        lcd.setCursor(0, 0);
        lcd.print("Error 1");
        //return; //may need to comment this out
      }

      int tempAvg = (int)((temp1 + temp2) / 2.0);

      // Serial output (you’ll hook up your new TFT later)
      Serial.print("Temp1 (in box #1): ");/*19*/ Serial.print(temp1); Serial.println(" F");
      Serial.print("Temp2 (in box #2): "); Serial.print(temp2); Serial.println(" F");
      Serial.print("Ambient (TC3):     "); Serial.print(temp3); Serial.println(" F");
      Serial.print("Inlet Pipe (TC4):  "); Serial.print(temp4); Serial.println(" F");
      Serial.print("Average (1&2):     "); Serial.print(tempAvg); Serial.println(" F");


      String LCDOutputLine1 = "AVG: " + String(tempAvg) + " AMB: " + String((int)temp3);
      String LCDOutputLine2 = "IN: " + String((int)temp4);



      lcd.setCursor(0, 0);
      lcd.print(LCDOutputLine1);
      lcd.setCursor(0, 1);
      lcd.print(LCDOutputLine2);

      return tempAvg;
}



void setup() {
  Serial.begin(9600);
  Serial.println("MAX6675 controller start");

  lcd.init();
  lcd.backlight();

  pinMode(slowPin, OUTPUT);
  digitalWrite(slowPin, LOW);
  pinMode(fastPin, OUTPUT); // fan starts from a digital low on slowpin, digital high on fastpin, and digital high on relay pin
  digitalWrite(fastPin, LOW); // low to high transition on slow pin turns the fan off
  pinMode(relayPin, OUTPUT);
  
  digitalWrite(slowPin, HIGH);
  //digitalWrite(fastPin, HIGH);
  digitalWrite(relayPin, HIGH);
  Serial.println("Ran Heater"); // Testing the heater
  delay(5000);
  
  //digitalWrite(relayPin, LOW);
  //digitalWrite(slowPin, HIGH);
  //Serial.println("Ran low fan");
  //delay(5000);

  digitalWrite(slowPin, LOW);
  //digitalWrite(fastPin, LOW);
  digitalWrite(relayPin, LOW);
  Serial.println("Ran high fan");
  delay(5000);
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
    
    tempAvg = temperature_readings();
    sending_data(get_all_data(low_fan, heater));
    delay(2000);
    // Control logic based on average of in-box sensors
    if (tempAvg < cold) {
     heater = true; // Heater is turned on
      if (low_fan = true)
        {
          low_fan = false;
          digitalWrite(slowPin, HIGH);
          digitalWrite(relayPin, HIGH);
          Serial.println("Low Fan is turned off and Heater is on");
          temperature_readings();
          sending_data(get_all_data(low_fan, heater));
        }
      //else if (high_fan = true)
      //{
      //    high_fan = false;
      //    digitalWrite(fastPin, LOW);
      //    digitalWrite(relayPin, HIGH);
      //    Serial.println("High fan is turned off and Heater is on");
     // }
      else
      {
        digitalWrite(relayPin, HIGH);
        Serial.println("Heater is on");
        temperature_readings();
        sending_data(get_all_data(low_fan, heater));
      }

      //delay(300000); // run for 5 minutes to make sure it goes over threshold
      
      while (tempAvg <= 60){ // if still not greatly above threshold, we continue until it is. Think this need to be in both if statements?
        tempAvg = temperature_readings();
        sending_data(get_all_data(low_fan, heater));
        delay(2000);
      }
     
    } else if (tempAvg < hot && tempAvg > cold) {
        low_fan = true;
        //if (high_fan = true)
       // {
       //   high_fan = false;
       //   digitalWrite(fastPin, LOW);
       //   digitalWrite(slowPin, HIGH);
      //    Serial.println("High Fan is turned off and Low Fan is on");
      //  }
        if (heater = true)
        {
          heater = false;
          digitalWrite(relayPin, LOW);
          digitalWrite(slowPin, LOW);
          Serial.println("Heater is turned off and Low Fan is on");
          temperature_readings();
          sending_data(get_all_data(low_fan, heater));
        }
        else
        {
          digitalWrite(slowPin, LOW);
          Serial.println("Low fan is on");
          temperature_readings();
          sending_data(get_all_data(low_fan, heater));
        }
    }
    else{
      digitalWrite(slowPin, LOW);
      digitalWrite(relayPin, LOW);
      temperature_readings();
      sending_data(get_all_data(low_fan, heater));
    }
      
   // } else { // tempAvg >= hot
   //     high_fan = true;
   //     if (low_fan = true)
   //     {
   //       low_fan = false;
   //       digitalWrite(slowPin, LOW);
   //       digitalWrite(fastPin, HIGH);
   //       Serial.println("Low Fan is turned off and High Fan is on");
   //     }
   //     else if (heater = true)
   //     {
   //       heater = false;
   //       digitalWrite(relayPin, LOW);
   //       digitalWrite(fastPin, HIGH);
   //       Serial.println("Heater is turned off and High Fan is on");          
   //     }
   //     else
   //     {
    //      digitalWrite(fastPin, HIGH);
     //     Serial.println("High Fan is on");
      //  }
      //delay(300000); // run for 5 minutes to make sure it is under threshold

     // while (tempAvg >= 200){ // if still not greatly below threshold, we continue until it is
      //  tempAvg = temperature_readings();
       // delay(2000);
     // }
      
    }

    

    // Log runtime, average temp, ambient temp, and inlet pipe temp to SD
    //logDataToSD(totalRuntime, tempAvg, (int)temp3, (int)temp4);
  }
//}
