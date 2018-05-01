#ifndef __SETTINGS__
#define __SETTINGS__

#define DPS_BAUD 19200

#define DPS_REG_VOLTAGE 0x0000
#define DPS_REG_CURRENT 0x0001
#define DPS_REG_ONOFF   0x0009

#define MIN_VOLTAGE 0
#define MAX_VOLTAGE 5000
#define MIN_CURRENT 0
#define MAX_CURRENT 4999

#define MDSN_NAME "dps"
#define LED_PIN D0

#define SPIFFS_WIFI_FILE "/wifi.txt"
#define NTP_POOL "hu.pool.ntp.org"
#define TIMEZONE 1

#define WIFI_SOFTAP_SSID "dps-ap"
#define WIFI_SOFTAP_PASS WIFI_SOFTAP_SSID

#endif
