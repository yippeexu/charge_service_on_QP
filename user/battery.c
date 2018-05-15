#include "qpc.h"
#include "charger.h"
#include "bsp.h"

#include <system.h>

Q_DEFINE_THIS_MODULE("battery")

/* 超时处理：重试，当超过次数后恢复idle状态。 */
#define BAT_OPEN_TMOUT_PROC_TRY_AGAIN(me_, state_, next_state_, times_)	do { \
											if (++(me_)->open_timeout_count < (times_)) { \
												status_ = Q_TRAN((state_)); \
											} \
											else { \
												status_ = Q_TRAN((next_state_)); \
											} \
										}while(0)


typedef struct {
	QActive super;

	QTimeEvt batCheckTimeEvt;
	QTimeEvt batOpenCheckTimeEvt;
	uint8_t battery_heartbeat_count;
	uint8_t open_timeout_count;

	uint8_t ctrl_output;
	battery_status_t status;
}Battery;

/* protected: */
static QState Battery_initial(Battery * const me, QEvt const * const e);
static QState Battery_active(Battery * const me, QEvt const * const e);
static QState Battery_idle(Battery * const me, QEvt const * const e);
static QState Battery_opening(Battery * const me, QEvt const * const e);
static QState Battery_fault(Battery * const me, QEvt const * const e);


/* Local objects -----------------------------------------------------------*/
static Battery l_battery;

/* Global objects ----------------------------------------------------------*/
QActive * const AO_Battery = &l_battery.super; /* "opaque" pointers to Battery AO */

void Battery_ctor(void)
{
	Battery *me = &l_battery;

	QActive_ctor(&me->super, Q_STATE_CAST(&Battery_initial));
	QTimeEvt_ctorX(&me->batCheckTimeEvt, &me->super, BATTERY_CHECK_TIMEOUT_SIG, 0);
	QTimeEvt_ctorX(&me->batOpenCheckTimeEvt, &me->super, BATTERY_OPEN_TIMEOUT_SIG, 0);
}

