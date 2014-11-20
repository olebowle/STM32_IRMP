#include <inttypes.h>
#include <string.h>
#include "config.h"
#include "handler.h"

int8_t get_handler(uint8_t *buf)
{
	/* number of valid bytes in buf, -1 signifies error */
	int8_t ret = 3;
	uint8_t idx;

	switch ((enum command) buf[2]) {
	case CMD_ALARM:
		/* AlarmValue -> buf[3-6] */
		memcpy(&buf[3], &AlarmValue, sizeof(AlarmValue));
		ret += sizeof(AlarmValue);
		break;
	case CMD_MACRO:
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * buf[3] + SIZEOF_IR * buf[4];
		eeprom_restore(&buf[3], idx);
		ret += SIZEOF_IR * sizeof(uint16_t);
		break;
	case CMD_WAKE:
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * MACRO_SLOTS + SIZEOF_IR * buf[3];
		eeprom_restore(&buf[3], idx);
		ret += SIZEOF_IR * sizeof(uint16_t);
		break;
	default:
		ret = -1;
	}

	return ret;
}

int8_t set_handler(uint8_t *buf)
{
	/* number of valid bytes in buf, -1 signifies error */
	int8_t ret = 3;
	uint8_t idx;
	uint8_t tmp[SIZEOF_IR * sizeof(uint16_t)];

	switch ((enum command) buf[2]) {
	case CMD_ALARM:
		AlarmValue = (buf[3]<<24)|(buf[4]<<16)|(buf[5]<<8)|buf[6];
		break;
	case CMD_MACRO:
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * buf[3] + SIZEOF_IR * buf[4];
		eeprom_store(idx, &buf[5]);
		ret += SIZEOF_IR * sizeof(uint16_t);
		/* validate stored value in eeprom */
		eeprom_restore(tmp, idx);
		if (memcmp(&buf[5], tmp, sizeof(tmp)))
			ret = -1;
		break;
	case CMD_WAKE:
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * MACRO_SLOTS + SIZEOF_IR * buf[3];
		eeprom_store(idx, &buf[4]);
		/* validate stored value in eeprom */
		eeprom_restore(tmp, idx);
		if (memcmp(&buf[4], tmp, sizeof(tmp)))
			ret = -1;
		break;
	default:
		ret = -1;
	}

	return ret;
}

int8_t reset_handler(uint8_t *buf)
{
	/* number of valid bytes in buf, -1 signifies error */
	int8_t ret = 3;
	uint8_t idx;
	uint8_t zeros[SIZEOF_IR * sizeof(uint16_t)] = {0};

	switch ((enum command) buf[2]) {
	case CMD_ALARM:
		AlarmValue = 0xFFFFFFFF;
		break;
	case CMD_MACRO:
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * buf[3] + SIZEOF_IR * buf[4];
		eeprom_store(idx, zeros);
		ret += SIZEOF_IR * sizeof(uint16_t);
		break;
	case CMD_WAKE:
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * MACRO_SLOTS + SIZEOF_IR * buf[3];
		eeprom_store(idx, zeros);
		break;
	default:
		ret = -1;
	}
	return ret;
}
