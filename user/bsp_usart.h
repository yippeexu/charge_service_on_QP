#ifndef __BSP_USART_H__
#define __BSP_USART_H__


void BSP_usart1_init(void);
void BSP_usart2_init(void);
void BSP_usart1_tx_buffer(uint8_t *buf, uint16_t len);

#endif
