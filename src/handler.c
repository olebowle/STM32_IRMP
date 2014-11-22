#include <inttypes.h>
#include <string.h>
#include "config.h"
#include "handler.h"
#include "irsnd.h"

/* keep in sync with ir{mp,snd}config.h  */
const uint8_t caps_packet[] = {
	IRMP_SIRCS_PROTOCOL,
	IRMP_NEC_PROTOCOL,
	IRMP_SAMSUNG_PROTOCOL,
	IRMP_KASEIKYO_PROTOCOL,
	IRMP_JVC_PROTOCOL,
	IRMP_NEC16_PROTOCOL,
	IRMP_NEC42_PROTOCOL,
	IRMP_MATSUSHITA_PROTOCOL,
	IRMP_DENON_PROTOCOL,
	IRMP_RC5_PROTOCOL,
	IRMP_RC6_PROTOCOL,
	IRMP_RC6A_PROTOCOL,
	IRMP_IR60_PROTOCOL,
	IRMP_GRUNDIG_PROTOCOL,
	IRMP_SIEMENS_PROTOCOL,
	IRMP_NOKIA_PROTOCOL,
	0
};

int8_t get_handler(uint8_t *buf)
{
	/* number of valid bytes in buf, -1 signifies error */
	int8_t ret = 3;
	uint8_t idx;

	switch ((enum command) buf[2]) {
	case CMD_CAPS:
		/* in first query we give informaton about slots and depth */
		if (!buf[3]) {
			buf[3] = MACRO_SLOTS;
			buf[4] = MACRO_DEPTH;
			buf[5] = WAKE_SLOTS;
			ret += 3;
			break;
		}
		/* in later queries we give information about supported protocols */
		idx = BYTES_PER_QUERY * (buf[3] - 1);
		memcpy(&buf[3], &caps_packet[idx], BYTES_PER_QUERY);
		/* actually this is not true for the last transmission,
		 * but it doesn't matter since it's NULL terminated
		 */
		ret = HID_IN_BUFFER_SIZE;
		break;
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
	case CMD_EMIT:
		/* disable receiving of ir, since we don't want to rx what we txed*/
		enable_ir_receiver(0);
		irsnd_send_data((IRMP_DATA *) &buf[3], 1);
		delay_ms(300);
		/* reenable receiving of ir */
		enable_ir_receiver(1);
		break;
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
