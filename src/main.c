/*
 *  IR receiver, sender, USB wakeup, motherboard switch wakeup, wakeup timer,
 *  USB HID device, eeprom emulation
 *
 *  Copyright (C) 2014 Joerg Riechardt
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "stm32f10x.h"
#include "core_cm3.h"
#include "usb_hid.h"
#include "irmpmain.h"
#include "irsndmain.h"
#include "eeprom.h"

/* uncomment this, if you use a ST-Link */
/*#define ST_Link*/

/*
 * only if you want to use CLK and DIO on the blue ST-Link Emulator with mistakenly connected Pins
 * WARNING: further firmware updates will become difficult!
 * better use TMS and TCK instead, and leave this commented out
 */
/*#define BlueLink_Remap*/

/* for use of wakeup reset pin */
#define WAKEUP_RESET

#ifdef BlueLink_Remap
#define OUT_PORT	GPIOA
#define LED_PIN		GPIO_Pin_14
#define WAKEUP_PIN	GPIO_Pin_13
#else
#define OUT_PORT	GPIOB
#define LED_PIN		GPIO_Pin_13
#define WAKEUP_PIN	GPIO_Pin_14
#endif /* BlueLink_Remap */

#ifdef WAKEUP_RESET
#define WAKEUP_RESET_PIN GPIO_Pin_12
#endif /* WAKEUP_RESET */

#define SND_MAX 2

#define ACCESS_COUNT 3
enum __attribute__ ((__packed__)) access {
	ACC_GET,
	ACC_SET,
	ACC_RESET
};

#define COMMAND_COUNT 6
enum __attribute__ ((__packed__)) command {
	CMD_EMIT,
	CMD_CAPS,
	CMD_FW,
	CMD_ALARM,
	CMD_MACRO,
	CMD_WAKE
};

#define STAT_COUNT 3
enum __attribute__ ((__packed__)) status {
	STAT_CMD,
	STAT_SUCCESS,
	STAT_FAILURE
};

__IO uint8_t PrevXferComplete = 1;
uint8_t PA9_state = 0;
uint8_t buf[HID_OUT_BUFFER_SIZE];
uint8_t wakeup_buf[6];
uint32_t timestamp = 0;
uint32_t AlarmValue = 0xFFFFFFFF;
volatile unsigned int systicks = 0;
#ifdef ST_Link
volatile unsigned int systicks2 = 0;
#endif /* ST_Link */

void delay_ms(unsigned int msec)
{
	systicks = 0;
	while (systicks <= msec);
}

void LED_Switch_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
#ifdef BlueLink_Remap
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	/* disable SWD, so pins are available */
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
#else
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
#endif /* BlueLink_Remap */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(OUT_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
	GPIO_Init(OUT_PORT, &GPIO_InitStructure);
	/* wakeup reset pin */
#ifdef	WAKEUP_RESET
	GPIO_InitStructure.GPIO_Pin = WAKEUP_RESET_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(OUT_PORT, &GPIO_InitStructure);
#endif /* WAKEUP_RESET */
	/* start with LED on */
	GPIO_WriteBit(OUT_PORT, LED_PIN, Bit_SET);
}

#ifdef ST_Link
void LED_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	/* PA9 (red + yellow LED on ST-Link Emus) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void LED_deinit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	/* PA9 (red + yellow LED) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* red + yellow fast toggle */
void fast_toggle(void)
{
	LED_init();
	systicks2 = 0;
	while (systicks2 <= 500) {
		GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET);
		delay_ms(50);
		GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_RESET);
		delay_ms(50);
	}
	/* off */
	LED_deinit();
	PA9_state = 0;
	/* red on */
	/*GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET);*/
}

/* red + yellow both on */
void both_on(void)
{
	LED_init();
	systicks2 = 0;
	while (systicks2 <= 500) {
		GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET);
		delay_ms(1);
		GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_RESET);
		delay_ms(1);
	}
	/* red on */
	/*GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET);*/
	/* off */
	LED_deinit();
}

/* yellow on */
void yellow_on(void)
{
	LED_init();
	GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_RESET);
	delay_ms(100);
	/* red on */
	/*GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET);*/
	/* off */
	LED_deinit();
}
#else
void LED_init(void) {}
void LED_deinit(void) {}
void fast_toggle(void) {}
void both_on(void) {}
void yellow_on(void) {}
#endif /* ST_Link */

/* toggle red [/ yellow] and external LED */
void toggle_LED(void)
{
#ifdef ST_Link
	if (!PA9_state) {
		LED_init();
		GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET);
		PA9_state = 1;
	} else {
		LED_deinit();
		PA9_state = 0;
	}
	/*GPIOA->ODR ^= GPIO_Pin_9;*/
