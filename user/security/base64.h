#ifndef FIRMWARE_BASE64_H
#define FIRMWARE_BASE64_H

/**
 * base64编码
 * @param input 要编base64码的数据
 * @param input_length 要编码的数据长度
 * @param output 编码后的base64数据
 * @return
 */
char* base64_encode(const unsigned char *input, int input_length, char *output);

/**
 * base64解码
 * @param input 要解base64码的数据
 * @param output 解base64码后的数据
 * @return
 */
int base64_decode(const char *input, unsigned char *output);

#endif // FIRMWARE_BASE64_H

