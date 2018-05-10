#ifndef __HTTP_H__
#define __HTTP_H__


#define HTTP_BUFFER_LEN     1536//FIRMWARE_BLOCK_SIZE + 256


/* 设置AT+HTTPPARA=URL,指令的url */
int http_set_url_request(char url[], sys_info_t *info, char *request_type, char *json_data, uint8_t key_flag);

/* 解析设备激活的应答，获取sec_key */
int parse_http_active_resp(char *buf, uint16_t len, char sec_key[]);

#endif //__HTTP_H__
