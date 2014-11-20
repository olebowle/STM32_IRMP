#ifndef __CONFIG_H
#define __CONFIG_H

#define MACRO_SLOTS	8
#define MACRO_DEPTH	8
#define WAKE_SLOTS	8
#define SIZEOF_IR	3

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

#endif /* __CONFIG_H */
