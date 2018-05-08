//
// Created by YangYongbao on 2017/4/22.
//

#ifndef FIRMWARE_STRING_TOOL_H
#define FIRMWARE_STRING_TOOL_H

#include "stdint.h"

/**
 * ascii值转为十六进制
 * @param ascii 要转换的ascii数据
 * @param ascii_len 要转换的ascii数据长度
 * @param flag 转换的结果标志（为0时，转换的结果为小写；非0时，转换的结果为大写）
 * @param result 转换的十六进制结果
 */
void ascii2hex(uint8_t *ascii, uint16_t ascii_len, int flag, char *result);

/**
 * 字符串形式的版本号比较
 * @param version1
 * @param version2
 * @param count 段数
 * @return 0: 版本相同, 1: version1>version2, -1:version1<version2
 */
int compare_version(char *version1, char *version2, int count);

/**
* 判断字符串是否为合法字符（数字、字母）
* @param buf
* @param len
* @return 0: 是, -1:否
*/
int is_string(char *buf, uint16_t len);

#endif //FIRMWARE_STRING_TOOL_H
