/********************************************************************************************************
 * @file    hci_uart.c
 *
 * @brief   This is the source file for hci_uart
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

#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"

#if (ZBHCI_EN && ZBHCI_UART)
#include "zb_task_queue.h"
#include "../zbhci.h"
// #include "../../zigbee/af/zb_af.h"

#define UART_TX_BUF_SIZE 64
#define UART_RX_BUF_SIZE 64

// begin
typedef struct
{
    u32 dataLen;
    u8 dataPayload[1];
} uart_rxData_t;

__attribute__((aligned(4))) u8 uartTxBuf[UART_TX_BUF_SIZE] = {0};
__attribute__((aligned(4))) u8 uartRxBuf[UART_RX_BUF_SIZE] = {0};

void swapEndian(unsigned int *value)
{
    // 交换字节顺序
    *value = ((*value >> 24) & 0x000000FF) | // 移动最高字节到最低字节
             ((*value >> 8) & 0x0000FF00) |  // 移动次高字节到次低字节
             ((*value << 8) & 0x00FF0000) |  // 移动次低字节到次高字节
             ((*value << 24) & 0xFF000000);  // 移动最低字节到最高字节
}

// extern void send_zigbee_data();

void uart_send_zigbee_tiaose(zigbee_dengdai_cmd *tmp)
{
    // lsh  按键发送数测试
    epInfo_t dstEpInfo;
    TL_SETSTRUCTCONTENT(dstEpInfo, 0);

    dstEpInfo.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dstEpInfo.dstEp = 2;//SAMPLE_TEST_ENDPOINT;
    dstEpInfo.dstAddr.shortAddr = tmp->dstAddr; 
    dstEpInfo.profileId = HA_PROFILE_ID;
    dstEpInfo.txOptions = 0;
    dstEpInfo.radius = 0;
    u8 buf[15] = {0};
    memcpy(buf, tmp, sizeof(zigbee_dengdai_cmd));
    u8 seqNum = 0;
    //SAMPLE_GW_ENDPOINT 1
    af_dataSend(1, &dstEpInfo, ZCL_CLUSTER_TELINK_SDK_LIGHT_COLOR_REQ, 15, buf, &seqNum);
    LOG("af_dataSend addr:0x%x success\r\n",tmp->dstAddr);
}

void uart_send_zigbee_switch(zigbee_switch_cmd *tmp)
{
    // lsh  按键发送数测试
    epInfo_t dstEpInfo;
    TL_SETSTRUCTCONTENT(dstEpInfo, 0);

    dstEpInfo.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dstEpInfo.dstEp = 2;//SAMPLE_TEST_ENDPOINT;
    dstEpInfo.dstAddr.shortAddr = tmp->dstAddr; 
    dstEpInfo.profileId = HA_PROFILE_ID;
    dstEpInfo.txOptions = 0;
    dstEpInfo.radius = 0;
    u8 buf[12] = {0};
    memcpy(buf, tmp, sizeof(zigbee_switch_cmd));
    u8 seqNum = 0;
    //SAMPLE_GW_ENDPOINT 1
    af_dataSend(1, &dstEpInfo, ZCL_CLUSTER_TELINK_SDK_SWITCH_ONOFF_REQ, 12, buf, &seqNum);
    LOG("af_dataSend addr:0x%x success\r\n",tmp->dstAddr);
}

// 串口接收到数据的解析程序
void uart_data_handler(void *arg)
{

    /*
     * the format of the uart rx data: length(4 Bytes) + payload
     *
     * */
    u8 ret[4] = {0, 0, 0, 0};
    u8 st = SUCCESS;
    u8 len = 0; // 回复的数据长度。

    uart_rxData_t *rxData = (uart_rxData_t *)uartRxBuf; // 前4个字节发送时自己组包的。  接收的时候也会自己加个长度的头。
    zbhci_msg_t *msg = (zbhci_msg_t *)(rxData->dataPayload);
    if (rxData->dataLen == 0)
    {
        st = ZBHCI_MSG_STATUS_UART_EXCEPT;
    }

    if (rxData->dataLen > (UART_RX_BUF_SIZE - 4))
    {
        st = ZBHCI_MSG_STATUS_MSG_OVERFLOW;
    }

    LOG("recv len = %d\r\n", rxData->dataLen);
    for (int i = 4; i < rxData->dataLen + 4; i++)
    {
        printf("%x ", uartRxBuf[i]);
    }
    // 判断包头和包尾是否正确，不正确丢弃该指令
    rk_cmd *p = rxData->dataPayload;
    // swapEndian(&p->cmdHead);
    LOG("recv cmd 0x%x\r\n", p->cmd);
    if (p->cmdHead != UART_HEAD)
    {
        LOG("cmd head is not 0xabcdacd :0x%x\r\n", p->cmdHead);
        return;
    }

    memset(uartTxBuf, 0x0, UART_RX_BUF_SIZE);
    rk_cmd *p_send = uartTxBuf;
    p_send->cmdHead = UART_HEAD;

    switch (p->cmd)
    {
    case UART_CMD_RK_B0: // 控制打开协调器允许入网
    {
        zb_nlmePermitJoiningRequest(180); // 打开允许加入网络，180秒。
        p_send->cmd = UART_CMD_TL_B0;
        len = 10;
    }
    break;
    case UART_CMD_RK_B2: // 查看当前协调器的允许设备入网状态
    {
        u8 assocPermit = 0;
        assocPermit = zb_getMacAssocPermit(); // 获取本节点的入网允许状态
        assocPermit = zb_getMacAssocPermit(); // 0:不允许， ！0 ：允许
        p_send->cmd = UART_CMD_TL_B2;
        p_send->data[0] = assocPermit;
        len = 11;
    }
    break;
    case UART_CMD_RK_B4: // 转发的调色指令
    {
        p_send->cmd = UART_CMD_TL_B4;
        p_send->data[0] = 1;//成功
        len = 11;
        // 开始发送zigbee消息,调色指令
        zigbee_dengdai_cmd tmp;
        memset(&tmp, 0, sizeof(zigbee_dengdai_cmd));
        tmp.cmdHead = UART_HEAD;
        tmp.cmd = UART_CMD_TL__GW_D0;
        tmp.dstAddr = ((zigbee_dengdai_cmd *)p)->dstAddr;
        tmp.r = ((zigbee_dengdai_cmd *)p)->r;
        tmp.g = ((zigbee_dengdai_cmd *)p)->g;
        tmp.b = ((zigbee_dengdai_cmd *)p)->b;
        tmp.c = ((zigbee_dengdai_cmd *)p)->c;
        tmp.w = ((zigbee_dengdai_cmd *)p)->w;
        tmp.level = ((zigbee_dengdai_cmd *)p)->level;
        tmp.onOff = ((zigbee_dengdai_cmd *)p)->onOff;
        tmp.check = 0xff;
        uart_send_zigbee_tiaose(&tmp);
    }
    break;
    case UART_CMD_RK_B6: // 转发的开关指令
    {
        p_send->cmd = UART_CMD_TL_B6;
        p_send->data[0] = 1;//成功
        len = 11;
        // 开始发送zigbee消息,开关指令
        zigbee_switch_cmd tmp;
        memset(&tmp, 0, sizeof(zigbee_switch_cmd));
        tmp.cmdHead = UART_HEAD;
        tmp.cmd = UART_CMD_TL__GW_D2;
        tmp.dstAddr = ((zigbee_switch_cmd *)p)->dstAddr;
        tmp.onOff = ((zigbee_switch_cmd *)p)->onOff;
        tmp.check = 0xff;
        uart_send_zigbee_switch(&tmp);
    }
    break;
    case 0xFB: // 恢复出厂
    {
        zb_factoryReset();
    }
    break;
        // case 1://控制GW广播zigbee消息
        // {
        //     send_zigbee_data();
        //     p_send->cmd = 0x11;
        //     len = 6;
        // }
        // break;

    default:
        LOG("recv cmd not support\r\n");
    }
    uart_txMsg(ZBHCI_CMD_ZCL_ATTR_WRITE, len, uartTxBuf); // 前6个字节是包头，最后一个字节是校验和。
