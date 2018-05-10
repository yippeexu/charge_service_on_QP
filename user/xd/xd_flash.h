//
// Created by YangYongbao on 2017/3/23.
//

#ifndef FW_STM32_1S_FLASH_H
#define FW_STM32_1S_FLASH_H

#include "system.h"

#define APPLICATION_RUN_ADDRESS     0x008008000
#define APPLICATION_ISP_ADDRESS     0x008022000
#define APPLICATION_PAGE_COUNT      52
#define FLASH_PAGE_SIZE             2048        //1024       // flash页大小

//需要存储的读取校验
__packed typedef struct _ST_PARAM_DATA{
    INT8U app_config;             // 获取app配置信息
    INT8U sec_key[16];            // 服务器获取的加密数据
    INT8U iccid[20];              // 产品序列号
    INT8U active_flag;            // 激活标志位
    INT8U md5[8];                 // MD5前8位作为存储校验
}PARAM_DATA;

typedef struct UPDATE_STRUCT
{
    INT8U       app_config;             //
    INT8U       update_flag;            // 升级标志位
    uint32_t    firmware_len;           // 固件长度
    uint32_t    firmware_crc;           // 固件CRC校验
    uint8_t     firmware_url[200];      // 固件URL
    INT8U       err_flag;               // 升級錯誤類型
    INT8U       md5[8];                 // MD5前8位作为存储校验
} UPDATE_STRUCT;

/**
 * 配置枚举值，设置配置。在Flash从后往前的顺序进行读写
 * 注意：枚举值不能修改
 */
typedef enum {
    // 同一个flash区
    APP_CONFIG  = 1,    // APP的启动区域
    SEC_KEY,            // 加密密钥
    ICCID,              // 存储20iccid
    ACTIVE_FLAG,        // 存储激活标志位
    // 同一个flash区
    UPDATE_FLAG,        // 存储升级标志位
    FIRMWARE_INFO       // 固件信息
} CONFIG_KEY;

/**
 * stm32的内置Flash编程操作都是以页为单位写入
 * 写入的操作必须要以32位字或16位半字宽度数据为单位，允许跨页写；写入非字或半字长数据时将导致stm32内部总线错误。
 * @param config_name
 * @param value
 * @param len
 * @return
 */
int set_config(CONFIG_KEY config_name, uint8_t *value, uint32_t len);

int get_config(CONFIG_KEY config_name, uint8_t *value, uint32_t len);

void erase_config_flash(uint32_t address);

int write_flash(uint32_t *flash_address, uint32_t* data, uint32_t data_length, uint32_t flash_end_address);

#endif //FW_STM32_1S_FLASH_H
