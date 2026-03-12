#include "HardwareSerial.h"

HardwareSerial espSerial(2);


void setup() {
  Serial.begin(115200);
  espSerial.begin(9600, SERIAL_8N1, 27, 26);
}

void loop() {
  if (espSerial.available()){
    String received = espSerial.readStringUntil('\n');
    received.trim();
    Serial.println("Received from Arduino: " + received);
    espSerial.println("hello from esp");

  }
}
