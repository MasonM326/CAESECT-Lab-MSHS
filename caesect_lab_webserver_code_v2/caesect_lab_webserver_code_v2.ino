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
String heaterStatus = "OFF", fanStatus = "OFF";
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
  TempValues["low_fan"] = fanStatus;
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
  Serial.println("a");

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
  ws.cleanupClients(); 
    if (espSerial.available()){ // we are having the data be read in order with some delay so it takes in total one second
        did_start = true;
        TempValue1 = espSerial.readStringUntil('\n');
        Serial.println("1");
        delay(143);
        if (espSerial.available()){
          TempValue2 = espSerial.readStringUntil('\n');
          Serial.println("2");
          delay(143);
          if (espSerial.available()){
            TempValue3 = espSerial.readStringUntil('\n');
            Serial.println("3");
            delay(143);
            if (espSerial.available()){
              TempValue4 = espSerial.readStringUntil('\n');
              Serial.println("4");
              delay(143);
              if (espSerial.available()){
                TempValue5 = espSerial.readStringUntil('\n');
                Serial.println("5");
                delay(143);
                if (espSerial.available()){
                  fanStatus = espSerial.readStringUntil('\n');
                  if (fanStatus = "1"){
                    fanStatus = "ON";
                  }
                  else{
                    fanStatus = "OFF";
                  }
                  Serial.println("6");
                  delay(143);
                  if (espSerial.available()){
                    heaterStatus = espSerial.readStringUntil('\n');
                    if (heaterStatus = "1"){
                    heaterStatus = "ON";
                  }
                    else{
                    heaterStatus = "OFF";
                  }
                    Serial.println("7");
                    delay(143);
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }

    if (did_start = true){
    
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
    

    String outgoingJSON = getTempValues();
    //Serial.println("Broadcasting: " + outgoingJSON);

    ws.textAll(outgoingJSON);
    //lastTime = millis();
  }
//}