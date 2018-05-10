#include "qpc.h"
#include "bsp.h"

#include <system.h>
#include "http.h"

#define HTTP_PRE_URL        ""ACTIVE_SERVER_HOST"/lhc/2.0/device/handle?\0" // http的前缀
#define HTTP_VERSION        "2.0"


int prase_seckey(const char *psrc, char *pstr);

int http_set_url_request(char url[], sys_info_t *info, char *request_type, char *json_data, uint8_t key_flag)
{
	char data_aes_str[400];
	char sign_str[600];

	if (url == NULL || info == NULL || request_type == NULL)
		return XD_ERROR_INTERNAL;


	memcpy(url, L206_HTTPPARA_URL, strlen(L206_HTTPPARA_URL));
	memcpy(url + strlen(url), HTTP_PRE_URL, strlen(HTTP_PRE_URL));
	// 设置请求类型
	memcpy(url + strlen(url), "a=xd.device.", 12);
	memcpy(url + strlen(url), request_type, strlen(request_type));
	// 设置device Id
	memcpy(url + strlen(url), "&devId=", 7);
	memcpy(url + strlen(url), info->device_id, DEVICEID_MAX_LENGTH);

	/* 注：激活接口不带数据 */
	if (key_flag != 0) {
		// 设置真实传输数据的AES加密后的16进制值
		memset(data_aes_str, 0, HTTP_BUFFER_LEN);
		memcpy(url + strlen(url), "&data=", 6);
		aes128_ecb_encrypt_hex((uint8_t *)json_data, (uint8_t *)info->sec_key, 0, data_aes_str);
		memcpy(url + strlen(url), data_aes_str, strlen(data_aes_str));
	}

	memcpy(url + strlen(url), "&t=", 3);

	char time_str[16];
	uint64_t t = 1525923305;
	snprintf(time_str, sizeof(time_str), "%lld", t);

	memcpy(url + strlen(url), time_str, strlen(time_str));

	// generate sign
	memset(sign_str, 0, sizeof(sign_str));
	memcpy(sign_str + strlen(sign_str), "a=xd.device.", 12);
	memcpy(sign_str + strlen(sign_str), request_type, strlen(request_type));
	memcpy(sign_str + strlen(sign_str), "||data=", 7);
	if (key_flag != 0) {
		memcpy(sign_str + strlen(sign_str), data_aes_str, strlen(data_aes_str));
	}

	memcpy(sign_str + strlen(sign_str), "||devId=", 8);
	memcpy(sign_str + strlen(sign_str), info->device_id, DEVICEID_MAX_LENGTH);

	memcpy(url + strlen(url), "&iccId=", 7);
	memcpy(url + strlen(url), info->iccid, ICCID_MAX_LENGTH);

	memcpy(sign_str + strlen(sign_str), "||iccId=", 8);
	memcpy(sign_str + strlen(sign_str), info->iccid, ICCID_MAX_LENGTH);

	memcpy(sign_str + strlen(sign_str), "||t=", 4);
	memcpy(sign_str + strlen(sign_str), time_str, strlen(time_str));
	memcpy(sign_str + strlen(sign_str), "||v=", 4);
	memcpy(sign_str + strlen(sign_str), HTTP_VERSION, 3);
	memcpy(sign_str + strlen(sign_str), "||", 2);

	if (key_flag == 0) {
		memcpy(sign_str + strlen(sign_str), info->serial_num, SN_MAX_LENGTH);
	}
	else {
		memcpy(sign_str + strlen(sign_str), info->sec_key, SECKEY_MAX_LENGTH);
	}

	uint8_t sign_md5_str[33];
	memset(sign_md5_str, 0, 33);
	get_md5_32_str((uint8_t *)sign_str, strlen(sign_str), sign_md5_str);
	memcpy(url + strlen(url), "&sign=", 6);
	memcpy(url + strlen(url), sign_md5_str, 32);

	// 加上AT指令最后一部分
	memcpy(url + strlen(url), "\"\r\n", strlen("\"\r\n"));

	return XD_SUCCESS;
}

int parse_http_active_resp(char *buf, uint16_t len, char sec_key[])
{
	if (buf == NULL || len == 0)
		return XD_ERROR_INTERNAL;

	/* 跳过帧头 */
	char *p = strstr(buf, "Content-Length: ");
	if (p == NULL) return XD_ERROR_INTERNAL;
	p += strlen("Content-Length: ");

	/* 获取body data长度 */
	uint16_t data_len = atoi(p);
	debug_print("data_len = %d", data_len);
	if (data_len > HTTP_BUFFER_LEN) {
		data_len = HTTP_BUFFER_LEN;
	}

	/* 跳过\r\n\r\n */
	p = strstr(p, "\r\n\r\n");
	if (p == NULL) return XD_ERROR_INTERNAL;
	p += strlen("\r\n\r\n");

	return prase_seckey(p, sec_key);
}

int prase_seckey(const char *psrc, char *pstr)
{
	char *ptr1 = strstr(psrc, "secKey");
	if (ptr1 != NULL)//&& point < len)
	{
		int pos1 = ptr1 - psrc;
		//        debug("pos1:%d", pos1);
		char *ptr2 = strstr(&psrc[pos1], ":");
		if (ptr2 == NULL)
			return XD_ERROR_INTERNAL;

		int pos2 = ptr2 - psrc;
		char *ptr3 = strstr(&psrc[pos2], "\"");
		if (ptr3 == NULL)
			return XD_ERROR_INTERNAL;

		int pos3 = ptr3 - psrc;
		char *ptr4 = strstr(&psrc[pos3 + 1], "\"");
		if (ptr4 == NULL)
			return XD_ERROR_INTERNAL;
		int pos4 = ptr4 - psrc;

		if (pos4 - pos3 - 1 == SECKEY_MAX_LENGTH)    //大于seckey的最大长度
		{
			memcpy(pstr, ptr3 + 1, SECKEY_MAX_LENGTH);
			return XD_SUCCESS;
		}
	}

	return XD_ERROR_INTERNAL;
}
