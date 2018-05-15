#ifndef __CHARGER_H__
#define __CHARGER_H__


#define MEDIUM_BUFFER_MAX_LENGTH 400U
#define LARGE_BUFFER_MAX_LENGTH	 1536U

enum CHARGERsignals {
	BATTERY_OK_SIG = Q_USER_SIG,
	BATTERY_FAULT_SIG,
	CHARGE_ON_SIG,
	CHARGE_OFF_SIG,
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
	MQTT_STATUS_ERROR_SIG,		// used by MODULE for notify MQTT status error.
	MQTT_CONNECTED_SIG,			// posted directly to DISPLAY from MODULE.
	FIRST_REPORT_SIG,			// used by MODULE for first report.
	OPEN_SERVICE_START_SIG,		// posted directly to DISPLAY from MODULE.
	BATTERY_REPORT_SIG,			// posted directly to BATTERY from battery ISR.
	BATTERY_HEARTBEAT_SIG,		// posted directly to BATTERY from battery Heartbeat.
	BATTERY_CHECK_TIMEOUT_SIG,	// used by BATTERY for check battery time events.
	BATTERY_OPEN_TIMEOUT_SIG,	// used by BATTERY for check open time events.
	OUTPUT_OPEN_SIG,			// posted directly to MODULE from BATTERY.
	OUTPUT_CLOSE_SIG,			// posted directly to MODULE from BATTERY.
	OUTPUT_UPDATE_SIG,			// used by BATTERY for notify itself.
	CHARGE_FULL_SIG,			// posted directly to DISPLAY from BATTERY.
	CHARGE_NOT_FULL_SIG,		// posted directly to DISPLAY from BATTERY.
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

typedef struct report_frame_s {
	uint8_t bit_vol_value_h : 5; // 低位在前，高位在后
	uint8_t bit_onloading : 1;
	uint8_t bit_charge_full : 1;
	uint8_t bit_oncharging : 1;
	uint8_t bit_vol_value_l : 8;
	uint8_t bit_vol_percent : 7;
	uint8_t bit_outputing : 1;
}report_frame_t;

typedef union {
	report_frame_t struct_;
	uint8_t buf[3];
}report_frame_u;

typedef struct {
	QEvt super;

	report_frame_u frame;
}batteryReportEvt;

typedef struct {
	QEvt super;

	uint32_t v;
}normalEvt;

void Module_ctor(void);
extern QActive * const AO_Module;

void Module_rx_ctor(void);
extern QActive * const AO_Module_rx;

void Battery_ctor(void);
extern QActive * const AO_Battery;

#endif
