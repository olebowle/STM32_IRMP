#ifndef __HANDLER_H
#define __HANDLER_H

#include "usb_hid.h"

/* first 3 bytes: STAT_CMD ACC_GET CMD_CAPS not useable to transmit information */
#define BYTES_PER_QUERY	(HID_IN_BUFFER_SIZE - 3)

enum __attribute__ ((__packed__)) access {
	ACC_GET,
	ACC_SET,
	ACC_RESET
};

enum __attribute__ ((__packed__)) command {
	CMD_EMIT,
	CMD_CAPS,
	CMD_FW,
	CMD_ALARM,
	CMD_MACRO,
	CMD_WAKE
};

enum __attribute__ ((__packed__)) status {
	STAT_CMD,
	STAT_SUCCESS,
	STAT_FAILURE
};

extern uint32_t AlarmValue;

void eeprom_store(uint8_t virt_addr, uint8_t *buf);
void eeprom_restore(uint8_t *buf, uint8_t virt_addr);
void enable_ir_receiver(uint8_t enable);
void delay_ms(unsigned int msec);

int8_t get_handler(uint8_t *buf);
int8_t set_handler(uint8_t *buf);
int8_t reset_handler(uint8_t *buf);

#endif /* __HANDLER_H */
