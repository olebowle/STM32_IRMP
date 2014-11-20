#include "config.h"
#include "led.h"
#include "stm32f10x.h"

uint8_t PA9_state = 0;

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

/* toggle red [/ yellow] and external LED */
void toggle_LED(void)
{
	if (!PA9_state) {
		LED_init();
		GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET);
		PA9_state = 1;
	} else {
		LED_deinit();
		PA9_state = 0;
	}
	/*GPIOA->ODR ^= GPIO_Pin_9;*/
	OUT_PORT->ODR ^= LED_PIN;
}
#else
/* not implemented */
void LED_init(void) {};
void LED_deinit(void) {};
void fast_toggle(void) {};
void both_on(void) {};
void yellow_on(void) {};
void toggle_LED(void) {};
#endif /* ST_Link */
