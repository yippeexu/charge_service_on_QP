/*****************************************************************************
* Product: charge service based on QPC
* Designed by Ryan.Lei
* Last Updated for Version: 5.4.026
*
* Contact information:
* Email: 1184634560@qq.com
*****************************************************************************/

#include "qpc.h"
#include "charger.h"
#include "bsp.h"

int main()
{
	static QEvt const *moduleQueueSto[4];
	static QEvt const *moduleRxQueueSto[3];

	static QF_MPOOL_EL(QEvt)				smlPoolSto[10];
	static QF_MPOOL_EL(UartDataEvt)			medPoolSto[4];
	static QF_MPOOL_EL(largeUartDataEvt)	largePoolSto[2];

	/* explicitly invoke the active objects' ctors... */
	Module_ctor();
	Module_rx_ctor();

	QF_init();    /* initialize the framework and the underlying RT kernel */
	BSP_init();   /* initialize the Board Support Package */

	/* initialize the event pools... */
	QF_poolInit(smlPoolSto, sizeof(smlPoolSto), sizeof(smlPoolSto[0]));
	QF_poolInit(medPoolSto, sizeof(medPoolSto), sizeof(medPoolSto[0]));
	QF_poolInit(largePoolSto, sizeof(largePoolSto), sizeof(largePoolSto[0]));

	/* send object dictionaries for event queues... */
	QS_OBJ_DICTIONARY(moduleQueueSto);
	QS_OBJ_DICTIONARY(moduleRxQueueSto);

	/* send object dictionaries for event pools... */
	QS_OBJ_DICTIONARY(smlPoolSto);
	QS_OBJ_DICTIONARY(medPoolSto);
	QS_OBJ_DICTIONARY(largePoolSto);

	/* send signal dictionaries for globally published events... */
	QS_SIG_DICTIONARY(UART_DATA_BEGIN_SIG, (void *)0);
	QS_SIG_DICTIONARY(UART_DATA_READY_SIG, (void *)0);

	/* start the active objects... */
	QACTIVE_START(AO_Module,
		1U,                /* QP priority */
		moduleQueueSto, Q_DIM(moduleQueueSto), /* evt queue */
		(void *)0, 0U,     /* no per-thread stack */
		(QEvt *)0);        /* no initialization event */
	QACTIVE_START(AO_Module_rx,
		2U,                /* QP priority */
		moduleRxQueueSto, Q_DIM(moduleRxQueueSto), /* evt queue */
		(void *)0, 0U,     /* no per-thread stack */
		(QEvt *)0);        /* no initialization event */

	return QF_run(); /* run the QF application */
}
