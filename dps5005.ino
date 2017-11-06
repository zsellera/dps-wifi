#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <cstdlib>

#include "dps.hpp"
#include "settings.h"

//SSID and Password of your WiFi router
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

ESP8266WebServer server(80); //Server on port 80
File fsUploadFile; //holds the current upload

#define LED_PIN 2

const char* status_fmt =
  "{\"uset\":%d,"
  "\"iset\":%d,"
  "\"uout\":%d,"
  "\"iout\":%d,"
  "\"power\":%d,"
  "\"uin\":%d,"
  "\"lock\":%d,"
  "\"protect\":%d,"
  "\"cvcc\":%d,"
  "\"onoff\":%d}";


void handleStatus() {
  digitalWrite(LED_PIN, LOW);
  char buff[256];
  dps_status dps;
  if (dps_read_status(&dps)) {
    sprintf(buff, status_fmt,
            dps.uset, dps.iset, dps.uout, dps.iout, 
            dps.power, dps.uin, dps.lock, dps.protect, 
            dps.cvcc, dps.onoff);
    String data(buff);
    server.send(200, "application/json", data);
  } else {
    server.send(500, "application/json", "{}");
  }
}

void handleVoltage() {
  digitalWrite(LED_PIN, LOW);
  String value = server.arg("v");
  if (value.length() > 0) {
    int ival = atoi(value.c_str());
    if (ival >= MIN_VOLTAGE && ival < MAX_VOLTAGE) {
      dps_set_voltage((uint16_t) ival);
      server.send(200, "application/json", {});
      return;
    }
  }
  server.send(400, "application/json", "{}");
}

void handleCurrent() {
  digitalWrite(LED_PIN, LOW);
  String value = server.arg("v");
  if (value.length() > 0) {
    int ival = atoi(value.c_str());
    if (ival >= MIN_CURRENT && ival < MAX_CURRENT) {
      dps_set_current((uint16_t) ival);
      server.send(200, "application/json", {});
      return;
    }
  }
  server.send(400, "application/json", "{}");
}

void handleOnOff() {
  digitalWrite(LED_PIN, LOW);
  String value = server.arg("v");
  bool onoff_state = value == "1";
  dps_set_onoff(onoff_state);
  server.send(200, "application/json", {});
}


String getContentType(String filename){
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else return "text/plain";
}

void handleFiles() {
  digitalWrite(LED_PIN, LOW);
  String path = server.uri();
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  if(SPIFFS.exists(path)){
    if (path != "/index.html") {
      server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
      server.sendHeader("Last-Modified", "Mon, 30 Oct 2017 01:11:56 GMT");
      server.sendHeader("Expires", "06 Aug 2026 12:30:45 GMT");
    }
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/html", "not found " + path);
  }
}

void handleDeploy() {
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    fsUploadFile = SPIFFS.open("/index.html", "w");
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
  }
}

void setup(void){
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output

  SPIFFS.begin();
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("Connecting to wifi...");
 
  // Wait for connection
  int wait_counter = 0;
  digitalWrite(LED_PIN, HIGH);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_PIN, ++wait_counter % 2 ? HIGH : LOW);
    Serial.print(".");
  }
  digitalWrite(LED_PIN, LOW);
 
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

  server.on("/status", handleStatus);
  server.on("/uset", handleVoltage);
  server.on("/iset", handleCurrent);
  server.on("/onoff", handleOnOff);
  server.on("/deploy", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleDeploy);
  server.onNotFound(handleFiles);
 
  server.begin();                  //Start server
  Serial.println("HTTP server started");

  if (!MDNS.begin(MDSN_NAME)) {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  
  delay(500);  
  Serial.swap();
  delay(500);
}


void loop(void) {
  server.handleClient();          //Handle client requests
  digitalWrite(LED_PIN, HIGH);
}
