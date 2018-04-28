#include "qpc.h"
#include "charger.h"
#include "bsp.h"

#include <system.h>

Q_DEFINE_THIS_MODULE("module")

#define  L206_HANDCMD          "AT\r\n"                       // OK
#define  L206_MODULECMD        "AT+CGMM\r\n"		          // ��ѯģ���ͺ�  L206
#define  L206_SETBPSCMD        "AT+IPR="                      // Ҫ�ı䲨����
#define  L206_CLOSEATE         "ATE0\r\n"                     // �ر�ָ���ѡ���� "AT+CGMR\r\n" //
#define  L206_SETMUX0CMD       "AT+CIPMUX=0\r\n"              // ��ͨ��   1����ͨ��ģʽ
#define  L206_QGSN             "AT+CGSN\r\n"                  // ��ȡGPRSģ���IMEI��
#define  L206_EGMR             "AT+EGMR=0,5\r\n"              // ��ȡsn���к�  +CSNS?

//#define  L206_CNUM            "AT+CNUM?\r\n"                  // ��ȡ�ֻ���
#define  L206_FINSIMPINCMD     "AT+CPIN?\r\n"                 // �鿴sim���Ƿ����      1
#define  L206_ICCID            "AT+ICCID\r\n"                 // ��ȡ�ֻ���
#define  L206_FINSIMNETCMD1    "AT+CREG?\r\n"                 // ���ģ���Ƿ�ע��GSM    3
#define  L206_CGREG            "AT+CGREG?\r\n"                 // ���ģ���Ƿ�ע��GSM    3
#define  L206_FINGPRSCMD       "AT+CGATT?\r\n"                // ��ѯ GPRS �Ƿ���
#define  L206_CSQCMD           "AT+CSQ\r\n"				      // �ź�����      2
#define  L206_QLTS             "AT+CCLK?\r\n"				  // ��ȡ����ʱ��
//#define  L206_CSTT             "AT+CSTT=\"CMNET\"\r\n"        // ����APN
#define  L206_CSTT             "AT+CSTT=\"CMIOT\"\r\n"        // ����APN
#define  L206_CIICR            "AT+CIICR\r\n"                 // �����pdp
#define  L206_GETIPADDRCMD     "AT+CIFSR\r\n"                 // ��ȡ����ip
#define  L206_GTPOS            "AT+GTPOS\r\n"                 // ��ȡ��γ����Ϣ
#define  L206_GTPOS_1          "AT+GTPOS=1\r\n"                 // ��ȡ��γ����Ϣ
#define  L206_GTPOS_0          "AT+GTPOS=0\r\n"                 // ��ȡ��γ����Ϣ

#define  L206_MIPSTART         "AT+MIPSTART=\"mq.dian.so\",\"1883\"\r\n"    //MQTT
#define  L206_MCONNECT         "AT+MCONNECT=1,180\r\n"
#define  L206_MQTTTMOUT        "AT+MQTTTMOUT=200\r\n"                // keepalive ��ʱʱ�䴦��
#define  L206_MIPCLOSE         "AT+MIPCLOSE\r\n"                    //�ر�MQTT
#define  L206_MDISCONNECT      "AT+MDISCONNECT\r\n"                 //�ر�MQTT
#define  L206_MQTTMSGGET      "AT+MQTTMSGGET=%s\r\n"				// MQTT��ȡbuffer
#define  L206_CIPSTATUS        "AT+CIPSTATUS\r\n"                   //�ر�MQTT
#define  L206_MQTTSTATU        "AT+MQTTSTATU\r\n"                   // mqtt����״̬��ѯ 0 �Ͽ� 1 ���� 2 tcp���ӣ�mqtt�Ͽ�
#define  L206_ISLKVRSCAN       "AT+ISLKVRSCAN\r\n"                  // ��ȡ2Gģ������汾��
#define  L206_HANDCMD_1        "AT+CSCLK=1\r\n"                     // ������ʱ��ģʽ
#define  L206_HANDCMD_0        "AT+CSCLK=0\r\n"                     // �˳���ʱ��ģʽ

