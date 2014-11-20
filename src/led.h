#ifndef __LED_H
#define __LED_H

#include "config.h"

void LED_Switch_init(void);

extern volatile unsigned int systicks2;
void delay_ms(unsigned int msec);
void LED_init(void);
void LED_deinit(void);
void fast_toggle(void);
void both_on(void);
void yellow_on(void);
void toggle_LED(void);

#endif /* __LED_H */
