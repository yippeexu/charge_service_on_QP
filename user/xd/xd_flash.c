#include <stm32f10x.h>  /* CMSIS-compliant header file for the MCU used */
#include <system.h>
#include "xd_flash.h"


#define CONFIG_INFO_ADDRESS        0x00803F000      //sec_key, cell num, crc
#define FIRMWARE_INFO_ADDRESS      0x00803F800      //固件信息

void erase_config_flash(uint32_t address);
int write_flash(uint32_t *flash_address, uint32_t* data, uint32_t data_length, uint32_t flash_end_address);
void read_flash(uint32_t addr, uint8_t *pBuffer, uint16_t dataLen);


// 存储配置信息
int set_config(CONFIG_KEY config_name, char *value, uint32_t len)
{
    uint32_t address;
    uint8_t buf[512] = {0};
    uint8_t temp[33] = {0};
    uint16_t save_len = 0;
    PARAM_DATA *pdata = NULL;

    if (value == NULL || len == 0 || len > 1024)
    {
		//Sleep(50);
        return -1;
    }

    memset( buf, 0x00, sizeof(buf));

    if( config_name == APP_CONFIG || config_name == SEC_KEY || \
        config_name == ICCID   || config_name == ACTIVE_FLAG )
    {
        address = CONFIG_INFO_ADDRESS;
        save_len = sizeof(PARAM_DATA) + 4;
    }
    else if( config_name == UPDATE_FLAG || config_name == FIRMWARE_INFO)
    {
        address = FIRMWARE_INFO_ADDRESS;
        save_len = sizeof(UPDATE_STRUCT) + 4;

    } else
    {
		//Sleep(50);
        return -1;
    }
    uint8_t cnt;
    cnt = 5;
    do{
        // 存flash前先读取flash内的数据
        read_flash( address, buf, save_len);
        if(config_name != FIRMWARE_INFO)
        {
            pdata = (PARAM_DATA *)buf;
        }
//        debug("buf:%s", buf);
        switch (config_name) {
            case APP_CONFIG:
            {
                pdata->app_config = *value;
            }break;
            case SEC_KEY:
            {
                memcpy( pdata->sec_key, value, len);
            }break;
            case ICCID:
            {
                memcpy( pdata->iccid, value, len);
            }break;
            case ACTIVE_FLAG:
            {
                pdata->active_flag = *value;
            }break;
                /////////////////////////
            case UPDATE_FLAG:
            case FIRMWARE_INFO:
            {
                memcpy( buf, value, len);
            }break;
            default:
                return -1;
        }

        if(config_name != FIRMWARE_INFO)
        {
            get_md5_32_str(buf, sizeof(PARAM_DATA) - 8, temp);      //&buf[sizeof(PARAM_DATA) - 8]);
            memcpy( &buf[sizeof(PARAM_DATA) - 8], temp, 8);
        }
        else
        {
            get_md5_32_str(buf, sizeof(UPDATE_STRUCT) - 8, temp);   //&buf[sizeof(UPDATE_STRUCT) - 8]);
            memcpy( &buf[sizeof(UPDATE_STRUCT) - 8], temp, 8);
        }
//        debug("buf:%s",buf);
        //Sleep(10);
        if(write_flash( &address, (uint32_t *)buf, save_len, address + save_len))
        {
            //Sleep(50);
            cnt--;
            //debug("write flash fail 4");
            if(cnt == 0)
            {
                return -1;
            }
            continue;
        } else
        {
            //debug("write flash ok");
            return 0;
        }
    }while(cnt);

    return 0;
}

// 获取配置信息
int get_config(CONFIG_KEY config_name, char *value, uint32_t len)
{
    uint32_t address;
    uint8_t buf[512] = {0};
    PARAM_DATA *pdata;

    memset( buf, 0x00, sizeof(buf));


    if( config_name == APP_CONFIG || config_name == SEC_KEY || \
        config_name == ICCID   || config_name == ACTIVE_FLAG )
    {
        address = CONFIG_INFO_ADDRESS;
    }
    else if( config_name == UPDATE_FLAG || config_name == FIRMWARE_INFO)
    {
        address = FIRMWARE_INFO_ADDRESS;

    } else
    {
        return -1;
    }
    // 读取flash数据
    read_flash( address, buf, sizeof(buf));

    if( config_name != FIRMWARE_INFO)
    {
        pdata = (PARAM_DATA *)buf;
    }

    switch (config_name) {
        case APP_CONFIG:
        {
            *value = pdata->app_config;
        }break;
        case SEC_KEY:
        {
            memcpy( value, pdata->sec_key, sizeof(pdata->sec_key));
        }break;
        case ICCID:
        {
            memcpy( value, pdata->iccid, sizeof(pdata->iccid));
        }break;
        case ACTIVE_FLAG:
        {
            *value = pdata->active_flag;
        }break;
            /////////////////////////
        case UPDATE_FLAG:
        case FIRMWARE_INFO:
        {
            memcpy( value, buf, len);
        }break;
        default:
            return -1;
    }

    return 0;
}


