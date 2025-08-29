/*
 * @Description:
 * @Version: 2.0
 * @Autor: ruog__
 * @Date: 2024-07-29 16:40:43
 * @LastEditors: ruog__
 * @LastEditTime: 2025-02-12 16:42:43
 */
/********************************************************************************************************
 * @file    tl_common.h
 *
 * @brief   This is the header file for tl_common
 *
 * @author  Zigbee Group
 * @date    2021
 *
 * @par     Copyright (c) 2021, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *
 *          Licensed under the Apache License, Version 2.0 (the "License");
 *          you may not use this file except in compliance with the License.
 *          You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 *          Unless required by applicable law or agreed to in writing, software
 *          distributed under the License is distributed on an "AS IS" BASIS,
 *          WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *          See the License for the specific language governing permissions and
 *          limitations under the License.
 *******************************************************************************************************/

#pragma once

#include "app_cfg.h"
#include "platform.h"

#include "common/types.h"
#include "common/compiler.h"
#include "common/static_assert.h"
#include "common/assert.h"
#include "common/bit.h"
#include "common/utility.h"
#include "common/utlist.h"
#include "common/list.h"
#include "common/string.h"
#include "common/tlPrintf.h"
#include "common/mempool.h"

#include "os/ev_poll.h"
#include "os/ev_buffer.h"
#include "os/ev_queue.h"
#include "os/ev_timer.h"
#include "os/ev.h"

#include "drivers/drv_hw.h"
#include "drivers/drv_radio.h"
#include "drivers/drv_gpio.h"
#include "drivers/drv_adc.h"
#include "drivers/drv_flash.h"
#include "drivers/drv_i2c.h"
#include "drivers/drv_spi.h"
#include "drivers/drv_pwm.h"
#include "drivers/drv_uart.h"
#include "drivers/drv_pm.h"
#include "drivers/drv_timer.h"
#include "drivers/drv_keyboard.h"
#include "drivers/drv_nv.h"
#include "drivers/drv_putchar.h"
#include "drivers/drv_usb.h"

#define MYFILE(x) strrchr(x, '/') ? strrchr(x, '/') + 1 : x

#define LOG_ENABLE
#ifdef LOG_ENABLE
#define LOG(fmt, ...) printf("[%s:%s:%d] " fmt "\r\n", MYFILE(__FILE__),__FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

//===================>串口通信协议部分<========
#define UART_HEAD 0xABCDABCD
#define UART_HEAD_T 0xCDABCDAB

#define UART_CMD_RK_B0 0xb0   // 指令   打开允许入网请求
#define UART_CMD_TL_B0 0xb1 // 回复
#define UART_CMD_RK_B2 0xb2   // 指令   查看当前是否允许入网
#define UART_CMD_TL_B2 0xb3 // 回复
#define UART_CMD_RK_B4 0xb4   // 指令   转发灯带的调色指令
#define UART_CMD_TL_B4 0xb5 // 回复

#define UART_CMD_RK_B6 0xb6   // 指令   转发开关的开关指令
#define UART_CMD_TL_B6 0xb7 // 回复

#define UART_CMD_TL__GW_D0 0xd0  //指令   转发灯带的调色指令
#define UART_CMD_TLNODE_D0 0xd1 //回复 

#define UART_CMD_TL__GW_D2 0xd2  //指令   转发开关的开关指令
#define UART_CMD_TLNODE_D2 0xd3 //回复 

#pragma(1)
//串口指令====
// 定义指令的结构体
typedef struct __rk_cmd
{
    u32 cmdHead;
    u8 cmd;
    u8 data[1];
} rk_cmd;

//zigbee指令
typedef struct __zigbee_dengdai_cmd
{
    u32 cmdHead;
    u8 cmd;
    u16 dstAddr;
    u8 r;
    u8 g;
    u8 b;
    u8 c;
    u8 w;
    u8 level;
    u8 onOff;
    u8 check;
} zigbee_dengdai_cmd;

//开关
typedef struct __zigbee_switch_cmd
{
    u32 cmdHead;
    u8 cmd;
    u16 dstAddr;
    u8 onOff; //1：开  2：关
    u8 YL[3];//预留
    u8 check;
} zigbee_switch_cmd;
#pragma(0)