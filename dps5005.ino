#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Ticker.h>
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
enum {AP, CLIENT}           m_mode = CLIENT;       // current mode
Ticker                      m_tmr_dps;
boolean                     m_try_update_dps = true;
Ticker                      m_tmr_wifi;
boolean                     m_try_connect_wifi = true;
Ticker                      m_tmr_ntp;
boolean                     m_try_update_ntp = true;
Ticker                      m_tmr_led;

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
  "\"onoff\":%d,"
  "\"time\":%d}";


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
      password.trim();
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

void handleStatus() {
  char buff[256];
  sprintf(buff, status_fmt,
          m_dps_info.uset, m_dps_info.iset, m_dps_info.uout, m_dps_info.iout, 
          m_dps_info.power, m_dps_info.uin, m_dps_info.lock, m_dps_info.protect, 
          m_dps_info.cvcc, m_dps_info.onoff, now() - TIMEZONE * 3600);
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
  m_server.send(200, "application/json", "{}");
}

void handleWiFiStations()
{
  String json = "{";
  for (int i=0; i<m_access_points.size(); ++i)
  {
    if (i > 0)
    {
      json += ",";
    }
    json += "\"" + m_access_points[i] + "\":\"" + m_wifi_passwords[i] + "\"";
  }
  json += "}";
  m_server.send(200, "application/json", json);
}

void handleWiFiStationAdd()
{
  String ssid = m_server.arg("ssid");
  String pass = m_server.arg("pass");
  if (ssid.length() > 0 && pass.length() > 0) {
    for (int i=0; i<m_access_points.size(); ++i)
    {
      if (m_access_points[i] == ssid)
      {
        m_wifi_passwords[i] = pass;
        storeWifiPasswords();
        handleWiFiStations();
        return;
      }
    }
    m_access_points.push_back(ssid);
    m_wifi_passwords.push_back(pass);
    storeWifiPasswords();
    handleWiFiStations();
    return;
  }
  m_server.send(400, "application/json", "{}");
}

void handleWiFiStationClear()
{
  m_access_points.clear();
  m_wifi_passwords.clear();
  storeWifiPasswords();
  handleWiFiStations();
  return;
}

String getContentType(String filename)
{
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else return "text/plain";
}

void handleFiles()
{
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

String makeAutomationJson(automation_cronentry_t &entry)
{
  char buff[256];
  const char* fmt = "{\"min\": %d, \"hour\": %d, \"day\": %d, \"month\": %d, \"dow\": %d, \"type\": %s, ";
  const char* type = entry.step_type == AUTOMATION_SET_ONOFF ? "onoff" : "limits";
  sprintf(buff, fmt, entry.crondef.min, entry.crondef.hour, entry.crondef.day, entry.crondef.month, entry.crondef.dow, type); 
  String json(buff);
  
  if (entry.step_type == AUTOMATION_SET_ONOFF)
  {
    bool state;
    automation_parse_onoff(&entry.context, state);
    sprintf(buff, "\"state\": %d}", state);
    json += String(buff);
  }

  if (entry.step_type == AUTOMATION_SET_LIMITS)
  {
    uint16_t voltage, current;
    automation_parse_limits(&entry.context, voltage, current);
    sprintf(buff, "\"u\": %d, \"i\": %d}", voltage, current);
    json += String(buff);
  }
  
  return json;
}

void handleAutomation()
{
  automation_t entries = automation_entries();
  String json = "[";
  for (int i=0; i<entries.size(); ++i) {
    if (i != 0)
    {
      json += ",";
    }
    json += makeAutomationJson(entries[i]);
  }
  json += "]";
  m_server.send(200, "application/json", json);
}

void handleAutomationAdd()
{
  int min = atoi(m_server.arg("min").c_str());
  int hour = atoi(m_server.arg("hour").c_str());
  int day = atoi(m_server.arg("day").c_str());
  int month = atoi(m_server.arg("month").c_str());
  int dow = atoi(m_server.arg("dow").c_str());
  String type = m_server.arg("type");

  automation_crondef_t cd = {
    .min = min >= -1 && min <= 59 ? min : -1,
    .hour = hour >= -1 && hour <= 23 ? hour : -1,
    .day = day >= -1 && day <= 30 ? day : -1,
    .month = month >= -1 && month <= 11 ? month : -1,
    .dow = dow >= -1 && dow <= 6 ? dow : -1,
  };

  if (type == "onoff")
  {
    bool state = m_server.arg("state") == "1";
    automation_cronentry_t e = automation_entry_onoff(cd, state);
    automation_crontab_add(e);
  }

  if (type == "limits")
  {
    uint16_t i = (uint16_t) atoi(m_server.arg("i").c_str());
    uint16_t u = (uint16_t) atoi(m_server.arg("u").c_str());
    automation_cronentry_t e = automation_entry_limits(cd, u, i);
    automation_crontab_add(e);
  }

  File automation_file = SPIFFS.open(SPIFFS_AUTOMATION_FILE, "w");
  automation_store(automation_file);
  automation_file.close();

  handleAutomation();
}

void handleAutomationRemove()
{
  int n = atoi(m_server.arg("n").c_str());
  automation_t entries = automation_entries();
  
  if (n < entries.size() && n >=0)
  {
    automation_crontab_remove(n);
    File automation_file = SPIFFS.open(SPIFFS_AUTOMATION_FILE, "w");
    automation_store(automation_file);
    automation_file.close();
  }

  handleAutomation();
}

void handleAutomationState()
{
  String new_state = m_server.arg("state");
  if (new_state != "")
  {
    automation_set_enabled(new_state == "1");
  }
  
  char buff[64];
  bool current_state = automation_get_enabled();
  sprintf(buff, "{\"enabled\": %d}", current_state);
  m_server.send(200, "application/json", String(buff));
}

void handleStatusLED()
{
  static uint8_t call_counter = 0;
  ++call_counter;
  if (m_mode == CLIENT)
  {
    if (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_PIN, call_counter % 10 == 0);
    } else {
      digitalWrite(LED_PIN, call_counter % 10 != 0);
    }
    return;
  }
  if (m_mode == AP)
  {
    if (WiFi.softAPgetStationNum() > 0)
    {
      digitalWrite(LED_PIN, call_counter % 2 != 0 || call_counter % 8 > 4);
    } else {
      digitalWrite(LED_PIN, call_counter % 2 != 0);
    }
    return;
  }
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
      delay(500);
      WiFi.begin(ssid.c_str(), password.c_str());
      // Wait for connection
      int wait_counter = 0;
      while (WiFi.status() != WL_CONNECTED) {
        if (wait_counter++ > 20)
        {
          break;  
        }
        delay(500);
      }
      m_mode = CLIENT;

    } else {
      // No saved Wifi, connect to an AP
      if (m_mode == CLIENT)
      {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
        boolean result = WiFi.softAP(WIFI_SOFTAP_SSID, WIFI_SOFTAP_PASS);
        if (result)
        {
          m_mode = AP;
        }
      }
    }
    
    MDNS.update();
  }
}

