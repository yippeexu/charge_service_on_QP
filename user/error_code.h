#ifndef __ERROR_CODE_H__
#define __ERROR_CODE_H__ 


/** @defgroup NRF_ERRORS_BASE Error Codes Base number definitions
* @{ */
#define XD_ERROR_BASE_NUM      (0x0)       ///< Global error base
#define XD_ERROR_SDM_BASE_NUM  (0x1000)    ///< SDM error base
#define XD_ERROR_SOC_BASE_NUM  (0x2000)    ///< SoC error base
#define XD_ERROR_STK_BASE_NUM  (0x3000)    ///< STK error base
/** @} */

#define XD_SUCCESS                           (XD_ERROR_BASE_NUM + 0)  ///< Successful command
#define XD_ERROR_SVC_HANDLER_MISSING         (XD_ERROR_BASE_NUM + 1)  ///< SVC handler is missing
#define XD_ERROR_SOFTDEVICE_NOT_ENABLED      (XD_ERROR_BASE_NUM + 2)  ///< SoftDevice has not been enabled
#define XD_ERROR_INTERNAL                    (XD_ERROR_BASE_NUM + 3)  ///< Internal Error
#define XD_ERROR_NO_MEM                      (XD_ERROR_BASE_NUM + 4)  ///< No Memory for operation
#define XD_ERROR_NOT_FOUND                   (XD_ERROR_BASE_NUM + 5)  ///< Not found
#define XD_ERROR_NOT_SUPPORTED               (XD_ERROR_BASE_NUM + 6)  ///< Not supported
#define XD_ERROR_INVALID_PARAM               (XD_ERROR_BASE_NUM + 7)  ///< Invalid Parameter
#define XD_ERROR_INVALID_STATE               (XD_ERROR_BASE_NUM + 8)  ///< Invalid state, operation disallowed in this state
#define XD_ERROR_INVALID_LENGTH              (XD_ERROR_BASE_NUM + 9)  ///< Invalid Length
#define XD_ERROR_INVALID_FLAGS               (XD_ERROR_BASE_NUM + 10) ///< Invalid Flags
#define XD_ERROR_INVALID_DATA                (XD_ERROR_BASE_NUM + 11) ///< Invalid Data
#define XD_ERROR_DATA_SIZE                   (XD_ERROR_BASE_NUM + 12) ///< Data size exceeds limit
#define XD_ERROR_TIMEOUT                     (XD_ERROR_BASE_NUM + 13) ///< Operation timed out
#define XD_ERROR_NULL                        (XD_ERROR_BASE_NUM + 14) ///< Null Pointer
#define XD_ERROR_FORBIDDEN                   (XD_ERROR_BASE_NUM + 15) ///< Forbidden Operation
#define XD_ERROR_INVALID_ADDR                (XD_ERROR_BASE_NUM + 16) ///< Bad Memory Address
#define XD_ERROR_BUSY                        (XD_ERROR_BASE_NUM + 17) ///< Busy


enum errorCode {
	MQTT_CONN_ERROR = 0x10,	// mqtt连接失败
	HAND_ERROR = 0x11,		// 2G模块通信失败
	CARDPIN_ERROR = 0x12,	// SIM卡问题
	NETWORK_ERROR = 0x13,	// 网络连接基站出错
	BATTERY_ERROR = 0x14,	// 电池通信故障
	HTTP_CONN_ERROR = 0x15,	// http连接故障
	ICCID_ERROR = 0x16,		// ICCID获取失败
};

#endif
