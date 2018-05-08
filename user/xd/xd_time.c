#include "xd_time.h"

uint64_t xd_mktime(uint16_t year, uint8_t mon_a,
	uint8_t day, uint8_t hour,
	uint8_t min, uint8_t sec)
{
	int mon = mon_a;

	if (0 >= (mon -= 2)) {    /* 1..12 -> 11,12,1..10 */
		mon += 12;      /* Puts Feb last since it has leap day */
		year -= 1;
	}

	uint64_t time = ((((uint64_t) (year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day) +
		year * 365 - 719499
		) * 24 + hour /* now have hours */
		) * 60 + min /* now have minutes */
		) * 60 + sec; /* finally seconds */
	return time;// - 8*60*60;
}