void handleConnectWifi()
{
  m_try_connect_wifi = true;
}

void updateNtpTime() {
  if (m_mode == CLIENT) {
    m_timeClient.update();
    time_t _now = m_timeClient.getEpochTime();
    setTime(_now);
    adjustTime(TIMEZONE * 3600);
  }
}

void handleUpdateNtp()
{
  m_try_update_ntp = true;
}


void handleDPSUpdate()
{
  m_try_update_dps = true;
}

void setup(void){
  Serial.begin(DPS_BAUD);

  Serial.println("Initialize status LED");
  pinMode(LED_PIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  m_tmr_led.attach_ms(100, handleStatusLED);

  Serial.println("Initialize SPIFFS");
  SPIFFS.begin();

  Serial.println("Initialize WiFi");
  loadWifiPasswords();
  m_tmr_wifi.attach(30.0f, handleConnectWifi);

  Serial.println("Initialize Automation");
  automation_set_enabled(false);
  
  if (SPIFFS.exists(SPIFFS_AUTOMATION_FILE))
  {
    File automation_file = SPIFFS.open(SPIFFS_AUTOMATION_FILE, "r");
    automation_load(automation_file);
    automation_file.close();
  }

  Serial.println("Initialize HTTP handler");
  m_server.on("/status", handleStatus);
  m_server.on("/uset", handleVoltage);
  m_server.on("/iset", handleCurrent);
  m_server.on("/onoff", handleOnOff);
  m_server.on("/wifi", handleWiFiStations);
  m_server.on("/wifi/add", handleWiFiStationAdd);
  m_server.on("/wifi/clear", handleWiFiStationClear);
  m_server.on("/automation", handleAutomation);
  m_server.on("/automation/add", handleAutomationAdd);
  m_server.on("/automation/remove", handleAutomationRemove);
  m_server.on("/automation/state", handleAutomationState);
  m_server.on("/deploy", HTTP_POST, [](){ m_server.send(200, "text/plain", ""); }, handleDeploy);
  m_server.onNotFound(handleFiles);
  m_server.begin();                  //Start server


  Serial.println("Initialize Time");
  m_timeClient.begin();
  m_tmr_ntp.attach(60.0f, handleUpdateNtp);
  
  Serial.println("Initialize Service Discovery");
  if (!MDNS.begin(MDSN_NAME)) {
    Serial.println("Error setting up MDNS responder!");
  }
  MDNS.addService("http", "tcp", 80);

  Serial.println("Setup DPS");
  m_tmr_dps.attach(1.0f, handleDPSUpdate);

  Serial.println("Setup done");

  delay(500);  
  Serial.swap();
  delay(500);
}


void loop(void) {
  m_server.handleClient();          // Handle API requests
  automation_run();                 // Run from schedule
  
  if (m_try_connect_wifi)
  {
    connectWifi();
    m_try_connect_wifi = false;
  }
  
  if (m_try_update_ntp)
  {
    updateNtpTime();
    m_try_update_ntp = false;
  }

  if (m_try_update_dps)
  {
    dps_read_status(&m_dps_info);
    m_try_update_dps = false;
  }

}
