//
// Created by YangYongbao on 2017/4/22.
//

#include "string_tool.h"
#include <string.h>
#include <stdlib.h>

unsigned char hex_str_lower[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
unsigned char hex_str_upper[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

void ascii2hex(uint8_t *ascii, uint16_t ascii_len, int flag, char *result)
{
    uint16_t hex_index = 0;

    for (uint16_t index=0; index<ascii_len; index++) {
        // 十进制转16进制字符串
        if (flag == 0) {
            result[hex_index++] = hex_str_lower[(((int) ascii[index] & 0xf0) >> 4)]; // 高8位
            result[hex_index++] = hex_str_lower[((int) ascii[index] & 0x0f)]; // 低8位
        } else {
            result[hex_index++] = hex_str_upper[(((int) ascii[index] & 0xf0) >> 4)]; // 高8位
            result[hex_index++] = hex_str_upper[((int) ascii[index] & 0x0f)]; // 低8位
        }
    }
}

#define VERSION_PRE_LEN     10

/**
 * 将字符串形式的版本号转为数组形式的版本号
 * @param version 字符串版本号
 * @param ver_arr 数组形式版本号
 * @return
 */
int get_version(char *version, int *ver_arr)
{
    if (version == NULL)
        return -1;

    int index = 0;
    char num_str[VERSION_PRE_LEN];
    char *cur = version;
    char *pos = strchr(version, '.');
    while (pos != '\0') {
        memset(num_str, 0, VERSION_PRE_LEN);
        if (cur == version) {
            memcpy(num_str, cur, pos - cur);
        } else {
            memcpy(num_str, cur + 1, pos - cur);
        }

        ver_arr[index] = atoi(num_str);
        cur = pos;
        pos = strchr(pos + 1, '.');
        index++;
    }

    memset(num_str, 0, VERSION_PRE_LEN);
    memcpy(num_str, cur + 1, version + strlen(version) - cur);
    ver_arr[index] = atoi(num_str);

    return 0;
}

int compare_version(char *version1, char *version2, int count)
{
    int ver_arr1[count];
    int ver_arr2[count];

    get_version(version1, ver_arr1);
    get_version(version2, ver_arr2);

    // 一段一段的比较
    for (int index = 0; index < count; index++) {
        if (ver_arr1[index] > ver_arr2[index]) {
            return 1;
        } else if (ver_arr1[index] < ver_arr2[index]) {
            return -1;
        }
    }

    return 0;
}

int is_string(char *buf, uint16_t len)
{
	for (uint16_t index = 0; index < len; index++)
	{
		if (!(buf[index] >= '0' && buf[index] <= '9' || buf[index] >= 'a' && buf[index] <= 'z' || buf[index] >= 'A' && buf[index] <= 'Z'))
		{
			return -1;
		}
	}

	return 0;
}

#define IS_NUMBER(c) ((c >= '0' && c <= '9') ? 1 : 0)
#define IS_NOT_NUMBER(c) ((c >= '0' && c <= '9') ? 0 : 1)

int capture_number(char buf[], uint16_t len)
{
	int i, j, n = 0;

	// 忽略非数字字符
	for (i = 0; i < len && IS_NOT_NUMBER(buf[i]); i++) {};
	
	if (i >= len) return -1;

	// 捕获数字字符
	for (j = 0; IS_NUMBER(buf[i+j]); j++) {
		n = 10 * n + (buf[i + j] - '0');
	}
		
	return n;
}