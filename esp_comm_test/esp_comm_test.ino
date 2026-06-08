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
    Serial.println("Received from Arduino: " + ard_message);
    espSerial.println("hello from esp!");
  }
}
