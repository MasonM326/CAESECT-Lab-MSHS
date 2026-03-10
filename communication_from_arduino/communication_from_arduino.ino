#include <SoftwareSerial.h>

SoftwareSerial arduinoSerial(0, 1);


void setup() {
  Serial.begin(115200);
  arduinoSerial.begin(9600);
  delay(2000);
  arduinoSerial.println("hello from the Arduino!");
}

void loop() {
  if (arduinoSerial.available()){
    String received = arduinoSerial.readStringUntil('\n');
    received.trim();
    Serial.println("Received from ESP32: " + received);
  }

}
