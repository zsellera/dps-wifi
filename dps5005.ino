#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <cstdlib>
#include <memory>
#include <vector>
#include <algorithm>

#include "dps.hpp"
#include "automation.hpp"
#include "settings.h"

ESP8266WebServer            m_server(80);          // Server on port 80
File                        m_fsUploadFile;        // holds the current upload - quick html page upload
dps_status                  m_dps_info;            // holds the last values
WiFiUDP                     m_ntpUDP;              // UDP packet for NTP client
NTPClient                   m_timeClient(m_ntpUDP, NTP_POOL);
std::vector<String>         m_access_points;       // Saved Wifi APs
std::vector<String>         m_wifi_passwords;      // AP passwords

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
  char buff[256];
  sprintf(buff, status_fmt,
          m_dps_info.uset, m_dps_info.iset, m_dps_info.uout, m_dps_info.iout, 
          m_dps_info.power, m_dps_info.uin, m_dps_info.lock, m_dps_info.protect, 
          m_dps_info.cvcc, m_dps_info.onoff);
  String data(buff);
  m_server.send(200, "application/json", data);
}

void handleVoltage() {
  String value = m_server.arg("v");
  if (value.length() > 0) {
    int ival = atoi(value.c_str());
    if (ival >= MIN_VOLTAGE && ival < MAX_VOLTAGE) {
      dps_set_voltage((uint16_t) ival);
      m_server.send(200, "application/json", {});
      return;
    }
  }
  m_server.send(400, "application/json", "{}");
}

void handleCurrent() {
  String value = m_server.arg("v");
  if (value.length() > 0) {
    int ival = atoi(value.c_str());
    if (ival >= MIN_CURRENT && ival < MAX_CURRENT) {
      dps_set_current((uint16_t) ival);
      m_server.send(200, "application/json", {});
      return;
    }
  }
  m_server.send(400, "application/json", "{}");
}

void handleOnOff() {
  String value = m_server.arg("v");
  bool onoff_state = value == "1";
  dps_set_onoff(onoff_state);
  m_server.send(200, "application/json", {});
}


String getContentType(String filename){
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else return "text/plain";
}

void handleFiles() {
  String path = m_server.uri();
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  if(SPIFFS.exists(path)){
    if (path != "/index.html") {
      m_server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
      m_server.sendHeader("Last-Modified", "Mon, 30 Oct 2017 01:11:56 GMT");
      m_server.sendHeader("Expires", "06 Aug 2026 12:30:45 GMT");
    }
    File file = SPIFFS.open(path, "r");
    size_t sent = m_server.streamFile(file, contentType);
    file.close();
  } else {
    m_server.send(404, "text/html", "not found " + path);
  }
}

void handleDeploy()
{
  HTTPUpload& upload = m_server.upload();
  if(upload.status == UPLOAD_FILE_START){
    m_fsUploadFile = SPIFFS.open("/index.html", "w");
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(m_fsUploadFile)
      m_fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(m_fsUploadFile)
      m_fsUploadFile.close();
  }
}

void loadWifiPasswords()
{
  if (SPIFFS.exists(SPIFFS_WIFI_FILE))
  {
    File wifi_file = SPIFFS.open(SPIFFS_WIFI_FILE, "r");
    while (wifi_file.available())
    {
      String line = wifi_file.readStringUntil('\n');
      int p = line.indexOf(':');
      String ap = line.substring(0,p);
      String password = line.substring(p+1);
      m_access_points.push_back(ap);
      m_wifi_passwords.push_back(password);
    }
    wifi_file.close();
  } else {
    m_access_points.clear();
    m_wifi_passwords.clear();
  }
}

void storeWifiPasswords()
{
  char buff[256];
  File wifi_file = SPIFFS.open(SPIFFS_WIFI_FILE, "w");
  for (int i=0; i<m_access_points.size(); ++i)
  {
    sprintf(buff, "%s:%s", m_access_points[i].c_str(), m_wifi_passwords[i].c_str());
    wifi_file.println(buff);
  }
  wifi_file.close();
}

String findWifiPassword(String ssid)
{
  std::vector<String>::iterator is = std::find(m_access_points.begin(), m_access_points.end(), ssid);
  if (is != m_access_points.end())
  {
    ptrdiff_t pos = is - m_access_points.begin();
    return m_wifi_passwords[pos];
  }
  return "";
}

void connectWifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED_PIN, LOW);
    
    // find a stored wifi:
    int n = WiFi.scanNetworks();
    String ssid, password;
    for (int i=0; i<n; ++i)
    {
      ssid = WiFi.SSID(i);
      password = findWifiPassword(ssid);
      if (password != "")
      {
        break;
      }
    }

    if (password != "")
    {
      // connect to a wifi:
      WiFi.mode(WIFI_STA);
      delay(100);
      WiFi.begin(ssid.c_str(), password.c_str());
      // Wait for connection
      int wait_counter = 0;
      digitalWrite(LED_PIN, HIGH);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        digitalWrite(LED_PIN, ++wait_counter % 2 ? HIGH : LOW);
        Serial.print(".");
      }
      digitalWrite(LED_PIN, HIGH);
    } else {
      // No saved Wifi, connect to an AP
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(WIFI_SOFTAP_SSID, WIFI_SOFTAP_PASS);
      digitalWrite(LED_PIN, HIGH);
    }
    
  }
}

void updateNtpTime() {
  m_timeClient.update();
  time_t _now = m_timeClient.getEpochTime();
  setTime(_now);
  adjustTime(TIMEZONE * 3600);
}

void setup(void){
  Serial.begin(DPS_BAUD);
  pinMode(LED_PIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output

  SPIFFS.begin();

  loadWifiPasswords();
  connectWifi();
  Alarm.timerRepeat(30, connectWifi);

  m_server.on("/status", handleStatus);
  m_server.on("/uset", handleVoltage);
  m_server.on("/iset", handleCurrent);
  m_server.on("/onoff", handleOnOff);
  m_server.on("/deploy", HTTP_POST, [](){ m_server.send(200, "text/plain", ""); }, handleDeploy);
  m_server.onNotFound(handleFiles);
 
  m_server.begin();                  //Start server
  Serial.println("HTTP server started");

  Serial.println("Update time");
  m_timeClient.begin();
  updateNtpTime();
  Alarm.timerRepeat(60, updateNtpTime);
  Serial.print("Current time is: ");
  Serial.println(now());

  if (!MDNS.begin(MDSN_NAME)) {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

  Serial.println("Setup done");
  delay(500);  
  Serial.swap();
  delay(500);
}


void loop(void) {
  m_server.handleClient();          // Handle API requests
  Alarm.delay(0);                 // Run timer hooks
}