static QState Battery_initial(Battery * const me, QEvt const * const e)
{

	QS_OBJ_DICTIONARY(&l_battery);
	QS_OBJ_DICTIONARY(&l_battery.batCheckTimeEvt);
	QS_OBJ_DICTIONARY(&l_battery.batOpenCheckTimeEvt);

	QS_FUN_DICTIONARY(&Battery_initial);
	QS_FUN_DICTIONARY(&Battery_active);
	QS_FUN_DICTIONARY(&Battery_idle);
	QS_FUN_DICTIONARY(&Battery_opening);
	QS_FUN_DICTIONARY(&Battery_fault);

	QS_SIG_DICTIONARY(BATTERY_CHECK_TIMEOUT_SIG, me);
	QS_SIG_DICTIONARY(BATTERY_OPEN_TIMEOUT_SIG, me);
	QS_SIG_DICTIONARY(OUTPUT_UPDATE_SIG, me);
	QS_SIG_DICTIONARY(OUTPUT_OPEN_SIG, me);
	QS_SIG_DICTIONARY(OUTPUT_CLOSE_SIG, me);
	QS_SIG_DICTIONARY(BATTERY_REPORT_SIG, me);
	QS_SIG_DICTIONARY(BATTERY_HEARTBEAT_SIG, me);
	QS_SIG_DICTIONARY(CHARGE_FULL_SIG, me);
	QS_SIG_DICTIONARY(CHARGE_NOT_FULL_SIG, me);

	//debug_print("sizeof(report_frame_u) = %d, sizeof(report_frame_t) = %d",
	//	sizeof(report_frame_u), sizeof(report_frame_t));
	return Q_TRAN(&Battery_active);
}
static QState Battery_active(Battery * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->batCheckTimeEvt, BSP_TICKS_PER_SEC * 90, BSP_TICKS_PER_SEC * 90);
		me->battery_heartbeat_count = 0;
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		QTimeEvt_disarm(&me->batCheckTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case Q_INIT_SIG: {
		status_ = Q_TRAN(&Battery_idle);
		break;
	}
	case BATTERY_REPORT_SIG: {
		// TODO
		const report_frame_u *frame = &(Q_EVT_CAST(batteryReportEvt)->frame);
		
		/* 判断充电标志位 */
		if (me->status.oncharging != frame->struct_.bit_oncharging) {
			me->status.oncharging = frame->struct_.bit_oncharging;
			if (me->status.oncharging == 0) {
				static QEvt const chargeOnEvt = { CHARGE_OFF_SIG, 0U, 0U };
				QF_PUBLISH(&chargeOnEvt, me);
			}
			else {
				static QEvt const chargeOffEvt = { CHARGE_ON_SIG, 0U, 0U };
				QF_PUBLISH(&chargeOffEvt, me);
			}
		}

		/* 判断放电标志位 */
		if (me->status.onoutputing != frame->struct_.bit_outputing) {
			me->status.onoutputing = frame->struct_.bit_outputing;
			
			/* 通知状态机从opening状态回到idle状态 */
			if (me->status.onoutputing == me->ctrl_output) {
				QACTIVE_POST(&me->super, Q_NEW(QEvt, OUTPUT_UPDATE_SIG), me);
			}

			if (me->status.onoutputing == 0) {
				QACTIVE_POST(&me->super, Q_NEW(QEvt, OUTPUT_CLOSE_SIG), me);
			}
			else {
				QACTIVE_POST(&me->super, Q_NEW(QEvt, OUTPUT_OPEN_SIG), me);
			}
		}

		/* 判断充饱标志位 */
		if (me->status.charge_full != frame->struct_.bit_charge_full) {
			me->status.charge_full = frame->struct_.bit_charge_full;
			// TODO
			if (me->status.charge_full == 0) {
				//QACTIVE_POST(AO_Display, Q_NEW(QEvt, CHARGE_NOT_FULL_SIG), me);
			}
			else {
				//QACTIVE_POST(AO_Display, Q_NEW(QEvt, CHARGE_FULL_SIG), me);
			}
		}

		/* 更新电量百分比 */
		if (me->status.vol_percent != frame->struct_.bit_vol_percent) {
			me->status.vol_percent = frame->struct_.bit_vol_percent;

			normalEvt *pe = Q_NEW(normalEvt, VOL_PERCENT_UPDATE_SIG);
			pe->v = (uint32_t )me->status.vol_percent;
			QF_PUBLISH(&pe->super, me);
		}

		status_ = Q_HANDLED();
		break;
	}
	case BATTERY_HEARTBEAT_SIG: {
		me->battery_heartbeat_count++;
		status_ = Q_HANDLED();
		break;
	}
	case CTRL_OUTPUT_SIG: {
		me->ctrl_output = Q_EVT_CAST(normalEvt)->v;
		me->open_timeout_count = 0;
		status_ = Q_TRAN(&Battery_opening);
		break;
	}
	case BATTERY_CHECK_TIMEOUT_SIG: {
		if (me->battery_heartbeat_count > 0) {
			me->battery_heartbeat_count = 0;
			status_ = Q_HANDLED();
		}
		else {
			status_ = Q_TRAN(&Battery_fault);
		}
		break;
	}
	default: {
		status_ = Q_SUPER(&QHsm_top);
		break;
	}
	}
	return status_;
}
static QState Battery_idle(Battery * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		status_ = Q_HANDLED();
		break;
	}
	case CTRL_OUTPUT_SIG: {
		me->ctrl_output = Q_EVT_CAST(normalEvt)->v;
		me->open_timeout_count = 0;
		status_ = Q_TRAN(&Battery_opening);
		break;
	}
	default: {
		status_ = Q_SUPER(&Battery_active);
		break;
	}
	}
	return status_;
}
static QState Battery_opening(Battery * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->batOpenCheckTimeEvt, BSP_TICKS_PER_SEC * 3, 0);
		uint8_t frame_tx[5] = { 0xAA, 0x02, 0x02, 0x00, 0x00 };
		frame_tx[3] = me->ctrl_output;
		frame_tx[4] = frame_tx[1] + frame_tx[2] + frame_tx[3];
		BSP_usart3_tx_buffer(frame_tx, sizeof(frame_tx));
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		QTimeEvt_disarm(&me->batOpenCheckTimeEvt);
		status_ = Q_HANDLED();
		break;
	}
	case OUTPUT_UPDATE_SIG: {
		status_ = Q_TRAN(&Battery_idle);
		break;
	}
	case BATTERY_OPEN_TIMEOUT_SIG: {
		BAT_OPEN_TMOUT_PROC_TRY_AGAIN(me, &Battery_opening, &Battery_idle, 2);
		break;
	}
	default: {
		status_ = Q_SUPER(&Battery_active);
		break;
	}
	}
	return status_;
}
static QState Battery_fault(Battery * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		static QEvt const batFaultEvt = { BATTERY_FAULT_SIG, 0U, 0U };
		QF_PUBLISH(&batFaultEvt, me);
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		static QEvt const batOkEvt = { BATTERY_OK_SIG, 0U, 0U };
		QF_PUBLISH(&batOkEvt, me);
		status_ = Q_HANDLED();
		break;
	}
	case BATTERY_REPORT_SIG: {
		QACTIVE_POST(&me->super, Q_NEW(QEvt, BATTERY_REPORT_SIG), me);
		status_ = Q_TRAN(&Battery_active);
		break;
	}
	case BATTERY_HEARTBEAT_SIG: {
		QACTIVE_POST(&me->super, Q_NEW(QEvt, BATTERY_HEARTBEAT_SIG), me);
		status_ = Q_TRAN(&Battery_active);
		break;
	}
	default: {
		status_ = Q_SUPER(&QHsm_top);
		break;
	}
	}
	return status_;
}
