#include "qpc.h"
#include "charger.h"
#include "bsp.h"

#include <system.h>
#include <fifo.h>
Q_DEFINE_THIS_MODULE("bsp")

enum KernelAwareISRs {
	USART1_PRIO = QF_AWARE_ISR_CMSIS_PRI, /* see NOTE00 */
	USART3_PRIO,
	SYSTICK_PRIO,
	/* ... */
	MAX_KERNEL_AWARE_CMSIS_PRI /* keep always last */
};
/* "kernel-aware" interrupts should not overlap the PendSV priority */
Q_ASSERT_COMPILE(MAX_KERNEL_AWARE_CMSIS_PRI <= (0xFF >> (8 - __NVIC_PRIO_BITS)));

extern fifo_t receiver_fifo;
static uint64_t m_timestamp = 31507200; // 默认值为1971-01-01 00:00:00
static uint8_t timestamp_factor_count = 0;

void SysTick_Handler(void);
void BSP_RCC_init(void);
void BSP_NVIC_init(void);
void BSP_l206_gpio_init(void);

#ifdef Q_SPY
	QSTimeCtr QS_tickTime_;
	QSTimeCtr QS_tickPeriod_;

	/* event-source identifiers used for tracing */
	static uint8_t const l_SysTick_Handler = 0U;
	static uint8_t const l_Usart1_IRQHandler = 0U;
	static uint8_t const l_Usart3_IRQHandler = 0U;
	static uint8_t const l_debug_port = 0U;
	static uint8_t const l_usart_port = 0U;

	enum AppRecords { /* application-specific trace records */
		USER_DEBUG_PORT = QS_USER,
		USER_USART_PORT
	};

#endif

/* BSP functions ===========================================================*/
void BSP_init(void) {

	/* initial reset and clock control*/
	BSP_RCC_init();
	BSP_usart1_init();
	BSP_usart3_init();
	BSP_l206_gpio_init();

	/* NOTE: SystemInit() has been already called from the startup code
	*  but SystemCoreClock needs to be updated
	*/
	SystemCoreClockUpdate();

#ifdef Q_SPY
	/* system tick configuration*/
	QS_tickTime_ = 0;
	QS_tickPeriod_ = SystemCoreClock / BSP_TICKS_PER_SEC;
#endif
						   /* initialize the QS software tracing... */
	if (QS_INIT((void *)0) == 0U) {
		Q_ERROR();
	}
	QS_OBJ_DICTIONARY(&l_SysTick_Handler);
	QS_OBJ_DICTIONARY(&l_Usart1_IRQHandler);
	QS_OBJ_DICTIONARY(&l_debug_port);
	QS_OBJ_DICTIONARY(&l_usart_port);


}

void set_timestamp(uint64_t stamp)
{
	QF_INT_DISABLE();
	m_timestamp = stamp;
	QF_INT_ENABLE();
}

uint64_t get_timestamp(void)
{
	return m_timestamp;
}

/* ISRs used in the application ==========================================*/
void SysTick_Handler(void) {   /* system clock tick ISR */
							   /* state of the button debouncing, see below */
#ifdef Q_SPY
	{
		uint32_t tmp;

		tmp = SysTick->CTRL; /* clear CTRL_COUNTFLAG */
		QS_tickTime_ += QS_tickPeriod_; /* account for the clock rollover */
	}
#endif

	/* 系统时间戳计数 */
	if (++timestamp_factor_count >= BSP_TICKS_PER_SEC) {
		timestamp_factor_count = 0;
		QF_INT_DISABLE();
		m_timestamp++;
		QF_INT_ENABLE();
	}
	
	QF_TICK_X(0U, &l_SysTick_Handler); /* process time events for rate 0 */
}

void USART1_IRQHandler(void)
{
	if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET)	// 溢出错误中断
	{
		USART_ClearFlag(USART1, USART_FLAG_ORE);	// 清除标志
		USART_ReceiveData(USART1);
	}

	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)   //接收数据中断
	{
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);	// 清中断标记

		uint16_t d = USART_ReceiveData(USART1);	/* Get received byte */


		if (fifo_len(&receiver_fifo) == 0)
		{
			static QEvt const beginEvt = { UART_DATA_BEGIN_SIG, 0U, 0U };
			QACTIVE_POST(AO_Module_rx, &beginEvt, &l_Usart1_IRQHandler);
		}

		fifo_in_c(&receiver_fifo, (uint8_t)d); //put into receiver fifo

		/* 在临界区内部或ISR内部，应该调用带_NOCRIT宏，避免临界区嵌套 */
		//QS_BEGIN_NOCRIT(USER_USART_PORT, &l_usart_port); /* application-specific record begin */
		//	//QS_STR(&d);                 /* debug info */
		//	QS_U8(1, d);
		//QS_END_NOCRIT()
	}
}

