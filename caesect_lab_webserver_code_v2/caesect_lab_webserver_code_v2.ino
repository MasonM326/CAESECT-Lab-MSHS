#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include "HardwareSerial.h"

const char* ssid = "ESP32-Network";
const char* password = "abc123";
bool did_start = false;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

HardwareSerial espSerial(2);

// Simulation Variables
String TempValue1 = "0", TempValue2 = "0", TempValue3 = "0", TempValue4 = "0", TempValue5 = "0";
String heaterStatus = "OFF", fanStatus = "OFF", lowfanStatus = "NOT RUNNING", highfanStatus = "NOT RUNNING";
String ellapsed_hrs = "0";
String ellapsed_mins = "0";
String ellapsed_sec = "0";

// Timer variables for the PoC
unsigned long lastTime = 0;
unsigned long timerDelay = 1000; // Update every second

JSONVar TempValues;

String getTempValues(){
  JSONVar TempValues;

  TempValues["tempValue1"] = TempValue1;
  TempValues["tempValue2"] = TempValue2;
  TempValues["tempValue3"] = TempValue3;
  TempValues["tempValue4"] = TempValue4;
  TempValues["tempValue5"] = TempValue5;
  TempValues["heater"] = heaterStatus;
  TempValues["fan"] = fanStatus;
  TempValues["low_speed"] = lowfanStatus;
  TempValues["high_speed"] = highfanStatus;
  TempValues["hrs"] = ellapsed_hrs;
  TempValues["mins"] = ellapsed_mins;
  TempValues["secs"] = ellapsed_sec;
  return JSON.stringify(TempValues);
}

void notifyClients() {
  ws.textAll(getTempValues());
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "getValues") == 0) {
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      //Serial.printf("WebSocket client #%u connected\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    default:
      break;
  }
}

void setup() {
  delay(10000); // Ten seconds of delay to start the same time as Arduino
  Serial.begin(115200);
  espSerial.begin(9600, SERIAL_8N1, 27, 26);

  // Initialize File System [cite: 6]
  if(!LittleFS.begin()) { Serial.println("LittleFS Error"); return; }
  
  // Initialize WiFi as Access Point [cite: 8]
  WiFi.softAP(ssid, password);
  
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.serveStatic("/", LittleFS, "/");
  //Serial.println("b");

  server.begin();

}

