#include "qpc.h"
#include "bsp.h"

#include <system.h>


int parse_utc_time(char buf[], uint16_t len)
{
	uint32_t i, j;
	uint8_t time_statue = 0;
	CP56TIME2A_T time = { 0 };
	char *pdata = buf;
	char str[5] = { 0 };

	if (buf == NULL || len < 16)
		return XD_ERROR_INTERNAL;

	memset(str, 0x00, sizeof(str));

	for (j = 0, i = 0; j < len; j++)
	{

		switch (time_statue)
		{
		case 0: //年
		{
			if (*pdata != '/')
			{
				str[i++] = *pdata;
			}
			else
			{
				i = 0;
				time_statue = 1;
				time.year = atoi(str);
				time.year += 2000;
				memset(str, 0x00, sizeof(str));
			}
		}break;
		case 1: //月
		{
			if (*pdata != '/')
				str[i++] = *pdata;
			else
			{
				i = 0;
				time_statue = 2;
				time.month = atoi(str);
				memset(str, 0x00, sizeof(str));
			}
		}break;
		case 2: //日
		{
			if (*pdata != ',')
				str[i++] = *pdata;
			else
			{
				i = 0;
				time_statue = 3;
				time.day = atoi(str);
				memset(str, 0x00, sizeof(str));
			}
		}break;
		case 3: //时
		{
			if (*pdata != ':')
				str[i++] = *pdata;
			else
			{
				i = 0;
				time_statue = 4;
				time.hour = atoi(str);
				memset(str, 0x00, sizeof(str));
			}
		}break;
		case 4: //分
		{
			if (*pdata != ':')
				str[i++] = *pdata;
			else
			{
				i = 0;
				time_statue = 5;
				time.minute = atoi(str);
				memset(str, 0x00, sizeof(str));
			}
		}break;
		case 5: //秒
		{
			if (*pdata != '\"') {
				str[i++] = *pdata;
			}
			else
			{
				i = 0;
				time_statue = 5;
				time.sec = atoi(str);
				memset(str, 0x00, sizeof(str));
				if (time.year >= 2000&& time.month <= 12 && time.day <= 31
					&& time.hour <= 24 && time.minute <= 60 && time.sec <= 60)
				{
					set_timestamp(xd_mktime(time.year, time.month, time.day, time.hour, time.minute, time.sec));
					return XD_SUCCESS;
				}
				else
					return XD_ERROR_INTERNAL;
			}
		}break;
		}
		pdata++;
	}

	return XD_ERROR_INTERNAL;
}

int parse_lati_longi(char buf[], uint16_t len, char *lati_str, char *logi_str)
{
	uint16_t lati_len, longi_len;

	if (buf == NULL || len < 20)
		return XD_ERROR_INTERNAL;

	lati_len = strcspn(buf, ",");
	if (lati_len > 0)
	{
		strncpy(lati_str, buf, lati_len);
	}
	longi_len = strcspn(buf + lati_len + 1, ",");
	if (longi_len > 0)
	{
		strncpy(logi_str, buf + lati_len + 1, longi_len);
	}
	
	return XD_SUCCESS;
}

int mqtt_set_config(char buf[], char *device_id, char *serial_num, char *iccid)
{
	char md5_buf[33] = { 0 };
	char temp[64] = { 0 };

	if (buf == NULL || device_id == NULL || 
		serial_num == NULL || iccid == NULL)
		return XD_ERROR_INTERNAL;

	memset(md5_buf, 0, sizeof(md5_buf));
	memset(temp, 0, sizeof(temp));

	strncpy(temp, "xd", strlen("xd"));
	strncpy(&temp[strlen(temp)], device_id, DEVICEID_MAX_LENGTH);
	strncpy(&temp[strlen(temp)], serial_num, SN_MAX_LENGTH);
	strncpy(&temp[strlen(temp)], iccid, ICCID_MAX_LENGTH);

	get_md5_32_str((uint8_t *)temp, strlen(temp), (uint8_t *)md5_buf);

	debug_print("temp:%s", temp);
	debug_print("md5_buf:%s", md5_buf);

	strncpy(buf, L206_MCONFIG, strlen(L206_MCONFIG));
	strncpy(&buf[strlen(buf)], "\"", strlen("\""));
	strncpy(&buf[strlen(buf)], device_id, DEVICEID_MAX_LENGTH);		// client_id
	strncpy(&buf[strlen(buf)], "\",\"", strlen("\",\""));
	strncpy(&buf[strlen(buf)], device_id, DEVICEID_MAX_LENGTH);		// username
	strncpy(&buf[strlen(buf)], "\",\"", strlen("\",\""));
	strncpy(&buf[strlen(buf)], &md5_buf[8], 16);					// password
	strncpy(&buf[strlen(buf)], "\"\r\n", strlen("\"\r\n"));

	return XD_SUCCESS;
}

int mqtt_set_sub(char buf[], char *device_id)
{
	if (buf == NULL || device_id == NULL)
		return XD_ERROR_INTERNAL;

	strncpy(buf, L206_MSUB, strlen(L206_MSUB));

	strncpy(&buf[strlen(buf)], device_id, DEVICEID_MAX_LENGTH); // topic
	strncpy(&buf[strlen(buf)], "\",1\r\n", strlen("\",1\r\n"));

	return XD_SUCCESS;
}
