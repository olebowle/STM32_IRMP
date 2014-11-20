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
#include "handler.h"
#include "led.h"

__IO uint8_t PrevXferComplete = 1;
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

/* put wakeup IRData into eeprom */
void store_new_wakeup(void)
{
	IRMP_DATA wakeup_IRData;
	toggle_LED();
	systicks = 0;
	/* 5 seconds to press button on remote */
	delay_ms(5000);
	if (irmp_get_data(&wakeup_IRData)) {
		wakeup_IRData.flags = 0;
		/* store wakeup-code learned by remote in first wakeup slot */
		eeprom_store((MACRO_DEPTH + 1) * SIZEOF_IR * MACRO_SLOTS, (uint8_t *) &wakeup_IRData);
		toggle_LED();
	}
}

/* is received ir-code in one of the wakeup-slots? wakeup if true */
void check_wakeups(IRMP_DATA *ir) {
	uint8_t i, idx;
	uint8_t buf[SIZEOF_IR * sizeof(uint16_t)];

	for (i=0; i < WAKE_SLOTS; i++) {
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * MACRO_SLOTS + SIZEOF_IR * i;
		eeprom_restore(buf, idx);
		if (!memcmp(buf, ir, sizeof(ir)))
			Wakeup();
	}
}

void enable_ir_receiver(uint8_t enable)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = enable ? GPIO_Mode_IN_FLOATING : GPIO_Mode_AIN;
	GPIO_Init(OUT_PORT, &GPIO_InitStructure);
}

void transmit_macro(uint8_t macro)
{
	uint8_t i, idx;
	uint8_t buf[SIZEOF_IR * sizeof(uint16_t)];
	uint8_t zeros[SIZEOF_IR * sizeof(uint16_t)] = {0};

	/* disable receiving of ir, since we don't want to rx what we txed*/
	enable_ir_receiver(0);
	/* we start from 1, since we don't want to tx the trigger code of the macro*/
	for (i=1; i < MACRO_DEPTH + 1; i++) {
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * macro + SIZEOF_IR * i;
		eeprom_restore(buf, idx);
		/* first encounter of zero in macro means end of macro */
		if (!memcmp(buf, &zeros, sizeof(zeros)))
			break;
		irsnd_send_data((IRMP_DATA *) buf, 1);
	}
	/* reenable receiving of ir */
	enable_ir_receiver(1);
}

/* is received ir-code (trigger) in one of the macro-slots? transmit_macro if true */
void check_macros(IRMP_DATA *ir) {
	uint8_t i, idx;
	uint8_t buf[SIZEOF_IR * sizeof(uint16_t)];

	for (i=0; i < MACRO_SLOTS; i++) {
		idx = (MACRO_DEPTH + 1) * SIZEOF_IR * i;
		eeprom_restore(buf, idx);
		if (!memcmp(buf, ir, sizeof(ir)))
			transmit_macro(i);
	}
}

int main(void)
{
	uint8_t buf[HID_OUT_BUFFER_SIZE];
	IRMP_DATA myIRData, lastIRData = { 0, 0, 0, 0};
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

	while (1) {
		if (!AlarmValue)
			Wakeup();

#ifdef	WAKEUP_RESET
		/* wakeup reset pin pulled low? */
		if (!GPIO_ReadInputDataBit(OUT_PORT, WAKEUP_RESET_PIN)) {
			/* put wakeup IRData into buf and wakeup eeprom */
			store_new_wakeup();
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

			check_wakeups(&myIRData);

			check_macros(&myIRData);

			/* send IR-data via USB-HID */
			memset(buf, 0, sizeof(buf));
			/* myIRData -> buf[0-5] */
			memcpy(buf, &myIRData, sizeof(myIRData));
			/* timestamp -> buf[6-9] */
			memcpy(&buf[sizeof(myIRData)], &timestamp, sizeof(timestamp));
			USB_HID_SendData(buf, sizeof(myIRData) + sizeof(timestamp));
		}
	}
}
