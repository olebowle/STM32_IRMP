#include "irmpmain.h"

uint32_t SysCtlClockGet(void)
{
	RCC_ClocksTypeDef RCC_ClocksStatus;
	RCC_GetClocksFreq(&RCC_ClocksStatus);
	return RCC_ClocksStatus.SYSCLK_Frequency;
}

/* initalize timer and interrupt */
void IRMP_Init(void)
{

	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 7;
	TIM_TimeBaseStructure.TIM_Prescaler = ((F_CPU / F_INTERRUPTS)/8) - 1;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;
	NVIC_Init(&NVIC_InitStructure);

	TIM_Cmd(TIM2, ENABLE);

	irmp_init();
}

void TIM2_IRQHandler(void)
{
	TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	/* only if one timer is used for both irmp and irsnd */
//	if (! irsnd_ISR()) { /* call irsnd ISR */
	irmp_ISR(); /* if not busy call irmp ISR */
//	}
}
