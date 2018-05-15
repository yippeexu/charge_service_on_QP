#include "qpc.h"
#include "charger.h"
#include "bsp.h"

#include <system.h>

Q_DEFINE_THIS_MODULE("module")

/* 超时处理：重试，当超过次数后重启模块。 */
#define TMOUT_PROC_TRY_AGAIN(me_, state_, times_)	do { \
											if ((me_)->was_armed == 0 && (me_)->timeout_count == 0) { \
												(me_)->was_armed = 1; \
												status_ = Q_HANDLED(); \
											} \
											else if (++(me_)->timeout_count < (times_)) { \
												status_ = Q_TRAN((state_)); \
											} \
											else { \
												QACTIVE_POST(&(me_)->super, Q_NEW(QEvt, MODULE_RESTART_SIG), (me_)); \
												status_ = Q_HANDLED(); \
											} \
										}while(0)

/* 超时处理：不重试而进入下一个状态。设置 was_armed=2 的目的是超时后was_armed必为0，*
 *         对下一个状态造成影响。该宏与EXIT_GOTO_NEXT_STATE()搭配使用。 */
#define TMOUT_PROC_GOTO_NEXT_STATE(me_, state_)	do { \
											debug_print("[TIMEOUT]was_armed = %d", (me_)->was_armed); \
											if ((me_)->was_armed == 0) { \
												(me_)->was_armed = 1; \
												status_ = Q_HANDLED(); \
											} \
											else { \
												(me_)->was_armed = 2; \
												status_ = Q_TRAN((state_)); \
											} \
										}while(0)

 /* 超时处理：直接重启模块。 */
#define TMOUT_PROC_MODULE_RESTART(me_)	do { \
								if ((me_)->was_armed == 0) { \
									(me_)->was_armed = 1; \
									status_ = Q_HANDLED(); \
								} \
								else { \
									QACTIVE_POST(&(me_)->super, Q_NEW(QEvt, MODULE_RESTART_SIG), (me_)); \
									status_ = Q_HANDLED(); \
								} \
							}while(0)

/* 在状态EXIT时使用。该宏与 TMOUT_PROC_GOTO_NEXT_STATE() 搭配使用 */
#define EXIT_GOTO_NEXT_STATE(me_, timeEvt_)	do { \
								debug_print("[EXIT]was_armed = %d", (me_)->was_armed); \
								if ((me_)->was_armed == 2) { \
									QTimeEvt_disarm((timeEvt_)); \
									(me_)->was_armed = 1; \
								} \
								else { \
									(me_)->was_armed = QTimeEvt_disarm((timeEvt_)); \
								} \
								}while(0)

/* 超时处理。参考TMOUT_PROC_GOTO_NEXT_STATE(), 增加了额外处理,用于。 */
#define TMOUT_PROC_GOTO_NEXT_STATE_EXT1(me_, state_) do { \
											debug_print("[TIMEOUT]was_armed = %d", (me_)->was_armed); \
											if ((me_)->was_armed == 0) { \
												(me_)->was_armed = 1; \
												status_ = Q_HANDLED(); \
											} \
											else { \
												(me_)->was_armed = 2; \
												QACTIVE_POST(&(me_)->super, Q_NEW(QEvt, MQTT_STATUS_ERROR_SIG), (me_)); \
												status_ = Q_TRAN((state_)); \
											} \
										}while(0)
/* Module所关心的battery status */
typedef struct {
	uint8_t oncharging;
	uint8_t onoutputing;
	uint8_t vol_percent;
}battery_status_care_t;

typedef struct {
	QActive super;

	QTimeEvt delayTimeEvt;
	QTimeEvt recvDataTimeEvt;
	QTimeEvt mqttStatusTimeEvt;
	QEQueue deferredEvtQueue; /* native QF queue for deferred request events */
	QEvt const *deferredQSto[5]; /* storage for deferred queue buffer */
	uint8_t timeout_count;
	uint32_t mqtt_conn_timeout_count;
	uint8_t mqtt_status_error_count;
	uint8_t power_on_step;
	uint8_t error_code;
	uint8_t was_armed;
	uint8_t l206_ver_compatible;
	uint8_t csq;
	char latitude[15];
	char longitude[15];
	sys_info_t info;	// 业务关键参数
	battery_status_care_t bat;
}Module;

enum STEP {
	STEP_1 = 1,
	STEP_2,
	STEP_3,
	STEP_4,
	STEP_5
};


