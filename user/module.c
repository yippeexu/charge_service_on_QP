#include "qpc.h"
#include "charger.h"
#include "bsp.h"

#include <system.h>

Q_DEFINE_THIS_MODULE("module")

#define  L206_HANDCMD          "AT\r\n"                       // OK
#define  L206_MODULECMD        "AT+CGMM\r\n"		          // 查询模块型号  L206
#define  L206_SETBPSCMD        "AT+IPR="                      // 要改变波特率
#define  L206_CLOSEATE         "ATE0\r\n"                     // 关闭指令回选功能 "AT+CGMR\r\n" //
#define  L206_SETMUX0CMD       "AT+CIPMUX=0\r\n"              // 单通道   1，多通道模式
#define  L206_QGSN             "AT+CGSN\r\n"                  // 获取GPRS模块的IMEI码
#define  L206_EGMR             "AT+EGMR=0,5\r\n"              // 获取sn序列号  +CSNS?

//#define  L206_CNUM            "AT+CNUM?\r\n"                  // 获取手机号
#define  L206_FINSIMPINCMD     "AT+CPIN?\r\n"                 // 查看sim卡是否解锁      1
#define  L206_ICCID            "AT+ICCID\r\n"                 // 获取手机号
#define  L206_FINSIMNETCMD1    "AT+CREG?\r\n"                 // 检查模块是否注册GSM    3
#define  L206_CGREG            "AT+CGREG?\r\n"                 // 检查模块是否注册GSM    3
#define  L206_FINGPRSCMD       "AT+CGATT?\r\n"                // 查询 GPRS 是否附着
#define  L206_CSQCMD           "AT+CSQ\r\n"				      // 信号质量      2
#define  L206_QLTS             "AT+CCLK?\r\n"				  // 获取区域时间
//#define  L206_CSTT             "AT+CSTT=\"CMNET\"\r\n"        // 设置APN
#define  L206_CSTT             "AT+CSTT=\"CMIOT\"\r\n"        // 设置APN
#define  L206_CIICR            "AT+CIICR\r\n"                 // 激活场景pdp
#define  L206_GETIPADDRCMD     "AT+CIFSR\r\n"                 // 获取本地ip
#define  L206_GTPOS            "AT+GTPOS\r\n"                 // 获取经纬度信息
#define  L206_GTPOS_1          "AT+GTPOS=1\r\n"                 // 获取经纬度信息
#define  L206_GTPOS_0          "AT+GTPOS=0\r\n"                 // 获取经纬度信息

#define  L206_MIPSTART         "AT+MIPSTART=\"mq.dian.so\",\"1883\"\r\n"    //MQTT
#define  L206_MCONNECT         "AT+MCONNECT=1,180\r\n"
#define  L206_MQTTTMOUT        "AT+MQTTTMOUT=200\r\n"                // keepalive 超时时间处理
#define  L206_MIPCLOSE         "AT+MIPCLOSE\r\n"                    //关闭MQTT
#define  L206_MDISCONNECT      "AT+MDISCONNECT\r\n"                 //关闭MQTT
#define  L206_MQTTMSGGET      "AT+MQTTMSGGET=%s\r\n"				// MQTT读取buffer
#define  L206_CIPSTATUS        "AT+CIPSTATUS\r\n"                   //关闭MQTT
#define  L206_MQTTSTATU        "AT+MQTTSTATU\r\n"                   // mqtt连接状态查询 0 断开 1 正常 2 tcp连接，mqtt断开
#define  L206_ISLKVRSCAN       "AT+ISLKVRSCAN\r\n"                  // 获取2G模块软件版本号
#define  L206_HANDCMD_1        "AT+CSCLK=1\r\n"                     // 进入慢时钟模式
#define  L206_HANDCMD_0        "AT+CSCLK=0\r\n"                     // 退出慢时钟模式

#define  L206_HTTPPARA_PORT_80     "AT+HTTPPARA=PORT,80\r\n"      //设置http端口号
#define  L206_HTTPPARA_PORT_443    "AT+HTTPPARA=PORT,443\r\n"      //设置http端口号
#define  L206_HTTPPARA_CONNECT     "AT+HTTPPARA=CONNECT,0\r\n"      //设置http端口号
#define  L206_HTTPSETUP            "AT+HTTPSETUP\r\n"               //建立http连接
#define  L206_HTTPACTION           "AT+HTTPACTION=0\r\n"            //请求http数据
#define  L206_HTTPSSL_0            "AT+HTTPSSL=0\r\n"            //请求http数据
#define  L206_HTTPSSL_1            "AT+HTTPSSL=1\r\n"            //请求http数据


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

	L206_SLEEP_PRESS(); // 始终处于非睡眠模式
	L206_POWER_ON();	// 2G模块上电

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
