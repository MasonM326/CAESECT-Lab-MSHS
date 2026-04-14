#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include "HardwareSerial.h"

const char* ssid = "ESP32-Network";
const char* password = "abc123";

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
unsigned long timerDelay = 1000; // Update every 2 seconds

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
      Serial.printf("WebSocket client #%u connected\n", client->id());
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

  server.begin();
}

void loop() {
  ws.cleanupClients();

  // Proof of Concept: Generate random data every 2 seconds
  if (millis() - lastTime > timerDelay) {
    if (espSerial.available()){
    String ard_message = espSerial.readStringUntil('\n');
    ard_message.trim();
    Serial.println("Received from Arduino: " + ard_message);
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

    ellapsed_sec = String(ellapsed_sec.toInt() + 1);

    if (ellapsed_sec == "60")
    {
      ellapsed_mins = String(ellapsed_mins.toInt() + 1);
      ellapsed_sec = "0";
    }

    if (ellapsed_mins == "60")
    {
      ellapsed_hrs = String(ellapsed_hrs.toInt() + 1);
      ellapsed_mins = "0";
      ellapsed_sec = "0";

    }

    

    String outgoingJSON = getTempValues();
    Serial.println("Broadcasting: " + outgoingJSON);

    ws.textAll(outgoingJSON);
    lastTime = millis();
  }
}