/* protected: */
static QState Module_initial(Module * const me, QEvt const * const e);
static QState Module_on(Module * const me, QEvt const * const e);
static QState Module_power(Module * const me, QEvt const * const e);
static QState Module_config(Module * const me, QEvt const * const e);
static QState Module_hands(Module * const me, QEvt const * const e);
static QState Module_set_baudrate(Module * const me, QEvt const * const e);
static QState Module_close_echo(Module * const me, QEvt const * const e);
static QState Module_check_model(Module * const me, QEvt const * const e);
static QState Module_check_fm_version(Module * const me, QEvt const * const e);
static QState Module_get_sn(Module * const me, QEvt const * const e);
static QState Module_get_imei(Module * const me, QEvt const * const e);
static QState Module_check_sim_pin(Module * const me, QEvt const * const e);
static QState Module_network_setup(Module * const me, QEvt const * const e);
static QState Module_get_csq(Module * const me, QEvt const * const e);
static QState Module_check_cgreg(Module * const me, QEvt const * const e);
static QState Module_check_cgatt(Module * const me, QEvt const * const e);
static QState Module_set_apn(Module * const me, QEvt const * const e);
static QState Module_active_pdp(Module * const me, QEvt const * const e);
static QState Module_get_utc_time(Module * const me, QEvt const * const e);
static QState Module_get_location(Module * const me, QEvt const * const e);
static QState Module_get_iccid(Module * const me, QEvt const * const e);
static QState Module_mqtt_connecting(Module * const me, QEvt const * const e);
static QState Module_set_mqtttmout(Module * const me, QEvt const * const e);
static QState Module_set_mqttconfig(Module * const me, QEvt const * const e);
static QState Module_set_mqtt_start(Module * const me, QEvt const * const e);
static QState Module_mqtt_connect(Module * const me, QEvt const * const e);
static QState Module_mqtt_sub(Module * const me, QEvt const * const e);
static QState Module_mqtt_connected(Module * const me, QEvt const * const e);
static QState Module_mqtt_idle(Module * const me, QEvt const * const e);
static QState Module_mqtt_busy(Module * const me, QEvt const * const e);
static QState Module_mqtt_wait_ack(Module * const me, QEvt const * const e);
static QState Module_check_mqtt_status(Module * const me, QEvt const * const e);
static QState Module_mqtt_sleep_long(Module * const me, QEvt const * const e);
static QState Module_mqtt_sleep_short(Module * const me, QEvt const * const e);
static QState Module_mqtt_disconnect(Module * const me, QEvt const * const e);
static QState Module_device_active(Module * const me, QEvt const * const e);
static QState Module_set_httpssl(Module * const me, QEvt const * const e);
static QState Module_set_http_alive(Module * const me, QEvt const * const e);
static QState Module_set_http_url(Module * const me, QEvt const * const e);
static QState Module_set_http_port(Module * const me, QEvt const * const e);
static QState Module_http_setup(Module * const me, QEvt const * const e);
static QState Module_http_action(Module * const me, QEvt const * const e);


/* Local objects -----------------------------------------------------------*/
static Module l_module;

/* Global objects ----------------------------------------------------------*/
QActive * const AO_Module = &l_module.super; /* "opaque" pointers to Module AO */

void Module_ctor(void)
{
	Module *me = &l_module;

	QActive_ctor(&me->super, Q_STATE_CAST(&Module_initial));
	QTimeEvt_ctorX(&me->delayTimeEvt, &me->super, DELAY_TIMEOUT_SIG, 0);
	QTimeEvt_ctorX(&me->recvDataTimeEvt, &me->super, RECV_DATA_TIMEOUT_SIG, 0);
	QTimeEvt_ctorX(&me->mqttStatusTimeEvt, &me->super, MQTT_STATUS_CHECK_SIG, 0);
	QEQueue_init(&me->deferredEvtQueue, me->deferredQSto, Q_DIM(me->deferredQSto));	
}

