
#ifndef __DPS__
#define __DPS__

#include <stdint.h>

#define MIN_VOLTAGE 0
#define MAX_VOLTAGE 5000
#define MIN_CURRENT 0
#define MAX_CURRENT 4999

#define htons(x) ( ((x)<< 8 & 0xFF00) | ((x)>> 8 & 0x00FF) )

struct dps_status {
  uint16_t uset;
  uint16_t iset;
  uint16_t uout;
  uint16_t iout;
  uint16_t power;
  uint16_t uin;
  uint16_t lock;
  uint16_t protect;
  uint16_t cvcc;
  uint16_t onoff;
};

bool dps_read_status(dps_status *dest);
bool dps_set_voltage(const uint16_t voltage);
bool dps_set_current(const uint16_t current);
bool dps_set_voltage_current(const uint16_t voltage, const uint16_t current);
bool dps_set_onoff(bool value);

#endif
