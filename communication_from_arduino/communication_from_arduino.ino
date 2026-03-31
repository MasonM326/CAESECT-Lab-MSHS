
void setup() {
  Serial.begin(9600);
  delay(2000);
}

void loop() {
    Serial.println("hello from the Arduino!");
    delay(2000);
 if (Serial.available()){
    String received = Serial.readStringUntil('\n');
    received.trim();
    Serial.println("Received from ESP32: " + received);
  }

}