#endif /* ST_Link */
	OUT_PORT->ODR ^= LED_PIN;
}

/*
 * eeprom: after writing 0xABCD into virtual address 0x6666, you have CDAB 6666 in flash
 * 2 halfwords in reverse order, little endian
 */

void eeprom_store(uint8_t virt_addr, uint8_t *buf)
{
	/* reverse order */
	EE_WriteVariable(virt_addr,	(buf[1] << 8) | buf[0]);
	EE_WriteVariable(virt_addr + 1,	(buf[3] << 8) | buf[2]);
	EE_WriteVariable(virt_addr + 2,	(buf[5] << 8) | buf[4]);
}

void eeprom_restore(uint8_t *buf, uint8_t virt_addr)
{
	uint16_t EE_Data;
	/* reverse order */
	EE_ReadVariable(virt_addr, &EE_Data);
	memcpy(&buf[0], &EE_Data, 2);

	EE_ReadVariable(virt_addr + 1, &EE_Data);
	memcpy(&buf[2], &EE_Data, 2);

	EE_ReadVariable(virt_addr + 2, &EE_Data);
	memcpy(&buf[4], &EE_Data, 2);
}

/*
 * IRData -> buf[0-5]
 * irmplircd expects dummy as first byte (Report ID),
 * so start with buf[0], adapt endianness for irmplircd
 */
void IRData_to_buf(uint8_t *buf, IRMP_DATA *IRData)
{
	buf[0] = IRData->protocol;
	buf[2] = ((IRData->address) >> 8) & 0xFF;
	buf[1] = (IRData->address) & 0xFF;
	buf[4] = ((IRData->command) >> 8) & 0xFF;
	buf[3] = (IRData->command) & 0xFF;
	buf[5] = IRData->flags;
}

/* buf[0...5] -> IRData */
void buf_to_IRData(IRMP_DATA *IRData, uint8_t *buf)
{
	IRData->protocol = buf[0];
	IRData->address = (buf[1] << 8) | buf[2];
	IRData->command = (buf[3] << 8) | buf[4];
	IRData->flags = buf[5];
}

/* buf[0-5] <-> IRData */
uint8_t cmp_buf_IRData(uint8_t *buf, IRMP_DATA *IRData)
{
	return	IRData->protocol == buf[0] && \
		IRData->address == ((buf[1] << 8) | buf[2]) && \
		IRData->command == ((buf[3] << 8) | buf[4]) && \
		IRData->flags == buf[5];
}

void Systick_Init(void)
{
	/* 1ms */
	SysTick_Config((SysCtlClockGet()/1000));
}

void SysTick_Handler(void)
{
	static uint16_t i = 0;
	systicks++;
#ifdef ST_Link
	systicks2++;
#endif /* ST_Link */
	timestamp++;
	if (i == 1000) {
		if (AlarmValue)
			AlarmValue--;
		i = 0;
	} else {
		i++;
	}
}

void uint32_to_buf(uint8_t *buf, uint32_t val)
{
	buf[0] = ((val) >> 24) & 0xFF;
	buf[1] = ((val) >> 16) & 0xFF;
	buf[2] = ((val) >> 8) & 0xFF;
	buf[3] = val & 0xFF;
}

void Wakeup(void)
{
	AlarmValue = 0xFFFFFFFF;
	/* USB wakeup */
	Resume(RESUME_START);
	/* motherboard switch: WAKEUP_PIN short high */
	GPIO_WriteBit(OUT_PORT, WAKEUP_PIN, Bit_SET);
	delay_ms(500);
	GPIO_WriteBit(OUT_PORT, WAKEUP_PIN, Bit_RESET);
	/* both_on(); */
	fast_toggle();
}

/* put wakeup IRData into buf and wakeup eeprom */
void store_new_wakeup(void)
{
	IRMP_DATA wakeup_IRData;
	toggle_LED();
	systicks = 0;
	/* 5 seconds to press button on remote */
	delay_ms(5000);
	if (irmp_get_data(&wakeup_IRData))
	/* wakeup_IRData -> buf[0-5] */
	IRData_to_buf(buf, &wakeup_IRData);
	/* set flags to 0 */
	buf[5] = 0;
	/* buf[0-5] -> eeprom[0-2] */
	eeprom_store(0, buf);
	toggle_LED();
}