uint8_t bat_statue = 0;
uint8_t batlen = 0;
uint8_t bat_buf[8] = { 0 };
uint8_t npos = 0;
uint16_t bat_sum = 0;
uint8_t heartbeat_count = 0;
report_frame_u frame_last = { 0 };
void USART3_IRQHandler(void)
{
	uint8_t d;

	if (USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET)	// 溢出错误中断
	{
		USART_ClearFlag(USART3, USART_FLAG_ORE);	// 清除标志
		USART_ReceiveData(USART3);
	}

	if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)   //接收数据中断
	{
		USART_ClearITPendingBit(USART3, USART_IT_RXNE);    // 清中断标记
		d = USART_ReceiveData(USART3);    /* Get received byte */

		switch (bat_statue) {
			case 0x00: {
				if (d == 0xAA) {
					bat_statue = 0x01;
				}
				else {
					bat_statue = 0x00;
				}
				npos = 0;
				break;
			}
			case 0x01: {
				npos = 0;
				if (d == 2 || d == 4) {
					bat_statue = 0x02;
					batlen = d;
					bat_buf[npos++] = d;
				}
				else {
					bat_statue = 0x00;
					npos = 0;
				}
				break;
			}
			case 0x02: {
				bat_buf[npos] = d;
				npos++;
				if (npos > batlen + 1) {
					if (bat_buf[1] == 0x01) { // 上报帧
						bat_sum = 0;
						for (npos = 0; npos < batlen + 1; npos++) {
							bat_sum += bat_buf[npos];
							if (bat_sum > 0xff) {
								bat_sum = ~bat_sum;
								bat_sum += 1;
							}
							bat_sum = bat_sum & 0xff;
						}
						if (bat_sum == bat_buf[npos]) { //校验和正确
							//debug_print("%x,%x", bat_buf[2] & 0xE0, bat_buf[4]);
							if ((bat_buf[2] & 0xE0) != (frame_last.buf[0] & 0xE0)
								|| (bat_buf[4] != frame_last.buf[2]))
							{
								memcpy(&frame_last.buf[0], &bat_buf[2], sizeof(frame_last.buf));
								//debug_print("charge:%d, full:%d, out:%d, percent:%d, sizeof(buf):%d", frame_last.struct_.bit_oncharging,
									//frame_last.struct_.bit_charge_full, frame_last.struct_.bit_outputing, 
									//frame_last.struct_.bit_vol_percent, sizeof(frame_last.buf));

								batteryReportEvt *reportEvt = Q_NEW(batteryReportEvt, BATTERY_REPORT_SIG);
								memcpy(&reportEvt->frame.buf[0], &bat_buf[2], sizeof(frame_last.buf));
								QACTIVE_POST(AO_Battery, (QEvt *)reportEvt, &l_Usart3_IRQHandler);

								//battery.charge_flag = (bat_buf[2] >> 7) & 0x01;
								//battery.chargefull = (bat_buf[2] >> 6) & 0x01;
								//battery.loadflag = (bat_buf[2] >> 5) & 0x01;
								//battery.vol = (bat_buf[2] & 0x1F) * 256 + bat_buf[3];
								//battery.out_flag = (bat_buf[4] >> 7) & 0x01;
								//battery.temp_soc = bat_buf[4] & 0x7F;
								//if (battery.temp_soc <= 1)       // 忽略掉等于 0 时候的参数值
								//	battery.temp_soc = 1;
								//battery.err_bat = 0;
								//battery_timeout = GetTickCount64();					
							}

							if (++heartbeat_count >= 120) {
								heartbeat_count = 0;
								QACTIVE_POST(AO_Battery, Q_NEW(QEvt, BATTERY_HEARTBEAT_SIG), &l_Usart3_IRQHandler);
							}
						}
					}
						bat_statue = 0x00;
						npos = 0;
				}
				if (bat_buf[0] == 0x02) {
					bat_statue = 0x00;
					npos = 0;
				}
			break;
			}
			default: {
				bat_statue = 0x00;
				break;
			}
		}
	}
}

