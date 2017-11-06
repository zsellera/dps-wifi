#include "dps.hpp"
#include "modbus.hpp"
#include "settings.h"
#include <stdint.h>

bool dps_read_status(dps_status *dest) {
  if (modbus_read_regs(DPS_REG_VOLTAGE, DPS_REG_ONOFF + 1, dest)) {
    dest->uset = htons(dest->uset);
    dest->iset = htons(dest->iset);
    dest->uout = htons(dest->uout);
    dest->iout = htons(dest->iout);
    dest->power = htons(dest->power);
    dest->uin = htons(dest->uin);
    dest->lock = htons(dest->lock);
    dest->protect = htons(dest->protect);
    dest->cvcc = htons(dest->cvcc);
    dest->onoff = htons(dest->onoff);
    return true;
  } else {
    return false;
  }
}

bool dps_set_voltage(const uint16_t voltage) {
  return modbus_write_reg(DPS_REG_VOLTAGE, voltage);
}

bool dps_set_current(const uint16_t current) {
  return modbus_write_reg(DPS_REG_CURRENT, current);
}

bool dps_set_voltage_current(const uint16_t voltage, const uint16_t current) {
  const uint16_t data[] = {voltage, current};
  return modbus_write_regs(DPS_REG_VOLTAGE, 2, data);
}

bool dps_set_onoff(bool value) {
  modbus_write_reg(DPS_REG_ONOFF, value?1:0);
}

