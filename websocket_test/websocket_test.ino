#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>

const char* ssid = "ESP32-Network";
const char* password = "abc123";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Simulation Variables
String TempValue1 = "0", TempValue2 = "0", TempValue3 = "0", TempValue4 = "0", TempValue5 = "0";
String heaterStatus = "OFF", fanStatus = "OFF";

// Timer variables for the PoC
unsigned long lastTime = 0;
unsigned long timerDelay = 2000; // Update every 2 seconds

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
    // Random float between 65.0 and 85.0
    TempValue1 = String(random(650, 850) / 10.0);
    TempValue2 = String(random(650, 850) / 10.0);
    TempValue3 = String(random(650, 850) / 10.0);
    TempValue4 = String(random(700, 750) / 10.0); // Ambient is steadier
    TempValue5 = String(random(600, 900) / 10.0);
    
    // Randomize status strings
    heaterStatus = (random(0, 2) == 1) ? "ON" : "OFF";
    fanStatus = (random(0, 2) == 1) ? "RUNNING" : "STOPPED";

    String outgoingJSON = getTempValues();
    Serial.println("Broadcasting: " + outgoingJSON);

    ws.textAll(outgoingJSON);
    lastTime = millis();
  }
}