// 写flash
int write_flash( uint32_t* flash_address, uint32_t* data, uint32_t data_length, uint32_t flash_end_address)
{
    uint32_t index = 0;

#if defined(STM32F10X)

    __IO FLASH_Status FLASHStatus = FLASH_COMPLETE;

    FLASH_Unlock();
    //FLASH_OB_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    //debug("write flash:%d", *flash_address);
    // 若是写入1k的字节开始，擦除1k的数据
    if ( *flash_address % FLASH_PAGE_SIZE == 0 ) {
        FLASHStatus = FLASH_ErasePage( *flash_address);
        //debug("erase page");
    }

    for (index = 0; (index < data_length) && (*flash_address <= (flash_end_address - 4)) && (FLASHStatus == FLASH_COMPLETE); index++) {
        /* the operation will be done by word */
        if (FLASH_ProgramWord(*flash_address, *(uint32_t*)(data+index)) == FLASH_COMPLETE) {
            /* Check the written value */
            if (*(uint32_t*)*flash_address != *(uint32_t*)(data+index)) {
                /* Flash content doesn't match SRAM content */
                FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPRTERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
                FLASH_Lock();
                return -1; // 写入的数据和读出的数据不同
            }
            /* Increment FLASH destination address */
            *flash_address += 4;
        } else {
            /* Error occurred while writing data in Flash memory */
            FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPRTERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
            FLASH_Lock();
            return -1; // 写入失败
        }
        FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPRTERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
    }

    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPRTERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
    FLASH_Lock();

#endif


#if  defined(STM32F30X)
    __IO FLASH_Status FLASHStatus = FLASH_COMPLETE;

//    debug("test");
    FLASH_Unlock();
    //FLASH_OB_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    /*debug*/("write flash:%d", *flash_address);
    // 若是写入1k的字节开始，擦除1k的数据
    if ( *flash_address % FLASH_PAGE_SIZE == 0 ) {
        FLASHStatus = FLASH_ErasePage( *flash_address);
        /*debug*/("erase page");
    }

    for (index = 0; (index < data_length) && (*flash_address <= (flash_end_address - 4)) && (FLASHStatus == FLASH_COMPLETE); index++) {
        /* the operation will be done by word */
        if (FLASH_ProgramWord(*flash_address, *(uint32_t*)(data+index)) == FLASH_COMPLETE) {
            /* Check the written value */
            if (*(uint32_t*)*flash_address != *(uint32_t*)(data+index)) {
                /* Flash content doesn't match SRAM content */
                FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
                FLASH_Lock();
                return -1; // 写入的数据和读出的数据不同
            }
            /* Increment FLASH destination address */
            *flash_address += 4;
        } else {
            /* Error occurred while writing data in Flash memory */
            FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
            FLASH_Lock();
            return -1; // 写入失败
        }
        FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
    }

    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
    FLASH_Lock();
#endif
    return 0;
}

// 读取16位的flash数据
uint16_t flash_readhalfword(uint32_t addr)
{
    return *(volatile uint32_t*)addr;
}
// 读取16位的flash数据
uint32_t flash_readword(uint32_t addr)
{
    return *(volatile uint32_t*)addr;
}
// 读flash存储信息
void read_flash(uint32_t addr,uint8_t *pBuffer,uint16_t dataLen)
{
    uint16_t i;
    uint32_t data;

    for(i=0; i<dataLen; )
    {
        data = flash_readword(addr);	// 读取半字
        pBuffer[i] = data & 0xFF;
        i ++;
        if (i >= dataLen)
            break;

        pBuffer[i] = (data>>8) & 0xFF;;
        i ++;
        if (i >= dataLen)
            break;

        pBuffer[i] = (data>>16) & 0xFF;;
        i ++;
        if (i >= dataLen)
            break;

        pBuffer[i] = (data>>24) & 0xFF;;
        i ++;

        addr += 4;	 // 偏移2个字节
    }
}
// 擦出flash
void erase_config_flash(uint32_t address)
{

#if  defined(STM32F10X)
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPRTERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);

    FLASH_ErasePage(address);

    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPRTERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
    FLASH_Lock();
#endif  //#if  defined(STM32F30X)

#if  defined(STM32F30X)
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);

    FLASH_ErasePage(address);

    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR | FLASH_FLAG_EOP);
    FLASH_Lock();
#endif  //#if  defined(STM32F30X)
}
