#ifndef __BSP_H__
#define __BSP_H__


#include "stm32f10x.h"  /* CMSIS-compliant header file for the MCU used */
#include "bsp_usart.h"


#define BSP_TICKS_PER_SEC    100U

#define POWER_PORT	GPIOA
#define POWER_PIN	GPIO_Pin_12

#define PWRKEY_PORT	GPIOA
#define PWRKEY_PIN	GPIO_Pin_8

#define L206_SLEEP_PORT GPIOA
#define L206_SLEEP_PIN  GPIO_Pin_11

#define L206_POWER_OFF()		GPIO_ResetBits(POWER_PORT, POWER_PIN)		//电源是能控制端
#define L206_POWER_ON()			GPIO_SetBits  (POWER_PORT, POWER_PIN)		//电源是能控制端

#define L206_PWRKEY_LOW()		GPIO_SetBits  (PWRKEY_PORT, PWRKEY_PIN)		//  low
#define L206_PWRKEY_HIGH()		GPIO_ResetBits(PWRKEY_PORT, PWRKEY_PIN)		//  high

#define L206_SLEEP_PRESS()		GPIO_SetBits(L206_SLEEP_PORT, L206_SLEEP_PIN)
#define L206_SLEEP_RELEASE()	GPIO_ResetBits(L206_SLEEP_PORT, L206_SLEEP_PIN)


#define FIFO_MAX_LENGTH	2048U


#ifdef Q_SPY
#define debug_print(fmt, ...)	do{ \
									_debug_print(fmt, ##__VA_ARGS__); \
								}while(0)
#else
#define debug_print(fmt, ...)
#endif // QSPY

void set_timestamp(uint64_t stamp);
uint64_t get_timestamp(void);
void _debug_print(char *fmt, ...);
void BSP_init(void);

#endif