int8_t get_handler(uint8_t *buf)
{
	/* number of valid bytes in buf, -1 signifies error */
	int8_t ret = 3;
	uint8_t idx;

	switch ((enum command) buf[2]) {
	case CMD_ALARM:
		/* AlarmValue -> buf[3-6] */
		uint32_to_buf(&buf[3], AlarmValue);
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

	switch ((enum command) buf[2]) {
	case CMD_ALARM:
		AlarmValue = (buf[3]<<24)|(buf[4]<<16)|(buf[5]<<8)|buf[6];
		break;
	case CMD_MACRO:
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * buf[3] + SIZEOF_IR * buf[4];
		eeprom_store(idx, &buf[5]);
		ret += SIZEOF_IR * sizeof(uint16_t);
		break;
	case CMD_WAKE:
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * MACRO_SLOTS + SIZEOF_IR * buf[3];
		eeprom_store(idx, &buf[4]);
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
	uint8_t zeros[6] = {0};

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

int main(void)
{
	uint8_t k, trigger_send_buf[6], send_buf[SND_MAX][6];
	IRMP_DATA myIRData, loopIRData, sendIRData, lastIRData = { 0, 0, 0, 0};
	int8_t ret;

	LED_Switch_init();
	LED_init();
	/* red LED on */
	/*GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET);*/
	USB_HID_Init();
	IRMP_Init();
	IRSND_Init();
	FLASH_Unlock();
	EE_Init();
	Systick_Init();

	/* read wakeup IR-data from eeprom: eeprom[0-2] -> wakeup_buf */
	eeprom_restore(wakeup_buf, 0);

	/* read trigger IR-data from eeprom: eeprom[3-5] -> trigger_send_buf */
	eeprom_restore(trigger_send_buf, 3);

	/* read IR-data to send from eeprom: eeprom[6-8] -> send_buf[0], eeprom[9-11] -> send_buf[1], etc */
	for (k=0; k<SND_MAX; k++)
		eeprom_restore(send_buf[k], 6+k*3);

	while (1) {
		if (!AlarmValue)
			Wakeup();

#ifdef	WAKEUP_RESET
		/* wakeup reset pin pulled low? */
		if (!GPIO_ReadInputDataBit(OUT_PORT, WAKEUP_RESET_PIN)) {
			/* put wakeup IRData into buf and wakeup eeprom */
			store_new_wakeup();
			memcpy(wakeup_buf, buf, sizeof(wakeup_buf));
		}
#endif /* WAKEUP_RESET */

		/* test if USB is connected to PC, sendtransfer is complete and data is received */
		if (USB_HID_GetStatus() == CONFIGURED && PrevXferComplete && USB_HID_ReceiveData(buf) == RX_READY && buf[0] == STAT_CMD) {

			switch ((enum access) buf[1]) {
			case ACC_GET:
				ret = get_handler(buf);
				break;
			case ACC_SET:
				ret = set_handler(buf);
				break;
			case ACC_RESET:
				ret = reset_handler(buf);
				break;
			default:
				ret = -1;
			}

			if (ret == -1) {
				buf[0] = STAT_FAILURE;
				ret = 3;
			} else {
				buf[0] = STAT_SUCCESS;
			}

			/* send (modified) data (for verify) */
			USB_HID_SendData(buf, ret);
			toggle_LED();
		}

		/* poll IR-data */
		if (irmp_get_data(&myIRData)) {
			/* new IR-Data? */
			/* omit flags */
			if (!(	myIRData.protocol == lastIRData.protocol && \
				myIRData.address == lastIRData.address && \
				myIRData.command == lastIRData.command )) {

				toggle_LED();
				lastIRData.protocol = myIRData.protocol;
				lastIRData.address = myIRData.address;
				lastIRData.command = myIRData.command;
			}

			/* wakeup IR-data? */
			if (cmp_buf_IRData(wakeup_buf, &myIRData))
				Wakeup();

			/* trigger send IR-data? */
			if (cmp_buf_IRData(trigger_send_buf, &myIRData)) {
				for (k=0; k < SND_MAX; k++) {
					/* ?? 100 too small, 125 ok, RC5 is 114ms */
					delay_ms(115);
					/* send_buf[k] -> sendIRData */
					buf_to_IRData(&sendIRData, send_buf[k]);
					/* 0|1: don't|do wait until send finished */
					irsnd_send_data(&sendIRData, 1);
					yellow_on();
					/* ?? */
					delay_ms(300);
					/* receive sent by myself too, TODO */
					if (irmp_get_data(&loopIRData)) {
						memset(buf, 0, sizeof(buf));
						/* loopIRData -> buf[0-5] */
						IRData_to_buf(buf, &loopIRData);
						/* timestamp -> buf[6-9] */
						uint32_to_buf(&buf[6], timestamp);
						USB_HID_SendData(buf,11);
					}
				}
			}

			/* send IR-data via USB-HID */
			memset(buf, 0, sizeof(buf));
			/* myIRData -> buf[0-5] */
			IRData_to_buf(buf, &myIRData);
			/* timestamp -> buf[6-9] */
			uint32_to_buf(&buf[6], timestamp);
			USB_HID_SendData(buf, 11);
		}
	}
}
