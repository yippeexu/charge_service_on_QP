#ifndef __CHARGER_H__
#define __CHARGER_H__


#define MEDIUM_BUFFER_MAX_LENGTH 400U
#define LARGE_BUFFER_MAX_LENGTH	 1536U

enum CHARGERsignals {
	BATTERY_OK_SIG = Q_USER_SIG,
	BATTERY_FAULT_SIG,
	CHARGE_UPDATE_SIG,
	VOL_PERCENT_UPDATE_SIG,
	MAX_PUB_SIG,

	UART_DATA_BEGIN_SIG,		// posted directly to MODULE_RX from 2G module ISR.
	POLL_UART_DATA_SIG,			// used by MODULE_RX for poll time events.
	UART_DATA_READY_SIG,		// posted directly to MODULE from MODULE_RX.
	DELAY_TIMEOUT_SIG,			// used by MODULE for delay time events.
	RECV_DATA_TIMEOUT_SIG,		// used by MODULE for receive data time events.
	CTRL_OUTPUT_SIG,			// posted directly to BATTERY from MODULE.
	NETWORK_ERROR_SIG,			// posted directly to DISPLAY from MODULE.
	MODULE_RESTART_SIG,			// used by MODULE for restart 2G module.
	MQTT_STATUS_CHECK_SIG,		// used by MODULE for check MQTT status time events.
	OPEN_SERVICE_START_SIG,		// posted directly to DISPLAY from MODULE.
	BATTERY_STATUS_SIG,			// posted directly to BATTERY from battery ISR.
	BATTERY_CHECK_SIG,			// used by BATTERY for check battery time events.
	BATTERY_OPEN_TIMEOUT_SIG,	// used by BATTERY for check open time events.
	OUTPUT_UPDATE_SIG,			// posted directly to MODULE from BATTERY.
	CHARGE_FULL_UPDATE_SIG,		// posted directly to DISPLAY from BATTERY.
	OPEN_SERVICE_FINISHED_SIG,	// posted directly to MODULE from DISPLAY.
	MAX_SIG
};


typedef struct {
	QEvt super;

	char data[MEDIUM_BUFFER_MAX_LENGTH];
	uint16_t len;
}UartDataEvt;

typedef struct {
	QEvt super;

	char data[LARGE_BUFFER_MAX_LENGTH];
	uint16_t len;
}largeUartDataEvt;

void Module_ctor(void);
extern QActive * const AO_Module;

void Module_rx_ctor(void);
extern QActive * const AO_Module_rx;

#endif
