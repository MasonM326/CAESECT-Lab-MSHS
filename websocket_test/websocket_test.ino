#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>

// Replace with your network credentials
const char* ssid = "ESP32-Network";
const char* password = "abc123";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");

unsigned long lastTime = 0;
unsigned long timerDelay = 2000;

String message = "";
String TempValue1 = "0";
String TempValue2 = "0";
String TempValue3 = "0";
String TempValue4 = "0";
String TempValue5 = "0";
String heaterStatus = "OFF";
String fanStatus = "OFF";


const int resolution = 8;

//Json Variable to hold temp values
JSONVar TempValues;

//Get Slider Values
String getTempValues(){
  TempValues["tempValue1"] = String(TempValue1);
  TempValues["tempValue2"] = String(TempValue2);
  TempValues["tempValue3"] = String(TempValue3);
  TempValues["tempValue4"] = String(TempValue4);
  TempValues["tempValue5"] = String(TempValue5);
  TempValues["heater"] = heaterStatus;
  TempValues["low_fan"] = fanStatus;
  String jsonString = JSON.stringify(TempValues);
  return jsonString;
}

// Initialize LittleFS
void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  else{
   Serial.println("LittleFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.softAP(ssid,password);
  Serial.print("Connecting to WiFi ..");
  Serial.println("");
  Serial.println("IP address: "); // 192.168.4.1
  Serial.println(WiFi.softAPIP());
}

void notifyClients(String TempValues) {
  ws.textAll(TempValues);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
    if (message.indexOf("1s") >= 0) {
      TempValue1 = message.substring(2);
      Serial.print(getTempValues());
      notifyClients(getTempValues());
    }
    if (message.indexOf("2s") >= 0) {
      TempValue2 = message.substring(2);
      Serial.print(getTempValues());
      notifyClients(getTempValues());
    }    
    if (message.indexOf("3s") >= 0) {
      TempValue3 = message.substring(2);
      Serial.print(getTempValues());
      notifyClients(getTempValues());
    }
    if (message.indexOf("4s") >= 0) {  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
      TempValue4 = message.substring(2);
      Serial.print(getTempValues());
      notifyClients(getTempValues());
    }
    if (message.indexOf("5s") >= 0) {
      TempValue5 = message.substring(2);
      Serial.print(getTempValues());
      notifyClients(getTempValues());
    }
    if (strcmp((char*)data, "getValues") == 0) {
      notifyClients(getTempValues());
    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  initFS();
  initWiFi();

  initWebSocket();
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", LittleFS, "/");

  // Start server
  server.begin();
}

void loop() {
  ws.cleanupClients();

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

    notifyClients(getTempValues());
    lastTime = millis();
}

}