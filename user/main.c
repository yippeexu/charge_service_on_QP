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
#include <system.h>

int main()
{
	static QEvt const *batteryQueueSto[5];
	static QEvt const *moduleQueueSto[8];
	static QEvt const *moduleRxQueueSto[3];

	// TODO: QEvt类型不可行，选择合适事件类型给smlPoolSto
	static QF_MPOOL_EL(batteryReportEvt)	smlPoolSto[10];
	static QF_MPOOL_EL(UartDataEvt)			medPoolSto[4];
	static QF_MPOOL_EL(largeUartDataEvt)	largePoolSto[2];

	static QSubscrList    subscrSto[MAX_PUB_SIG];

	/* explicitly invoke the active objects' ctors... */
	Battery_ctor();
	Module_ctor();
	Module_rx_ctor();

	QF_init();    /* initialize the framework and the underlying RT kernel */
	BSP_init();   /* initialize the Board Support Package */

	/* init publish-subscribe... */
	QF_psInit(subscrSto, Q_DIM(subscrSto));

	/* initialize the event pools... */
	QF_poolInit(smlPoolSto, sizeof(smlPoolSto), sizeof(smlPoolSto[0]));
	QF_poolInit(medPoolSto, sizeof(medPoolSto), sizeof(medPoolSto[0]));
	QF_poolInit(largePoolSto, sizeof(largePoolSto), sizeof(largePoolSto[0]));

	/* send object dictionaries for event queues... */
	QS_OBJ_DICTIONARY(batteryQueueSto);
	QS_OBJ_DICTIONARY(moduleQueueSto);
	QS_OBJ_DICTIONARY(moduleRxQueueSto);

	/* send object dictionaries for event pools... */
	QS_OBJ_DICTIONARY(smlPoolSto);
	QS_OBJ_DICTIONARY(medPoolSto);
	QS_OBJ_DICTIONARY(largePoolSto);

	/* send signal dictionaries for globally published events... */
	QS_SIG_DICTIONARY(BATTERY_OK_SIG, (void *)0);
	QS_SIG_DICTIONARY(BATTERY_FAULT_SIG, (void *)0);
	QS_SIG_DICTIONARY(CHARGE_ON_SIG, (void *)0);
	QS_SIG_DICTIONARY(CHARGE_OFF_SIG, (void *)0);
	QS_SIG_DICTIONARY(VOL_PERCENT_UPDATE_SIG, (void *)0);

	/* start the active objects... */
	QACTIVE_START(AO_Battery,
		1U,                /* QP priority */
		batteryQueueSto, Q_DIM(batteryQueueSto), /* evt queue */
		(void *)0, 0U,     /* no per-thread stack */
		(QEvt *)0);        /* no initialization event */

	QACTIVE_START(AO_Module_rx,
		2U,                /* QP priority */
		moduleRxQueueSto, Q_DIM(moduleRxQueueSto), /* evt queue */
		(void *)0, 0U,     /* no per-thread stack */
		(QEvt *)0);        /* no initialization event */
	QACTIVE_START(AO_Module,
		3U,                /* QP priority */
		moduleQueueSto, Q_DIM(moduleQueueSto), /* evt queue */
		(void *)0, 0U,     /* no per-thread stack */
		(QEvt *)0);        /* no initialization event */
	//QACTIVE_START(AO_Display,
	//	4U,                /* QP priority */
	//	, Q_DIM(), /* evt queue */
	//	(void *)0, 0U,     /* no per-thread stack */
	//	(QEvt *)0);        /* no initialization event */

	return QF_run(); /* run the QF application */
}
