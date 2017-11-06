#ifndef __MODBUS__
#define __MODBUS__

#include <stdint.h>

#define SLAVE_UNIT_ID 1
#define MODBUS_TIMEOUT_MS 1000

#define READ_HOLDING_REG 0x03
#define WRITE_SINGLE_REG 0x06
#define WRITE_MULTIPLE_REGS 0x10

#define htons(x) ( ((x)<< 8 & 0xFF00) | ((x)>> 8 & 0x00FF) )

uint16_t modbus_crc16(uint8_t *buf, int len);
bool modbus_read_regs(uint16_t reg_addr, uint8_t n, void *dest);
bool modbus_write_reg(uint16_t reg_addr, const uint16_t value);
bool modbus_write_regs(uint16_t start_reg_addr, uint8_t n, const uint16_t values[]);

#endif
