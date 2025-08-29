/*
 * @Description: 
 * @Version: 2.0
 * @Autor: ruog__
 * @Date: 2024-12-13 13:57:32
 * @LastEditors: ruog__
 * @LastEditTime: 2024-12-13 17:32:29
 */


#ifndef __UART__
#define __UART__
#include <stdlib.h>  
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>   
#include <sys/mman.h>
#include <linux/i2c.h>   
#include <linux/i2c-dev.h>
#include <errno.h>
#include "drv_common.h"

#define UART_HEAD 0xABCDABCD

#define UART_CMD_RK_B0 0xb0   // 指令   打开允许入网请求
#define UART_CMD_TL_B0 0xb1 // 回复
#define UART_CMD_RK_B2 0xb2   // 指令   查看当前是否允许入网
#define UART_CMD_TL_B2 0xb3 // 回复

#pragma (1)
typedef struct __rk_cmd
{
    u32 cmdHead;
    u8 cmd;
    u8 data[1];
} rk_cmd;
#pragma ()
void open_join_network();
void rs422_send(unsigned char id);
int set_opt(int fd, int nSpeed, int nBits, char nEvent, int nStop);
void rs422_loopback(unsigned char id, unsigned int value);
void rs422_selfinc(unsigned char id, int pktCnt);
void rs422_selfinc_tx_delay(unsigned char id, int pktCnt, unsigned int delay);
void rs422_savefile(unsigned char id, int maxPkt);
void rs422_savefile_rx_delay(unsigned char id, int maxPkt, unsigned int delay);

void send_zigbee_to_light();
#endif