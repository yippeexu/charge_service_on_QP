//
// Created by YangYongbao on 2017/3/27.
//

#ifndef FIRMWARE_MD5_H
#define FIRMWARE_MD5_H

/**
 * 获取32位MD5校验值
 * @param encrypt 要校验的数据
 * @param encrypt_len 要校验的数据长度
 * @param result 生成的MD5校验值
 */
void get_md5_32_str(unsigned char* encrypt, unsigned int encrypt_len, unsigned char* result);

#endif //FIRMWARE_MD5_H
