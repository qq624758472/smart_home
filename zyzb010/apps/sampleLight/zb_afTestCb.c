/********************************************************************************************************
 * @file    zb_afTestCb.c
 *
 * @brief   This is the source file for zb_afTestCb
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

#if (__PROJECT_TL_DIMMABLE_LIGHT__)

/**********************************************************************
 * INCLUDES
 */
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "sampleLight.h"
#if ZBHCI_EN
#include "zbhci.h"
#endif

extern void sampleLight_updateColor(void);

#if AF_TEST_ENABLE
/**********************************************************************
 * LOCAL CONSTANTS
 */

/**********************************************************************
 * TYPEDEFS
 */

/**********************************************************************
 * LOCAL FUNCTIONS
 */

/**********************************************************************
 * GLOBAL VARIABLES
 */

/**********************************************************************
 * LOCAL VARIABLES
 */
u16 g_afTest_rcvReqCnt = 0;

/**********************************************************************
 * FUNCTIONS
 */
static void afTest_testReqPrc(apsdeDataInd_t *pApsdeInd)
{
    epInfo_t dstEp;
    int i = 0;
    TL_SETSTRUCTCONTENT(dstEp, 0);

    dstEp.dstEp = pApsdeInd->indInfo.src_ep;
    dstEp.profileId = pApsdeInd->indInfo.profile_id;
    dstEp.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dstEp.dstAddr.shortAddr = pApsdeInd->indInfo.src_short_addr;
    // LOG("recv ZCL_CLUSTER_TELINK_SDK_TEST_REQ cmd srcMacAddr:0x%x",pApsdeInd->indInfo.srcMacAddr);
    // for (i = 0; i < 8; i++)
    // {
    //     LOG("%d is :[0x%x]",i, pApsdeInd->indInfo.src_ext_addr[i]);
    // }
    u8 dataLen = 50;
    u8 *pBuf = (u8 *)ev_buf_allocate(dataLen);
    if (pBuf)
    {
        u8 *pData = pBuf;

        *pData++ = LO_UINT16(g_afTest_rcvReqCnt);
        *pData++ = HI_UINT16(g_afTest_rcvReqCnt);

        for (u8 i = 0; i < dataLen - 2; i++)
        {
            *pData++ = i;
        }

        u8 apsCnt = 0;
#if ZBHCI_EN
        zbhciTx(ZCL_CLUSTER_TELINK_SDK_TEST_RSP, pApsdeInd->asduLen, (u8 *)pApsdeInd->asdu);
#else
        af_dataSend(pApsdeInd->indInfo.dst_ep, &dstEp, ZCL_CLUSTER_TELINK_SDK_TEST_RSP, dataLen, pBuf, &apsCnt);
#endif

        ev_buf_free(pBuf);
    }
}

static void afTest_testClearReqPrc(apsdeDataInd_t *pApsdeInd)
{
    epInfo_t dstEp;
    TL_SETSTRUCTCONTENT(dstEp, 0);

    dstEp.dstEp = pApsdeInd->indInfo.src_ep;
    dstEp.profileId = pApsdeInd->indInfo.profile_id;
    dstEp.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dstEp.dstAddr.shortAddr = pApsdeInd->indInfo.src_short_addr;

    u8 st = SUCCESS;

    u8 apsCnt = 0;
    af_dataSend(pApsdeInd->indInfo.dst_ep, &dstEp, ZCL_CLUSTER_TELINK_SDK_TEST_CLEAR_RSP, 1, &st, &apsCnt);
}

