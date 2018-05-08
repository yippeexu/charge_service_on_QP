#ifndef __XD_TIME_H__
#define __XD_TIME_H__

#include <stdint.h>

// 时标
typedef struct strCP56TIME2A_
{
	uint16_t year;	// 年(from 2000)
	uint8_t month;	// 月(1-12)
	uint8_t day;	// 日(1-31)
	uint8_t hour;	// 时(0-23)
	uint8_t minute;	// 分(0-59)
	uint8_t sec;	// 秒
} CP56TIME2A_T;


uint64_t xd_mktime(uint16_t year, uint8_t mon_a,
	uint8_t day, uint8_t hour,
	uint8_t min, uint8_t sec);

#endif //__XD_TIME_H__
