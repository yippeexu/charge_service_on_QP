#ifndef __L206_H__
#define __L206_H__


#define  L206_HANDCMD          "AT\r\n"                       // OK
#define  L206_BAUDRATE_115200  "AT+IPR=115200\r\n"                      // 要改变波特率
#define  L206_MODULE_MODEL     "AT+CGMM\r\n"		          // 查询模块型号  L206
#define  L206_CLOSEATE         "ATE0\r\n"                     // 关闭指令回选功能 "AT+CGMR\r\n" //
#define  L206_EGMR             "AT+EGMR=0,5\r\n"              // 获取sn序列号  +CSNS?
#define  L206_QGSN             "AT+CGSN\r\n"                  // 获取GPRS模块的IMEI码
#define  L206_FINSIMPINCMD     "AT+CPIN?\r\n"                 // 查看sim卡是否解锁      1
#define  L206_ICCID            "AT+ICCID\r\n"                 // 获取手机号
#define  L206_FINSIMNETCMD1    "AT+CREG?\r\n"                 // 检查模块是否注册GSM    3
#define  L206_CGREG            "AT+CGREG?\r\n"                 // 检查模块是否注册GSM    3
#define  L206_CGATT		       "AT+CGATT?\r\n"                // 查询 GPRS 是否附着
#define  L206_CSQ			   "AT+CSQ\r\n"				      // 信号质量      2
#define  L206_CCLK             "AT+CCLK?\r\n"				  // 获取区域时间
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


#endif //__L206_H__