u8 afTest_ctr_dengdai(apsdeDataInd_t *pApsdeInd)
{
    u8 ret = 0;
    tiaoSeCMD *pData = pApsdeInd->asdu;
    // 调试
    LOG("asduLen:%d,pData->head:0x%x", pApsdeInd->asduLen,pData->head);
    for (int i = 0; i < pApsdeInd->asduLen; i++)
    {
        printf(" 0x%x", pApsdeInd->asdu[i]);
    }
    printf("\r\n");

    if (pData->head != UART_HEAD)
    {
        ret = 1;
        return ret;
    }
    // if (pData->check != 0xff)
    // {
    //     ret = 2;
    //     return ret;
    // }
    if (pData->cmd != 0xd0) // 是否是调色指令
    {
        ret = 3;
        return ret;
    }
    
    
    zcl_lightColorCtrlAttr_t *pColor = zcl_colorAttrGet();
    pColor->R = pData->r;
    pColor->G = pData->g;
    pColor->B = pData->b;
    pColor->C = pData->c;
    pColor->W = pData->w;
    pColor->LEVEL = pData->level;
    pColor->onOFF = pData->onOff;
    hwLight_colorUpdate_HSV2RGB_MY(pColor->R, pColor->G, pColor->B, pColor->C,pColor->W, pColor->LEVEL);
    // hwLight_colorUpdate_colorTemperature(pColor->colorTemperatureMireds, pLevel->curLevel);
    return ret;
}

// 调色控制
void afTest_start_tiaose(apsdeDataInd_t *pApsdeInd)
{
    epInfo_t dstEp;
    u8 isOK = 0; // 默认成功
    TL_SETSTRUCTCONTENT(dstEp, 0);

    // 进行调色
    isOK = afTest_ctr_dengdai(pApsdeInd);
    if (isOK != 0)
    {
        LOG("error: afTest_ctr_dengdai");
    }
    // 进行回复
    dstEp.dstEp = pApsdeInd->indInfo.src_ep;
    dstEp.profileId = pApsdeInd->indInfo.profile_id;
    dstEp.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dstEp.dstAddr.shortAddr = pApsdeInd->indInfo.src_short_addr;
    // LOG("node shortAddr: 0x%x, mac: 0x%x", dstEp.dstAddr.shortAddr, pApsdeInd->indInfo.srcMacAddr);

    u8 dataLen = sizeof(tiaoSeCMDRsp);
    u8 *pBuf = (u8 *)ev_buf_allocate(dataLen);
    if (pBuf)
    {
        tiaoSeCMDRsp *pData = pBuf;
        pData->head = 0xabcdabcd;
        pData->cmd = 0xd1;
        pData->isOK = isOK;
        pData->check = 0xff;
        u8 apsCnt = 0;
        af_dataSend(pApsdeInd->indInfo.dst_ep, &dstEp, ZCL_CLUSTER_TELINK_SDK_LIGHT_COLOR_RSP, dataLen, pBuf, &apsCnt);

        ev_buf_free(pBuf);
    }
}

void lsh_test_handle(void *arg)
{
    LOG("afTest_rx_handler");
    apsdeDataInd_t *pApsdeInd = (apsdeDataInd_t *)arg;

    switch (pApsdeInd->indInfo.cluster_id)
    {
    case ZCL_CLUSTER_TELINK_SDK_TEST_CLEAR_REQ:
        g_afTest_rcvReqCnt = 0;
        afTest_testClearReqPrc(pApsdeInd);
        break;
    case ZCL_CLUSTER_TELINK_SDK_TEST_REQ:
        g_afTest_rcvReqCnt++;
        afTest_testReqPrc(pApsdeInd);
        break;
    case ZCL_CLUSTER_TELINK_SDK_TEST_RSP:

        break;
    case ZCL_CLUSTER_TELINK_SDK_LIGHT_COLOR_REQ: // 调色指令。
        afTest_start_tiaose(pApsdeInd);          // 开始调色，并且回复rsp指令
        break;
    default:
        break;
    }

    /* Must be free here. */
    ev_buf_free((u8 *)arg);
}

void afTest_rx_handler(void *arg)
{
    TL_SCHEDULE_TASK(lsh_test_handle, arg); // 将任务推送到任务列表
}

void afTest_dataSendConfirm(void *arg)
{
    apsdeDataConf_t *pApsDataCnf = (apsdeDataConf_t *)arg;
    LOG("addr_short: %d", pApsDataCnf->dstAddr.addr_short);
}

#endif /* AF_TEST_ENABLE */
#endif /* __PROJECT_TL_DIMMABLE_LIGHT__ */