#if 0
    if (st == SUCCESS)
    {
        if (msg->startFlag == ZBHCI_MSG_START_FLAG)
        {
            msg->msgType16H = rxData->dataPayload[1];
            msg->msgType16L = rxData->dataPayload[2];
            msg->msgLen16H = rxData->dataPayload[3];
            msg->msgLen16L = rxData->dataPayload[4];

            /* check the start flag */
            u16 pktLen = (msg->msgLen16H << 8) | msg->msgLen16L;
            if ((pktLen + ZBHCI_MSG_HDR_LEN) == rxData->dataLen)
            {
                /* check the end flag */
                if (rxData->dataPayload[rxData->dataLen - 1] != ZBHCI_MSG_END_FLAG)
                {
                    st = ZBHCI_MSG_STATUS_ERROR_END_CHAR;
                }
            }
            else
            {
                st = ZBHCI_MSG_STATUS_BAD_MSG;
            }
        }
        else
        {
            st = ZBHCI_MSG_STATUS_ERROR_START_CHAR;
        }
    }

    u16 pktLen = (msg->msgLen16H << 8) | msg->msgLen16L;
    u16 msgType = (msg->msgType16H << 8) + msg->msgType16L;

    if (st == SUCCESS)
    {
        u8 crc8 = crc8Calculate(msgType, pktLen, msg->pData);
        if ((msgType == ZBHCI_CMD_OTA_START_REQUEST) || (msgType == ZBHCI_CMD_OTA_BLOCK_RESPONSE))
        {
            if (crc8 != msg->checkSum)
            {
                st = ZBHCI_MSG_STATUS_CRC_ERROR;
            }
        }
    }

    if (st == SUCCESS)
    {
        rxData->dataLen = 0;

        zbhciCmdHandler(msgType, pktLen, msg->pData);
    }
    else
    {
        rxData->dataLen = 0;

        if ((st == ZBHCI_MSG_STATUS_UART_EXCEPT) || (st == ZBHCI_MSG_STATUS_MSG_OVERFLOW) || (st == ZBHCI_MSG_STATUS_ERROR_START_CHAR))
        {
            msg->msgType16H = 0xff;
            msg->msgType16L = 0xff;
        }

        ret[0] = msg->msgType16H;
        ret[1] = msg->msgType16L;
        ret[2] = st;
        ret[3] = 0;

        zbhciTx(ZBHCI_CMD_ACKNOWLEDGE, 4, ret);
    }
#endif
}

void uartRcvHandler(void)
{

    TL_SCHEDULE_TASK(uart_data_handler, uartRxBuf);
}

void uart_txMsg(u16 u16Type, u16 u16Length, u8 *pu8Data)
{
    int n;
    // LOG("send len = %d\r\n", u16Length);
    // u8 crc8 = crc8Calculate(u16Type, u16Length, pu8Data);

    // u8 *p = uartTxBuf;
    // *p++ = 0x55;
    // *p++ = (u16Type >> 8) & 0xff;
    // *p++ = (u16Type >> 0) & 0xff;
    // *p++ = (u16Length >> 8) & 0xff;
    // *p++ = (u16Length >> 0) & 0xff;
    // *p++ = crc8;
    // for (n = 0; n < u16Length; n++)
    // {
    //     *p++ = pu8Data[n];
    // }
    // *p++ = 0xAA;

    drv_uart_tx_start(uartTxBuf, u16Length);
}

// 串口通信接口，接收和发送测试  lsh
void hci_uart_init(void)
{
    UART_PIN_CFG();
    drv_uart_init(9600, uartRxBuf, UART_RX_BUF_SIZE, uartRcvHandler);
}

#endif