/* QF callbacks ============================================================*/
void QF_onStartup(void) {
	/* set up the SysTick timer to fire at BSP_TICKS_PER_SEC rate */
	SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);

	BSP_NVIC_init();
}

/*..........................................................................*/
void QF_onCleanup(void) {
}
/*..........................................................................*/
void QV_onIdle(void) {  /* called with interrupts disabled, see NOTE01 */

						/* toggle an LED on and then off (not enough LEDs, see NOTE02) */
	QF_INT_DISABLE();
	//GPIOA->BSRRL |= LED_LD2;  /* turn LED[n] on  */
	//GPIOA->BSRRH |= LED_LD2;  /* turn LED[n] off */
	QF_INT_ENABLE();

#ifdef Q_SPY
	if ((USART2->SR & 0x0080U) != 0) {  /* is TXE empty? */
		uint16_t b;

		QF_INT_DISABLE();
		b = QS_getByte();
		QF_INT_ENABLE();

		if (b != QS_EOD) {  /* not End-Of-Data? */
			USART2->DR = (b & 0xFFU);  /* put into the DR register */
		}
	}
#elif defined NDEBUG
	/* Put the CPU and peripherals to the low-power mode.
	* you might need to customize the clock management for your application,
	* see the datasheet for your particular Cortex-M3 MCU.
	*/
	/* !!!CAUTION!!!
	* The WFI instruction stops the CPU clock, which unfortunately disables
	* the JTAG port, so the ST-Link debugger can no longer connect to the
	* board. For that reason, the call to __WFI() has to be used with CAUTION.
	*
	* NOTE: If you find your board "frozen" like this, strap BOOT0 to VDD and
	* reset the board, then connect with ST-Link Utilities and erase the part.
	* The trick with BOOT(0) is it gets the part to run the System Loader
	* instead of your broken code. When done disconnect BOOT0, and start over.
	*/
	//QV_CPU_SLEEP();  /* atomically go to sleep and enable interrupts */
	QF_INT_ENABLE(); /* for now, just enable interrupts */
#else
	QF_INT_ENABLE(); /* just enable interrupts */
#endif
}

void Q_onAssert(char const *module, int loc) {
	/*
	* NOTE: add here your application-specific error handling
	*/
	(void)module;
	(void)loc;
	QS_ASSERTION(module, loc, (uint32_t)10000U); /* report assertion to QS */
	NVIC_SystemReset();
}

/* QS callbacks ============================================================*/
#ifdef Q_SPY
/*..........................................................................*/
#define __DIV(__PCLK, __BAUD)       (((__PCLK / 4) *25)/(__BAUD))
#define __DIVMANT(__PCLK, __BAUD)   (__DIV(__PCLK, __BAUD)/100)
#define __DIVFRAQ(__PCLK, __BAUD)   \
    (((__DIV(__PCLK, __BAUD) - (__DIVMANT(__PCLK, __BAUD) * 100)) \
        * 16 + 50) / 100)
#define __USART_BRR(__PCLK, __BAUD) \
    ((__DIVMANT(__PCLK, __BAUD) << 4)|(__DIVFRAQ(__PCLK, __BAUD) & 0x0F))

