#include "qpc.h"
#include "charger.h"
#include "bsp.h"

#include <system.h>
#include <fifo.h>

Q_DEFINE_THIS_MODULE("module_rx")


typedef struct {
	QActive super;

	QTimeEvt pollTimeEvt;
	uint32_t recv_len_last;
}Module_rx;

/* protected: */
static QState Module_rx_initial(Module_rx * const me, QEvt const * const e);
static QState Module_rx_idle(Module_rx * const me, QEvt const * const e);
static QState Module_rx_polling(Module_rx * const me, QEvt const * const e);


/* Local objects -----------------------------------------------------------*/
static Module_rx l_module_rx;

/* Global objects ----------------------------------------------------------*/
QActive * const AO_Module_rx = &l_module_rx.super; /* "opaque" pointers to Philo AO */

fifo_t receiver_fifo;
uint8_t receiver_fifo_buf[FIFO_MAX_LENGTH] = { 0 };

void Module_rx_ctor(void)
{
	Module_rx *me = &l_module_rx;

	QActive_ctor(&me->super, Q_STATE_CAST(&Module_rx_initial));
	QTimeEvt_ctorX(&me->pollTimeEvt, &me->super, POLL_TIMEOUT_SIG, 0);

	/* 2G Module FIFO init on USART1*/
	fifo_init(&receiver_fifo, receiver_fifo_buf, sizeof(receiver_fifo_buf));

}

static QState Module_rx_initial(Module_rx * const me, QEvt const * const e)
{

	QS_OBJ_DICTIONARY(&l_module_rx);
	QS_OBJ_DICTIONARY(&l_module_rx.pollTimeEvt);

	QS_FUN_DICTIONARY(&Module_rx_initial);
	QS_FUN_DICTIONARY(&Module_rx_idle);
	QS_FUN_DICTIONARY(&Module_rx_polling);

	QS_SIG_DICTIONARY(POLL_TIMEOUT_SIG, me);

	return Q_TRAN(&Module_rx_idle);
}

static QState Module_rx_idle(Module_rx * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		status_ = Q_HANDLED();
		break;
	}
	case UART_DATA_BEGIN_SIG: {
		status_ = Q_TRAN(&Module_rx_polling);
		break;
	}
	default: {
		status_ = Q_SUPER(&QHsm_top);
		break;
	}
	}
	return status_;
}

static QState Module_rx_polling(Module_rx * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		QTimeEvt_armX(&me->pollTimeEvt, BSP_TICKS_PER_SEC / 10, BSP_TICKS_PER_SEC / 10);
		me->recv_len_last = 0;
		status_ = Q_HANDLED();
		break;
	}
	case Q_EXIT_SIG: {
		QTimeEvt_disarm(&me->pollTimeEvt);
		me->recv_len_last = 0;
		status_ = Q_HANDLED();
		break;
	}
	case POLL_TIMEOUT_SIG: {
		uint32_t len = fifo_len(&receiver_fifo);
		Q_ASSERT(len > 0);
		if (len == me->recv_len_last) {
			UartDataEvt *pe = Q_NEW(UartDataEvt, UART_DATA_READY_SIG);
			pe->len = len;
			for (uint32_t i = 0; i < len; i++) {
				uint8_t c;
				uint32_t ret = fifo_out_c(&receiver_fifo, &c);
				Q_ASSERT(ret == 0);
				pe->data[i] = c;
			}
			debug_str(pe->data);
			QACTIVE_POST(AO_Module, &pe->super, me);
			status_ = Q_TRAN(&Module_rx_idle);
		}
		else {
			me->recv_len_last = len;
			status_ = Q_HANDLED();
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
