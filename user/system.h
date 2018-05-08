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

#include <error_code.h>
#include <xd_time.h>
#include <l206.h>


#define SN_MAX_LENGTH			14
#define IMEI_MAX_LENGTH			15
#define ICCID_MAX_LENGTH		20

#define DEVICEID_HEAD		"g100000"
#define DEVICEID_HEAD_LENGTH	(sizeof(DEVICEID_HEAD) - 1)
#define DEVICEID_MAX_LENGTH		(DEVICEID_HEAD_LENGTH + IMEI_MAX_LENGTH)



#endif
