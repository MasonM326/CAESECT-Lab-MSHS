#include "HardwareSerial.h"

HardwareSerial espSerial(2);


void setup() {
  Serial.begin(115200);
  espSerial.begin(9600, SERIAL_8N1, 27, 26);
}

void loop() {
  if (espSerial.available()){
    String ard_message = espSerial.readStringUntil('\n');
    ard_message.trim();
    Serial.println("Received from Arduino: " + received);
      if (ard_message == "Are you ready for data ESP?"){
        espSerial.println("I am");

        TempValue1 = espSerial.readStringUntil('\n');
        TempValue2 = espSerial.readStringUntil('\n');
        TempValue3 = espSerial.readStringUntil('\n');
        TempValue4 = espSerial.readStringUntil('\n');
        TempValue5 = espSerial.readStringUntil('\n');
        heaterStatus = espSerial.readStringUntil('\n');
        fanStatus = espSerial.readStringUntil('\n');
      }
    

  }
}