#define  L206_HTTPPARA_PORT_80     "AT+HTTPPARA=PORT,80\r\n"      //����http�˿ں�
#define  L206_HTTPPARA_PORT_443    "AT+HTTPPARA=PORT,443\r\n"      //����http�˿ں�
#define  L206_HTTPPARA_CONNECT     "AT+HTTPPARA=CONNECT,0\r\n"      //����http�˿ں�
#define  L206_HTTPSETUP            "AT+HTTPSETUP\r\n"               //����http����
#define  L206_HTTPACTION           "AT+HTTPACTION=0\r\n"            //����http����
#define  L206_HTTPSSL_0            "AT+HTTPSSL=0\r\n"            //����http����
#define  L206_HTTPSSL_1            "AT+HTTPSSL=1\r\n"            //����http����


typedef struct {
	QActive super;

	QTimeEvt delayTimeEvt;
	QTimeEvt recvDataTimeEvt;
	uint8_t timeout_count;
	uint8_t power_on_step;
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
static QState Module_start(Module * const me, QEvt const * const e);
static QState Module_power_on(Module * const me, QEvt const * const e);
static QState Module_hands(Module * const me, QEvt const * const e);
static QState Module_set_baudrate(Module * const me, QEvt const * const e);


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
}

static QState Module_initial(Module * const me, QEvt const * const e)
{

	L206_SLEEP_PRESS(); // ʼ�մ��ڷ�˯��ģʽ
	L206_POWER_ON();	// 2Gģ���ϵ�

	QS_OBJ_DICTIONARY(&l_module);
	QS_OBJ_DICTIONARY(&l_module.delayTimeEvt);
	QS_OBJ_DICTIONARY(&l_module.recvDataTimeEvt);

	QS_FUN_DICTIONARY(&Module_initial);
	QS_FUN_DICTIONARY(&Module_start);
	QS_FUN_DICTIONARY(&Module_power_on);
	QS_FUN_DICTIONARY(&Module_hands);
	QS_FUN_DICTIONARY(&Module_set_baudrate);

	QS_SIG_DICTIONARY(DELAY_TIMEOUT_SIG, me);
	QS_SIG_DICTIONARY(RECV_DATA_TIMEOUT_SIG, me);

	return Q_TRAN(&Module_start);
}

static QState Module_start(Module * const me, QEvt const * const e)
{
	QState status_;
	switch (e->sig) {
		case Q_INIT_SIG: {
			status_ = Q_TRAN(&Module_power_on);
			break;
		}
		default: {
			status_ = Q_SUPER(&QHsm_top);
			break;
		}
	}
	return status_;
}

static QState Module_power_on(Module * const me, QEvt const * const e)
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
			debug_int(me->power_on_step);
			status_ = Q_HANDLED();
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
			status_ = Q_SUPER(&Module_start);
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
			QTimeEvt_armX(&me->recvDataTimeEvt, BSP_TICKS_PER_SEC, BSP_TICKS_PER_SEC);
			me->timeout_count = 0;
			BSP_usart1_tx_buffer(L206_HANDCMD, sizeof(L206_HANDCMD));
			status_ = Q_HANDLED();
			break;
		}
		case Q_EXIT_SIG: {
			QTimeEvt_disarm(&me->recvDataTimeEvt);
			status_ = Q_HANDLED();
			break;
		}
		case RECV_DATA_TIMEOUT_SIG: {
			me->timeout_count++;
			if(me->timeout_count < 10)
			{ 
				BSP_usart1_tx_buffer(L206_HANDCMD, sizeof(L206_HANDCMD));
				status_ = Q_HANDLED();
			}
			else
			{
				status_ = Q_TRAN(&Module_start);
			}
			break;
		}
		case UART_DATA_READY_SIG: {
			Q_ASSERT(Q_EVT_CAST(UartDataEvt)->data != NULL && Q_EVT_CAST(UartDataEvt)->len > 0);
			char *p = strstr(Q_EVT_CAST(UartDataEvt)->data, "AT");
			if (p != NULL) {
				status_ = Q_TRAN(&Module_set_baudrate);
			}
			else
			{
				status_ = Q_HANDLED();
			}
			break;
		}
		default: {
			status_ = Q_SUPER(&Module_start);
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
			status_ = Q_HANDLED();
			break;
		}
		case Q_EXIT_SIG: {
			status_ = Q_HANDLED();
			break;
		}
		default: {
			status_ = Q_SUPER(&Module_start);
			break;
		}
	}
	return status_;
}