static QState Module_initial(Module * const me, QEvt const * const e)
{
	me->mqtt_conn_timeout_count = 0;

	L206_SLEEP_PRESS(); // 始终处于非睡眠模式
	L206_POWER_ON();	// 2G模块上电

	init_sys_info(&me->info);
	debug_print("seckey:%s", me->info.sec_key);
	debug_print("iccid:%s", me->info.iccid);

	//debug_print("0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
	//strncpy(me->info.iccid, "898607b9101700284575", strlen("898607b9101700284575"));
	//strncpy(me->info.serial_num, "P4H90305042327", strlen("P4H90305042327"));
	//strncpy(me->info.device_id, "g100000866131034806659", strlen("g100000866131034806659"));

	//char at_httppara_url_buf[HTTP_BUFFER_LEN];
	//memset(at_httppara_url_buf, 0, sizeof(at_httppara_url_buf));
	//http_set_url_request(at_httppara_url_buf, &me->info, "active\0", NULL, 0);
	//debug_print("[url]:%s", at_httppara_url_buf);

	QActive_subscribe((QActive *)me, BATTERY_OK_SIG);
	QActive_subscribe((QActive *)me, BATTERY_FAULT_SIG);
	QActive_subscribe((QActive *)me, CHARGE_ON_SIG);
	QActive_subscribe((QActive *)me, CHARGE_OFF_SIG);
	QActive_subscribe((QActive *)me, VOL_PERCENT_UPDATE_SIG);

	QS_OBJ_DICTIONARY(&l_module);
	QS_OBJ_DICTIONARY(&l_module.delayTimeEvt);
	QS_OBJ_DICTIONARY(&l_module.recvDataTimeEvt);

	QS_FUN_DICTIONARY(&Module_initial);
	QS_FUN_DICTIONARY(&Module_on);
	QS_FUN_DICTIONARY(&Module_power);
	QS_FUN_DICTIONARY(&Module_config);
	QS_FUN_DICTIONARY(&Module_hands);
	QS_FUN_DICTIONARY(&Module_set_baudrate);
	QS_FUN_DICTIONARY(&Module_close_echo);
	QS_FUN_DICTIONARY(&Module_check_model);
	QS_FUN_DICTIONARY(&Module_check_fm_version);
	QS_FUN_DICTIONARY(&Module_get_sn);
	QS_FUN_DICTIONARY(&Module_get_imei);
	QS_FUN_DICTIONARY(&Module_check_sim_pin);
	QS_FUN_DICTIONARY(&Module_network_setup);
	QS_FUN_DICTIONARY(&Module_get_csq);
	QS_FUN_DICTIONARY(&Module_check_cgreg);
	QS_FUN_DICTIONARY(&Module_check_cgatt);
	QS_FUN_DICTIONARY(&Module_set_apn);
	QS_FUN_DICTIONARY(&Module_active_pdp);
	QS_FUN_DICTIONARY(&Module_get_utc_time);
	QS_FUN_DICTIONARY(&Module_get_location);
	QS_FUN_DICTIONARY(&Module_get_iccid);
	QS_FUN_DICTIONARY(&Module_mqtt_connecting);
	QS_FUN_DICTIONARY(&Module_set_mqtttmout);
	QS_FUN_DICTIONARY(&Module_set_mqttconfig);
	QS_FUN_DICTIONARY(&Module_set_mqtt_start);
	QS_FUN_DICTIONARY(&Module_mqtt_connect);
	QS_FUN_DICTIONARY(&Module_mqtt_sub);
	QS_FUN_DICTIONARY(&Module_mqtt_connected);
	QS_FUN_DICTIONARY(&Module_mqtt_idle);
	QS_FUN_DICTIONARY(&Module_mqtt_busy);
	QS_FUN_DICTIONARY(&Module_mqtt_wait_ack);
	QS_FUN_DICTIONARY(&Module_check_mqtt_status);
	QS_FUN_DICTIONARY(&Module_mqtt_sleep_long);
	QS_FUN_DICTIONARY(&Module_mqtt_sleep_short);
	QS_FUN_DICTIONARY(&Module_mqtt_disconnect);
	QS_FUN_DICTIONARY(&Module_device_active);
	QS_FUN_DICTIONARY(&Module_set_httpssl);
	QS_FUN_DICTIONARY(&Module_set_http_alive);
	QS_FUN_DICTIONARY(&Module_set_http_url);
	QS_FUN_DICTIONARY(&Module_set_http_port);
	QS_FUN_DICTIONARY(&Module_http_setup);
	QS_FUN_DICTIONARY(&Module_http_action);

	QS_SIG_DICTIONARY(DELAY_TIMEOUT_SIG, me);
	QS_SIG_DICTIONARY(RECV_DATA_TIMEOUT_SIG, me);
	QS_SIG_DICTIONARY(CTRL_OUTPUT_SIG, me);
	QS_SIG_DICTIONARY(NETWORK_ERROR_SIG, me);
	QS_SIG_DICTIONARY(MODULE_RESTART_SIG, me);
	QS_SIG_DICTIONARY(MQTT_STATUS_CHECK_SIG, me);
	QS_SIG_DICTIONARY(OPEN_SERVICE_START_SIG, me);
	QS_SIG_DICTIONARY(FIRST_REPORT_SIG, me);
	QS_SIG_DICTIONARY(MQTT_STATUS_ERROR_SIG, me);


	return Q_TRAN(&Module_power);
}
static QState Module_on(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_INIT_SIG: {
		status_ = Q_TRAN(&Module_power);
		break;
	}
	case CHARGE_ON_SIG: {
		me->bat.oncharging = 1;
		status_ = Q_HANDLED();
		break;
	}
	case CHARGE_OFF_SIG: {
		me->bat.oncharging = 0;
		status_ = Q_HANDLED();
		break;
	}
	case OUTPUT_OPEN_SIG: {
		me->bat.onoutputing = 1;
		status_ = Q_HANDLED();
		break;
	}
	case OUTPUT_CLOSE_SIG: {
		me->bat.onoutputing = 0;
		status_ = Q_HANDLED();
		break;
	}
	case VOL_PERCENT_UPDATE_SIG: {
		me->bat.vol_percent = Q_EVT_CAST(normalEvt)->v;
		status_ = Q_HANDLED();
		break;
	}
	default: {
		status_ = Q_SUPER(&QHsm_top);
		break;
	}
	}
	return status_;
}
static QState Module_power(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->delayTimeEvt, BSP_TICKS_PER_SEC * 1, 0U);
		me->power_on_step = STEP_1;
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		QTimeEvt_disarm(&me->delayTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case DELAY_TIMEOUT_SIG: {
		debug_print("power_on_step = %d", me->power_on_step);
		switch (me->power_on_step)
		{
		case STEP_1: {
			L206_PWRKEY_LOW();
			QTimeEvt_armX(&me->delayTimeEvt, BSP_TICKS_PER_SEC, 0U);
			me->power_on_step++;
			status_ = Q_HANDLED();
			break;
		}
		case STEP_2: {
			L206_PWRKEY_HIGH();
			QTimeEvt_armX(&me->delayTimeEvt, BSP_TICKS_PER_SEC, 0U);
			me->power_on_step++;
			status_ = Q_HANDLED();
			break;
		}
		case STEP_3: {
			L206_PWRKEY_LOW();
			QTimeEvt_armX(&me->delayTimeEvt, BSP_TICKS_PER_SEC * 2, 0U);
			me->power_on_step++;
			status_ = Q_HANDLED();
			break;
		}
		case STEP_4: {
			L206_PWRKEY_HIGH();
			QTimeEvt_armX(&me->delayTimeEvt, BSP_TICKS_PER_SEC * 3, 0U);
			me->power_on_step++;
			status_ = Q_HANDLED();
			break;
		}
		case STEP_5: {
			status_ = Q_TRAN(&Module_hands);
			break;
		}
		}
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_config(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		me->error_code = HAND_ERROR;
		me->was_armed = 1;
		me->timeout_count = 0;
		status_ = Q_HANDLED();
		break;
	}
	case Q_INIT_SIG: {
		status_ = Q_TRAN(&Module_hands);
		break;
	}
	case MODULE_RESTART_SIG: {
		// TODO
		//QACTIVE_POST(AO_DISPLAY, NETWORK_ERROR_SIG, error_code);
		status_ = Q_TRAN(&Module_power);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_hands(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 1, 0U);
		BSP_usart1_tx_buffer(L206_HANDCMD, sizeof(L206_HANDCMD));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "AT");
		if (p) {
			me->timeout_count = 0;
			status_ = Q_TRAN(&Module_set_baudrate);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {		
		//debug_print("was_armed = %d, timeout_count = %d", me->was_armed, me->timeout_count);
		TMOUT_PROC_TRY_AGAIN(me, &Module_hands, 10);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_config);
		break;
	}
	}
	return status_;
}
static QState Module_set_baudrate(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 2, 0U);
		BSP_usart1_tx_buffer(L206_BAUDRATE_115200, sizeof(L206_BAUDRATE_115200));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			me->timeout_count = 0;
			status_ = Q_TRAN(&Module_close_echo);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_set_baudrate, 3);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_config);
		break;
	}
	}
	return status_;
}
static QState Module_close_echo(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 3, 0U);
		BSP_usart1_tx_buffer(L206_CLOSEATE, sizeof(L206_CLOSEATE));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "ATE0");
		if (p) {
			me->timeout_count = 0;
			status_ = Q_TRAN(&Module_check_model);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_close_echo, 3);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_config);
		break;
	}
	}
	return status_;
}
static QState Module_check_model(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 4, 0U);
		BSP_usart1_tx_buffer(L206_MODULE_MODEL, sizeof(L206_MODULE_MODEL));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "L206\r\n\r\nOK");
		if (p) {
			me->timeout_count = 0;
			status_ = Q_TRAN(&Module_check_fm_version);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_check_model, 3);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_config);
		break;
	}
	}
	return status_;
}
static QState Module_check_fm_version(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 2, 0U);
		BSP_usart1_tx_buffer(L206_ISLKVRSCAN, sizeof(L206_ISLKVRSCAN));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+ISLKVRSCAN: ");
		if (p) {
			p += strlen("+ISLKVRSCAN: ");
			int ret = is_string(p, 16);
			if (ret == 0) {
				if (strstr(p, "L206v01.01b14.05")) {
					me->l206_ver_compatible = 1;
				}
				else if (strstr(p, "L206v01.01b14.00")) {
					me->l206_ver_compatible = 0;
				}
				else {
					me->l206_ver_compatible = 0;
				}

				me->timeout_count = 0;
				status_ = Q_TRAN(&Module_get_sn);
			}
			else {
				status_ = Q_HANDLED();
			}
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_check_fm_version, 3);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_config);
		break;
	}
	}
	return status_;
}
static QState Module_get_sn(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 5, 0U);
		memset(me->info.serial_num, '\0', sizeof(me->info.serial_num));
		BSP_usart1_tx_buffer(L206_EGMR, sizeof(L206_EGMR));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+EGMR: \"");
		if (p) {
			p += strlen("+EGMR: \"");
			int ret = is_string(p, SN_MAX_LENGTH);
			if (ret == 0) {
				strncpy(me->info.serial_num, p, SN_MAX_LENGTH);
				debug_print("serial_num:%s", me->info.serial_num);
				me->timeout_count = 0;
				status_ = Q_TRAN(&Module_get_imei);
			}
			else {
				status_ = Q_HANDLED();
			}
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_get_sn, 5);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_config);
		break;
	}
	}
	return status_;
}
static QState Module_get_imei(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 5, 0U);
		memset(me->info.device_id, '\0', sizeof(me->info.device_id));
		strncpy(me->info.device_id, DEVICEID_HEAD, DEVICEID_HEAD_LENGTH);
		BSP_usart1_tx_buffer(L206_QGSN, sizeof(L206_QGSN));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "\r\n");
		if (p) {
			p += strlen("\r\n");
			int ret = is_string(p, IMEI_MAX_LENGTH);
			if (ret == 0) {
				strncpy(&me->info.device_id[DEVICEID_HEAD_LENGTH], p, IMEI_MAX_LENGTH);
				debug_print("device_id:%s", me->info.device_id);
				me->timeout_count = 0;
				status_ = Q_TRAN(&Module_check_sim_pin);
			}
			else {
				status_ = Q_HANDLED();
			}
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_get_imei, 5);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_config);
		break;
	}
	}
	return status_;
}
static QState Module_check_sim_pin(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 3, 0U);
		BSP_usart1_tx_buffer(L206_FINSIMPINCMD, sizeof(L206_FINSIMPINCMD));
		me->error_code = CARDPIN_ERROR;
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+CPIN: ");
		if (p) {
			p += strlen("+CPIN: ");
			if (strstr(p, "READY") != NULL) {    // PIN码已解除
				me->timeout_count = 0;
				status_ = Q_TRAN(&Module_get_csq);
			}				
			else {
				status_ = Q_HANDLED();
			}
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_check_sim_pin, 8);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_network_setup(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		me->error_code = NETWORK_ERROR;
		me->timeout_count = 0;
		status_ = Q_HANDLED();
		break;
	}
	case Q_INIT_SIG: {
		status_ = Q_TRAN(Module_get_csq);
		break;
	}
	case MODULE_RESTART_SIG: {
		// TODO
		//QACTIVE_POST(AO_DISPLAY, NETWORK_ERROR_SIG, error_code);
		status_ = Q_TRAN(&Module_power);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_get_csq(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC, 0U);
		me->csq = 0;
		BSP_usart1_tx_buffer(L206_CSQ, sizeof(L206_CSQ));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+CSQ: ");
		if (p) {
			p += strlen("+CSQ: ");
			int csq = capture_number(p, strlen(p));
			if(csq > 0 && csq < 32) { // csq取值范围为0-31
				me->csq = (csq * 100 / 32);	// 转化为百分比
				me->timeout_count = 0;
				status_ = Q_TRAN(&Module_check_cgreg);
			}
			else {
				status_ = Q_HANDLED();
			}
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_get_csq, 20);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_network_setup);
		break;
	}
	}
	return status_;
}
static QState Module_check_cgreg(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 1, 0U);
		BSP_usart1_tx_buffer(L206_CGREG, sizeof(L206_CGREG));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+CGREG: ");
		if (p) {
			p += strlen("+CGREG: ");
			if (strstr(p, "0,1") != NULL || strstr(p, "0,5") != NULL) {    // 基站注册成功
				me->timeout_count = 0;
				status_ = Q_TRAN(&Module_check_cgatt);
			}
			else {
				status_ = Q_HANDLED();
			}
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_check_cgreg, 40);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_network_setup);
		break;
	}
	}
	return status_;
}
static QState Module_check_cgatt(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 2, 0U);
		BSP_usart1_tx_buffer(L206_CGATT, sizeof(L206_CGATT));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+CGATT: 1"); // GPRS附着成功
		if (p) {
			me->timeout_count = 0;
			status_ = Q_TRAN(&Module_set_apn);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_check_cgatt, 40);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_network_setup);
		break;
	}
	}
	return status_;
}
static QState Module_set_apn(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 5, 0U);
		BSP_usart1_tx_buffer(L206_CSTT, sizeof(L206_CSTT));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			me->timeout_count = 0;
			status_ = Q_TRAN(&Module_active_pdp);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_set_apn, 3);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_network_setup);
		break;
	}
	}
	return status_;
}
static QState Module_active_pdp(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 2, 0U);
		BSP_usart1_tx_buffer(L206_CIICR, sizeof(L206_CIICR));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			me->timeout_count = 0;
			status_ = Q_TRAN(&Module_get_utc_time);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_active_pdp, 30);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_network_setup);
		break;
	}
	}
	return status_;
}
static QState Module_get_utc_time(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 2, 0U);
		BSP_usart1_tx_buffer(L206_CCLK, sizeof(L206_CCLK));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		EXIT_GOTO_NEXT_STATE(me, &me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+CCLK: \"");
		if (p) {
			p += strlen("+CCLK: \"");
			parse_utc_time(p, strlen(p));
			debug_print("timestamp = %lld", get_timestamp());
			status_ = Q_TRAN(&Module_get_location);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_GOTO_NEXT_STATE(me, &Module_get_location);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_get_location(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 4, 0U);
		BSP_usart1_tx_buffer(L206_GTPOS, sizeof(L206_GTPOS));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		EXIT_GOTO_NEXT_STATE(me, &me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+GTPOS: ");
		if (p) { // 第二次获取最好延时时间间隔大于2分钟  返回-2 访问基站地经纬度太频繁
			p += strlen("+GTPOS: ");
			parse_longi_lati(p, strlen(p), me->longitude, me->latitude);
			debug_print("longi: %s, lati: %s", me->longitude, me->latitude);
			status_ = Q_TRAN(&Module_get_iccid);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_GOTO_NEXT_STATE(me, &Module_get_iccid);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_get_iccid(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 3, 0U);
		me->error_code = NETWORK_ERROR;
		BSP_usart1_tx_buffer(L206_ICCID, sizeof(L206_ICCID));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+ICCID: ");
		if (p) {
			p += strlen("+ICCID: ");
			int ret = is_string(p, ICCID_MAX_LENGTH);
			if (ret == 0) {
				me->timeout_count = 0;
				// TODO : 需要初始化me->info.iccid赋值
				debug_print("[old]iccid = %s", me->info.iccid);
				if (strncmp(me->info.iccid, p, ICCID_MAX_LENGTH) == 0) {
					status_ = Q_TRAN(&Module_mqtt_connecting);
				}
				else {
					strncpy(me->info.iccid, p, ICCID_MAX_LENGTH);
					debug_print("[new]iccid = %s", me->info.iccid);
					status_ = Q_TRAN(&Module_device_active);
				}			
			}
			else {
				status_ = Q_HANDLED();
			}
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_TRY_AGAIN(me, &Module_get_iccid, 5);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_connecting(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		me->error_code = MQTT_CONN_ERROR;
		status_ = Q_HANDLED();
		break;
	}
	case Q_INIT_SIG: {
		if (me->l206_ver_compatible == 1) { // 新版本固件才有AT+MQTTTMOUT指令
			status_ = Q_TRAN(Module_set_mqtttmout);
		}
		else
		{ 
			status_ = Q_TRAN(Module_set_mqttconfig);
		}
		break;
	}
	case MODULE_RESTART_SIG: {
		// TODO 
		//QACTIVE_POST(AO_DISPLAY, NETWORK_ERROR_SIG, error_code);
		me->mqtt_conn_timeout_count++;
		debug_print("mqtt_conn_timeout_count = %d", me->mqtt_conn_timeout_count);
		if (me->mqtt_conn_timeout_count % 9 == 0) {
			status_ = Q_TRAN(&Module_mqtt_sleep_long);
		}
		else if (me->mqtt_conn_timeout_count % 3 == 0) {
			status_ = Q_TRAN(&Module_mqtt_sleep_short);
		}
		else {
			status_ = Q_TRAN(&Module_mqtt_disconnect);
		}
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_set_mqtttmout(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 2, 0U);
		BSP_usart1_tx_buffer(L206_MQTTTMOUT, sizeof(L206_MQTTTMOUT));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			status_ = Q_TRAN(&Module_set_mqttconfig);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_connecting);
		break;
	}
	}
	return status_;
}
static QState Module_set_mqttconfig(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		char at_mconfig_buf[100];
		memset(at_mconfig_buf, 0, sizeof(at_mconfig_buf));
		mqtt_set_config(at_mconfig_buf, me->info.device_id, me->info.serial_num, me->info.iccid);
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 5, 0U);
		BSP_usart1_tx_buffer((uint8_t *)at_mconfig_buf, strlen(at_mconfig_buf));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			status_ = Q_TRAN(&Module_set_mqtt_start);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_connecting);
		break;
	}
	}
	return status_;
}
static QState Module_set_mqtt_start(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 10, 0U);
		BSP_usart1_tx_buffer(L206_MIPSTART, sizeof(L206_MIPSTART));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "CONNECT OK");
		if (p) {
			status_ = Q_TRAN(&Module_mqtt_connect);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_connecting);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_connect(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 15, 0U);
		BSP_usart1_tx_buffer(L206_MCONNECT, sizeof(L206_MCONNECT));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "CONNACK OK");
		if (p) {
			status_ = Q_TRAN(&Module_mqtt_sub);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_connecting);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_sub(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		char at_msub_buf[100];
		memset(at_msub_buf, 0, sizeof(at_msub_buf));
		mqtt_set_sub(at_msub_buf, me->info.device_id);
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 10, 0U);
		BSP_usart1_tx_buffer((uint8_t *)at_msub_buf, strlen(at_msub_buf));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "SUBACK");
		if (p) {
			status_ = Q_TRAN(&Module_mqtt_connected);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_connecting);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_connected(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->mqttStatusTimeEvt, BSP_TICKS_PER_SEC * 60 * 3, BSP_TICKS_PER_SEC * 60 * 3);
		// TODO
		//QACTIVE_POST(AO_DISPLAY, Q_NEW(QEvt， MQTT_CONNECTED_SIG)， me);
		QACTIVE_POST(&me->super, Q_NEW(QEvt, FIRST_REPORT_SIG), me);
		me->mqtt_status_error_count = 0;
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		QTimeEvt_disarm(&me->mqttStatusTimeEvt);
		QActive_flushDeferred(&me->super, &me->deferredEvtQueue);
		status_ = Q_HANDLED();
		break;
	}
	case Q_INIT_SIG: {
		status_ = Q_TRAN(&Module_mqtt_idle);
		break;
	}
	case MQTT_STATUS_ERROR_SIG: {
		if (++me->mqtt_status_error_count >= 2) {
			QACTIVE_POST(&me->super, Q_NEW(QEvt, MODULE_RESTART_SIG), me);
		}
		status_ = Q_HANDLED();
		break;
	}
	case MODULE_RESTART_SIG: {
		//QACTIVE_POST(AO_DISPLAY, Q_NEW(QEvt， MQTT_DISCONNECTED_SIG)， me);
		status_ = Q_TRAN(&Module_mqtt_disconnect);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_idle(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		/* recall the oldest deferred event... */
		if (QActive_recall(&me->super, &me->deferredEvtQueue)) {
			debug_print("Event recalled");
		}
		else {
			debug_print("No deferred event");
		}
		status_ = Q_HANDLED();
		break;
	}
	case FIRST_REPORT_SIG: {
		// TODO
		status_ = Q_TRAN(&Module_mqtt_wait_ack);
		break;
	}
	case MQTT_STATUS_CHECK_SIG: {

		status_ = Q_TRAN(&Module_check_mqtt_status);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_connected);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_busy(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		status_ = Q_HANDLED();
		break;
	}
	case FIRST_REPORT_SIG: {
		/* defer the new request event... */
		if (QActive_defer(&me->super, &me->deferredEvtQueue, e)) {
			debug_print("Event #%d deferred", e->sig);
		}
		else {
			/* notify the request sender that his request was denied... */
			debug_print("Event #%d IGNORED, can't be deferred", e->sig);
		}

		status_ = Q_HANDLED();
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_connected);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_wait_ack(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 10, 0U);
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		EXIT_GOTO_NEXT_STATE(me, &me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "PUBACK");
		if (p) {
			status_ = Q_TRAN(&Module_mqtt_idle);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_GOTO_NEXT_STATE(me, &Module_mqtt_idle);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_busy);
		break;
	}
	}
	return status_;
}
static QState Module_check_mqtt_status(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 2, 0U);
		if (me->l206_ver_compatible == 1) {
			BSP_usart1_tx_buffer(L206_MQTTSTATU, sizeof(L206_MQTTSTATU));
		}
		else {
			BSP_usart1_tx_buffer(L206_MIPSTART, sizeof(L206_MIPSTART));
		}
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		EXIT_GOTO_NEXT_STATE(me, &me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = NULL;
		if (me->l206_ver_compatible == 1) {
			p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+MQTTSTATU:1");
		}
		else {
			p = strstr(Q_EVT_CAST(UartDataEvt)->data, "ALREADY CONNECT");
		}

		if (p) {
			me->mqtt_status_error_count = 0;
			status_ = Q_TRAN(&Module_mqtt_idle);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_GOTO_NEXT_STATE_EXT1(me, &Module_mqtt_idle);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_mqtt_busy);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_sleep_long(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		/* 延时1 hour */
		QTimeEvt_armX(&me->delayTimeEvt, BSP_TICKS_PER_SEC * 60 * 60, 0U);
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		QTimeEvt_disarm(&me->delayTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case DELAY_TIMEOUT_SIG: {
		status_ = Q_TRAN(&Module_mqtt_disconnect);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_sleep_short(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		/* 延时10 mins */
		QTimeEvt_armX(&me->delayTimeEvt, BSP_TICKS_PER_SEC * 60 * 10, 0U);
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		QTimeEvt_disarm(&me->delayTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case DELAY_TIMEOUT_SIG: {
		status_ = Q_TRAN(&Module_mqtt_disconnect);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_mqtt_disconnect(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QACTIVE_POST(&me->super, Q_NEW(QEvt, MODULE_RESTART_SIG), me);
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		status_ = Q_HANDLED();
		break;
	}
	case MODULE_RESTART_SIG: {
		status_ = Q_TRAN(&Module_power);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_device_active(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		me->error_code = HTTP_CONN_ERROR;
		status_ = Q_HANDLED();
		break;
	}
	case Q_INIT_SIG: {
		status_ = Q_TRAN(Module_set_httpssl);
		break;
	}
	case MODULE_RESTART_SIG: {
		// TODO
		//QACTIVE_POST(AO_DISPLAY, NETWORK_ERROR_SIG, error_code);
		status_ = Q_TRAN(&Module_power);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_on);
		break;
	}
	}
	return status_;
}
static QState Module_set_httpssl(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 10, 0U);
		BSP_usart1_tx_buffer(L206_HTTPSSL_1, sizeof(L206_HTTPSSL_1));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			status_ = Q_TRAN(&Module_set_http_alive);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_device_active);
		break;
	}
	}
	return status_;
}
static QState Module_set_http_alive(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 10, 0U);
		BSP_usart1_tx_buffer(L206_HTTPPARA_CONNECT, sizeof(L206_HTTPPARA_CONNECT));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			status_ = Q_TRAN(&Module_set_http_url);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_device_active);
		break;
	}
	}
	return status_;
}
static QState Module_set_http_url(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		char at_httppara_url_buf[HTTP_BUFFER_LEN];
		memset(at_httppara_url_buf, 0, sizeof(at_httppara_url_buf));
		http_set_url_request(at_httppara_url_buf, &me->info, "active\0", NULL, 0);
		//debug_print("http url:%s", at_httppara_url_buf);
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 10, 0U);
		BSP_usart1_tx_buffer((uint8_t *)at_httppara_url_buf, strlen(at_httppara_url_buf));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			status_ = Q_TRAN(&Module_set_http_port);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_device_active);
		break;
	}
	}
	return status_;
}
static QState Module_set_http_port(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 10, 0U);
		BSP_usart1_tx_buffer(L206_HTTPPARA_PORT_443, sizeof(L206_HTTPPARA_PORT_443));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			status_ = Q_TRAN(&Module_http_setup);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_device_active);
		break;
	}
	}
	return status_;
}
static QState Module_http_setup(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 30, 0U);
		BSP_usart1_tx_buffer(L206_HTTPSETUP, sizeof(L206_HTTPSETUP));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "OK");
		if (p) {
			status_ = Q_TRAN(&Module_http_action);
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_device_active);
		break;
	}
	}
	return status_;
}
static QState Module_http_action(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC * 20, 0U);
		BSP_usart1_tx_buffer(L206_HTTPACTION, sizeof(L206_HTTPACTION));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		me->was_armed = QTimeEvt_disarm(&me->recvDataTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_READY_SIG: {
		Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
		char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "+HTTPRECV:");
		if (p) {
			p += strlen("+HTTPRECV:");
			// TODO: 此处打印会qs error
			//debug_print("%s", p);
			int ret = parse_http_active_resp(p, strlen(p), me->info.sec_key);
			debug_print("sec_key = %s, ret = %d", me->info.sec_key, ret);
			if (ret == XD_SUCCESS) {
				set_config(SEC_KEY, me->info.sec_key, SECKEY_MAX_LENGTH);
				set_config(ICCID, me->info.iccid, ICCID_MAX_LENGTH);
				status_ = Q_TRAN(&Module_mqtt_connecting);
			}
			else {
				status_ = Q_HANDLED();
			}
		}
		else {
			status_ = Q_HANDLED();
		}
		break;
	}
	case RECV_DATA_TIMEOUT_SIG: {
		TMOUT_PROC_MODULE_RESTART(me);
		break;
	}
	default: {
		status_ = Q_SUPER(&Module_device_active);
		break;
	}
	}
	return status_;
}