void loop() {
  
  ws.cleanupClients(); // think i can add a mills timer here to make code more clean
  /*
   if (espSerial.available()){ // we are having the data be read in order with some delay so it takes in total one second
        did_start = true;
        TempValue1 = espSerial.readStringUntil('\n');
        Serial.println("1");
        delay(111);
        if (espSerial.available()){
          TempValue2 = espSerial.readStringUntil('\n');
          Serial.println("2");
          delay(111);
          if (espSerial.available()){
            TempValue3 = espSerial.readStringUntil('\n');
            Serial.println("3");
            delay(111);
            if (espSerial.available()){
              TempValue4 = espSerial.readStringUntil('\n');
              Serial.println("4");
              delay(111);
              if (espSerial.available()){
                TempValue5 = espSerial.readStringUntil('\n');
                Serial.println("5");
                delay(111);
                if (espSerial.available()){
                  fanStatus = espSerial.readStringUntil('\n');
                  if (fanStatus == "1"){
                    fanStatus = "ON";
                  }
                  else{
                    fanStatus = "OFF";
                  }
                  Serial.println("6");
                  delay(111);

                  if (espSerial.available()){
                    lowfanStatus = espSerial.readStringUntil('\n');
                    if (lowfanStatus == "1"){
                    lowfanStatus = "RUNNING";
                  }
                    else{
                    lowfanStatus = "NOT RUNNING";
                  }
                    Serial.println("7");
                    delay(111);

                  if (espSerial.available()){
                    highfanStatus = espSerial.readStringUntil('\n');
                    if (highfanStatus == "1"){
                    highfanStatus = "RUNNING";
                  }
                    else{
                    highfanStatus = "NOT RUNNING";
                  }
                    Serial.println("8");
                    delay(111);

                    if (espSerial.available()){
                    heaterStatus = espSerial.readStringUntil('\n');
                    if (heaterStatus == "1"){
                    heaterStatus = "ON";
                  }
                    else{
                    heaterStatus = "OFF";
                  }
                    Serial.println("9");
                    delay(111);
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
        }
   }

*/
/**/
if (espSerial.available()){
    did_start = true;
    
    // Read 1st temp value and tell Arduino to send the next
    TempValue1 = espSerial.readStringUntil('\n'); TempValue1.trim();
    Serial.println("1");
    
    // Wait for 2nd line to arrive in the buffer
    while(!espSerial.available());
    TempValue2 = espSerial.readStringUntil('\n'); TempValue2.trim();
    Serial.println("2");
    
    // Wait for 3rd line
    while(!espSerial.available());
    TempValue3 = espSerial.readStringUntil('\n'); TempValue3.trim();
    Serial.println("3");
    
    // Wait for 4th line
    while(!espSerial.available());
    TempValue4 = espSerial.readStringUntil('\n'); TempValue4.trim();
    Serial.println("4");
    
    // Wait for 5th line (this is avg temp)
    while(!espSerial.available());
    TempValue5 = espSerial.readStringUntil('\n'); TempValue5.trim();
    Serial.println("5");
    
    //****************************
    // FAN STATUS
    //Wait until arduino transmits data from above
    while(!espSerial.available()); //rather than being timedependent, its event driven
    //reads incoming line (physically be 1 or 0, removing hidden spaces also)
    String rawFan = espSerial.readStringUntil('\n'); rawFan.trim();
    //checks if rawFan is 1: if yes, fanStatus is ON; if no, fanStatus is OFF
    fanStatus = (rawFan == "1") ? "ON" : "OFF";
    Serial.println("6");
    
    while(!espSerial.available()); 
    String rawlowFan = espSerial.readStringUntil('\n'); rawFan.trim();
    lowfanStatus = (rawlowFan == "1") ? "RUNNING" : "NOT RUNNING";
    Serial.println("7");

    while(!espSerial.available()); 
    String rawhighFan = espSerial.readStringUntil('\n'); rawFan.trim();
    highfanStatus = (rawhighFan == "1") ? "RUNNING" : "NOT RUNNING";
    Serial.println("8");

    // HEATER STATUS
    //Waits until data transmitted again
    while(!espSerial.available());
    //reads incoming line (physically will be 1 or 0, removes hidden spaces)
    String rawHeater = espSerial.readStringUntil('\n'); rawHeater.trim();
    //checks if rawHeater is 1: if yes, heaterStatus is ON; if no, heaterStatus is OFF
    heaterStatus = (rawHeater == "1") ? "ON" : "OFF";
    Serial.println("9");
  
  }
/*
    if (did_start == true){
    
    ellapsed_sec = String(ellapsed_sec.toInt() + 1); // adding one to the time

    if (ellapsed_sec == "60")
    {
      ellapsed_mins = String(ellapsed_mins.toInt() + 1); // adding a minute
      ellapsed_sec = "0";
    }

    if (ellapsed_mins == "60")
    {
      ellapsed_hrs = String(ellapsed_hrs.toInt() + 1); // adding a second
      ellapsed_mins = "0";
      ellapsed_sec = "0";

    }
    } 
*/
if (did_start) {
  static unsigned long lastClockTick = 0;
  if (millis() - lastClockTick >= 1000) {
    lastClockTick = millis();
    int s = ellapsed_sec.toInt() + 1;
    int m = ellapsed_mins.toInt();
    int h = ellapsed_hrs.toInt();

  //same as before just condensed a bit
  if (s >= 60) { 
    s = 0; // resets clock back to 0 if over 60 sec 
    m++; //increases min by 1
  }
  if (m >= 60) { 
    m = 0; //resets clock to 0 if over 60 min
    h++; //increases hour by 1
  }

  //convert to strings again
    ellapsed_sec = String(s);
    ellapsed_mins = String(m);
    ellapsed_hrs = String(h);

    
}

    //String outgoingJSON = getTempValues();
    //Serial.println("Broadcasting: " + outgoingJSON);

    ws.textAll(getTempValues());
    //lastTime = millis();
  }
  
}