/*..........................................................................*/
uint8_t QS_onStartup(void const *arg) {
	static uint8_t qsBuf[2 * 1024]; /* buffer for Quantum Spy */

	(void)arg; /* avoid the "unused parameter" compiler warning */
	QS_initBuf(qsBuf, sizeof(qsBuf));

	BSP_usart2_init();


	/* setup the QS filters... */
	QS_FILTER_ON(QS_QEP_STATE_ENTRY);
	QS_FILTER_ON(QS_QEP_STATE_EXIT);
	QS_FILTER_ON(QS_QEP_STATE_INIT);
	QS_FILTER_ON(QS_QEP_INIT_TRAN);
	QS_FILTER_ON(QS_QEP_INTERN_TRAN);
	QS_FILTER_ON(QS_QEP_TRAN);
	QS_FILTER_ON(QS_QEP_IGNORED);
	QS_FILTER_ON(QS_QEP_DISPATCH);
	QS_FILTER_ON(QS_QEP_UNHANDLED);

	QS_FILTER_ON(QS_QF_PUBLISH);

	QS_FILTER_ON(USER_DEBUG_PORT);
	QS_FILTER_ON(USER_USART_PORT);

	/* for ISR */
	QS_SIG_DICTIONARY(UART_DATA_BEGIN_SIG, (void *)0);
	QS_SIG_DICTIONARY(BATTERY_REPORT_SIG, (void *)0);

	return (uint8_t)1; /* return success */
}
/*..........................................................................*/
void QS_onCleanup(void) {
}
/*..........................................................................*/
QSTimeCtr QS_onGetTime(void) { /* NOTE: invoked with interrupts DISABLED */
	if ((SysTick->CTRL & 0x00010000) == 0) {  /* COUNT no set? */
		return QS_tickTime_ - (QSTimeCtr)SysTick->VAL;
	}
	else { /* the rollover occured, but the SysTick_ISR did not run yet */
		return QS_tickTime_ + QS_tickPeriod_ - (QSTimeCtr)SysTick->VAL;
	}
}
/*..........................................................................*/
void QS_onFlush(void) {
	uint16_t b;

	QF_INT_DISABLE();
	while ((b = QS_getByte()) != QS_EOD) {    /* while not End-Of-Data... */
		QF_INT_ENABLE();
		while ((USART2->SR & 0x0080U) == 0U) { /* while TXE not empty */
		}
		USART2->DR = (b & 0xFFU);  /* put into the DR register */
		QF_INT_DISABLE();
	}
	QF_INT_ENABLE();
}
#endif /* Q_SPY */

void BSP_RCC_init(void)
{
	//将外设 RCC寄存器重设为缺省值
	RCC_DeInit();

	RCC_HSICmd(ENABLE);
	while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET) {}
	do {

		RCC_HCLKConfig(RCC_SYSCLK_Div1);//选择HCLK时钟源为系统时钟SYYSCLOCK
		RCC_PCLK2Config(RCC_HCLK_Div1);//APB2时钟为8M
		RCC_PCLK1Config(RCC_HCLK_Div2);//APB1时钟为4M

		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
		FLASH_SetLatency(FLASH_Latency_2);
		//设置 PLL 时钟源及倍频系数
		RCC_PLLConfig(RCC_PLLSource_HSI_Div2, RCC_PLLMul_16);   //8/2*16 = 64
																//使能或者失能 PLL,这个参数可以取：ENABLE或者DISABLE
		RCC_PLLCmd(ENABLE);//如果PLL被用于系统时钟,那么它不能被失能
						   //等待指定的 RCC 标志位设置成功 等待PLL初始化成功
		while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {}

		//设置系统时钟（SYSCLK） 设置PLL为系统时钟源
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
		//等待PLL成功用作于系统时钟的时钟源
		// 0x00：HSI 作为系统时钟
		// 0x04：HSE作为系统时钟
		// 0x08：PLL作为系统时钟
		while (RCC_GetSYSCLKSource() != 0x08) {
		}
	} while (0);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
		RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOF |
		RCC_APB2Periph_GPIOG, ENABLE);

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
}

void BSP_NVIC_init(void)
{
	/* set priorities of ALL ISRs used in the system, see NOTE00
	*
	* !!!!!!!!!!!!!!!!!!!!!!!!!!!! CAUTION !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	* Assign a priority to EVERY ISR explicitly by calling NVIC_SetPriority().
	* DO NOT LEAVE THE ISR PRIORITIES AT THE DEFAULT VALUE!
	*/

	NVIC_SetPriorityGrouping(3); // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	NVIC_SetPriority(USART1_IRQn, USART1_PRIO);
	NVIC_SetPriority(USART3_IRQn, USART3_PRIO);
	NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIO);
	/* ... */

	/* enable IRQs... */
	NVIC_EnableIRQ(USART1_IRQn);
	NVIC_EnableIRQ(USART3_IRQn);
}

void BSP_l206_gpio_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = POWER_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(POWER_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PWRKEY_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(PWRKEY_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = L206_SLEEP_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(PWRKEY_PORT, &GPIO_InitStructure);
}

void _debug_print(char *fmt, ...)
{
	char buf[300];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	QS_BEGIN(USER_DEBUG_PORT, &l_debug_port); /* application-specific record begin */
	QS_STR(buf);							/* debug info */
	QS_END()
}
