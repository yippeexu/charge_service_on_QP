#ifndef __CHARGER_H__
#define __CHARGER_H__


#define SMALL_BUFFER_MAX_LENGTH 100

enum CHARGERsignals {
	MAX_PUB_SIG = Q_USER_SIG,

	POLL_TIMEOUT_SIG,		// used by MODULE_RX for poll time events.
	UART_DATA_BEGIN_SIG,	// posted directly to MODULE_RX from 2G module ISR.
	UART_DATA_READY_SIG,	// posted directly to MODULE from MODULE_RX.
	DELAY_TIMEOUT_SIG,		// used by MODULE for delay time events.
	RECV_DATA_TIMEOUT_SIG,		// used by MODULE for receive data time events.
	MAX_SIG
};

typedef struct {
	QEvt super;

	char data[SMALL_BUFFER_MAX_LENGTH];
	uint16_t len;
}UartDataEvt;


void Module_ctor(void);
extern QActive * const AO_Module;

void Module_rx_ctor(void);
extern QActive * const AO_Module_rx;

#endif
