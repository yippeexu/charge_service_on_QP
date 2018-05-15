#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <string_tool.h>
#include <md5.h>
#include <base64.h>
#include <aes.h>


#define ENV_RELEASE

#if defined(ENV_RELEASE)
#define MQTT_SERVER_HOST "mq.dian.so"
#define ACTIVE_SERVER_HOST "b.dian.so"
#elif defined(ENV_DEV)
#define MQTT_SERVER_HOST "mq2.dian.so"
#define ACTIVE_SERVER_HOST "bdev.dian.so"
#elif defined(ENV_DOCKER5)
#define MQTT_SERVER_HOST "47.93.140.17"
#define ACTIVE_SERVER_HOST "bdev.dian.so"
#else
#define MQTT_SERVER_HOST
#define ACTIVE_SERVER_HOST
#endif


#define SN_MAX_LENGTH			14
#define IMEI_MAX_LENGTH			15
#define ICCID_MAX_LENGTH		20
#define SECKEY_MAX_LENGTH		16

#define DEVICEID_HEAD		"g300000"
#define DEVICEID_HEAD_LENGTH	(sizeof(DEVICEID_HEAD) - 1)
#define DEVICEID_MAX_LENGTH		(DEVICEID_HEAD_LENGTH + IMEI_MAX_LENGTH)

typedef struct {
	char device_id[DEVICEID_MAX_LENGTH + 1];
	char serial_num[SN_MAX_LENGTH + 1];
	char iccid[ICCID_MAX_LENGTH + 1];
	char sec_key[SECKEY_MAX_LENGTH + 1];
}sys_info_t;

typedef struct {
	uint8_t oncharging;
	uint8_t charge_full;
	uint8_t onoutputing;
	uint8_t vol_percent;
}battery_status_t;


#include <error_code.h>
#include <xd_time.h>
#include <l206.h>
#include <http.h>
#include <xd_flash.h>


void init_sys_info(sys_info_t *info);


#endif
