//
// Created by YangYongbao on 2017/3/27.
//

#ifndef FIRMWARE_AES_H
#define FIRMWARE_AES_H

#include <stdint.h>

#ifndef CBC
#define CBC 1
#endif

#ifndef ECB
#define ECB 1
#endif

#if defined(ECB) && ECB

void AES128_ECB_encrypt(const uint8_t* input, const uint8_t* key, uint8_t *output);  //AES128加密
void AES128_ECB_decrypt(const uint8_t* input, const uint8_t* key, uint8_t *output);  //AES128解密

#endif // #if defined(ECB) && ECB

#if defined(CBC) && CBC

void AES128_CBC_encrypt_buffer(uint8_t* output, uint8_t* input, uint32_t length, const uint8_t* key, const uint8_t* iv);
void AES128_CBC_decrypt_buffer(uint8_t* output, uint8_t* input, uint32_t length, const uint8_t* key, const uint8_t* iv);

#endif // #if defined(CBC) && CBC

/**
 * aes加密
 * @param input 将要加密的字符串
 * @param key 加密密钥
 * @param output 加密后的字符串
 * @return
 */
int aes128_ecb_encrypt(const uint8_t* input, const uint8_t* key, uint8_t *output);

/**
 * aes解密
 * @param input 将要解密的字符串
 * @param base_len 解密的数据长度
 * @param key 解密密钥
 * @param output 解密后的数据
 * @return 返回pkcs码
 */
uint8_t aes128_ecb_decrypt(const uint8_t* input, int base_len, const uint8_t* key, uint8_t *output); //AES128解密

/**
 * aes加密，生成16进制输出
 * @param input 将要加密的字符串
 * @param key 加密密钥
 * @param flag 转换的结果标志（为0时，转换的结果为小写；非0时，转换的结果为大写）
 * @param output 加密后的十六进制字符串
 */
void aes128_ecb_encrypt_hex(const uint8_t* input, const uint8_t* key, int flag, char *output);

#endif //FIRMWARE_AES_H
