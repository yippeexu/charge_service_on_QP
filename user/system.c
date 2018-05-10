#include "system.h"
#include "qpc.h"



void init_sys_info(sys_info_t *info)
{
	get_config(SEC_KEY, info->sec_key, SECKEY_MAX_LENGTH);
	get_config(ICCID, info->iccid, ICCID_MAX_LENGTH);

}
