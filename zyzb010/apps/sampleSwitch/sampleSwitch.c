/********************************************************************************************************
 * @file    sampleSwitch.c
 *
 * @brief   This is the source file for sampleSwitch
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

#if (__PROJECT_TL_SWITCH__)

/**********************************************************************
 * INCLUDES
 */
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "sampleSwitch.h"
#include "app_ui.h"
#if ZBHCI_EN
#include "zbhci.h"
#endif

/**********************************************************************
 * LOCAL CONSTANTS
 */

/**********************************************************************
 * TYPEDEFS
 */

/**********************************************************************
 * GLOBAL VARIABLES
 */
app_ctx_t g_switchAppCtx;

#ifdef ZCL_OTA
extern ota_callBack_t sampleSwitch_otaCb;

// running code firmware information
ota_preamble_t sampleSwitch_otaInfo = {
    .fileVer = FILE_VERSION,
    .imageType = IMAGE_TYPE,
    .manufacturerCode = MANUFACTURER_CODE_TELINK,
};
#endif

// Must declare the application call back function which used by ZDO layer
const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf,           // start device cnf cb
    NULL,                         // reset cnf cb
    NULL,                         // device announce indication cb
    sampleSwitch_leaveIndHandler, // leave ind cb
    sampleSwitch_leaveCnfHandler, // leave cnf cb
    NULL,                         // nwk update ind cb
    NULL,                         // permit join ind cb
    NULL,                         // nlme sync cnf cb
    NULL,                         // tc join ind cb
    NULL,                         // tc detects that the frame counter is near limit
};

/**
 *  @brief Definition for BDB finding and binding cluster
 */
u16 bdb_findBindClusterList[] =
    {
        ZCL_CLUSTER_GEN_ON_OFF,
};

/**
 *  @brief Definition for BDB finding and binding cluster number
 */
#define FIND_AND_BIND_CLUSTER_NUM (sizeof(bdb_findBindClusterList) / sizeof(bdb_findBindClusterList[0]))

/**
 *  @brief Definition for bdb commissioning setting
 */
bdb_commissionSetting_t g_bdbCommissionSetting = {
    .linkKey.tcLinkKey.keyType = SS_GLOBAL_LINK_KEY,
    .linkKey.tcLinkKey.key = (u8 *)tcLinkKeyCentralDefault, // can use unique link key stored in NV

    .linkKey.distributeLinkKey.keyType = MASTER_KEY,
    .linkKey.distributeLinkKey.key = (u8 *)linkKeyDistributedMaster, // use linkKeyDistributedCertification before testing

    .linkKey.touchLinkKey.keyType = MASTER_KEY,
    .linkKey.touchLinkKey.key = (u8 *)touchLinkKeyMaster, // use touchLinkKeyCertification before testing

#if TOUCHLINK_SUPPORT
    .touchlinkEnable = 1, /* enable touch-link */
#else
    .touchlinkEnable = 0, /* disable touch-link */
#endif
    .touchlinkChannel = DEFAULT_CHANNEL, /* touch-link default operation channel for target */
    .touchlinkLqiThreshold = 0xA0,       /* threshold for touch-link scan req/resp command */
};

#if PM_ENABLE
/**
 *  @brief Definition for wakeup source and level for PM
 */
drv_pm_pinCfg_t g_switchPmCfg[] = {
    {BUTTON1,
     PM_WAKEUP_LEVEL},
    {BUTTON2,
     PM_WAKEUP_LEVEL}};
#endif
/**********************************************************************
 * LOCAL VARIABLES
 */

/**********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
 * @fn      stack_init
 *
 * @brief   This function initialize the ZigBee stack and related profile. If HA/ZLL profile is
 *          enabled in this application, related cluster should be registered here.
 *
 * @param   None
 *
 * @return  None
 */
void stack_init(void)
{
    /* Initialize ZB stack */
    zb_init();

    /* Register stack CB */
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

//lsh add
void app_switch_rx_handler(void *pData)
{
    LOG("read data");
}

//lsh
u16 g_afTest_rcvReqCnt = 0;
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


u8 afTest_ctr_switch(apsdeDataInd_t *pApsdeInd)
{
    u8 ret = 0;
    switchCMD *pData = pApsdeInd->asdu;
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
    if (pData->cmd != 0xd2) // 是否是开关指令
    {
        ret = 3;
        return ret;
    }
    
    //根据开关的指令，设置LED1(也就是输出信号的)的信号
    if (pData->onOff == 0x01)
    {
        led_off(LED1);//低电平是打开开关
    }
    else
    {
        led_on(LED1);//高电平是关闭开关
    }
    
    return ret;
}

void afTest_testReqPrc(apsdeDataInd_t *pApsdeInd)
{

    afTest_ctr_switch(pApsdeInd);//进行开关控制

    epInfo_t dstEp;
    TL_SETSTRUCTCONTENT(dstEp, 0);

    dstEp.dstEp = pApsdeInd->indInfo.src_ep;
    dstEp.profileId = pApsdeInd->indInfo.profile_id;
    dstEp.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dstEp.dstAddr.shortAddr = pApsdeInd->indInfo.src_short_addr;
    LOG("dstAddrShortAddr = 0x%x,srtAddrShortAddr=0x%x, recvData:0x%x",pApsdeInd->indInfo.src_short_addr,pApsdeInd->indInfo.dst_addr,pApsdeInd->asdu[0]);


    u8 dataLen = 11;
    u8 *pBuf = (u8 *)ev_buf_allocate(dataLen);
    if (pBuf)
    {
        switchCMDRsp *pData = pBuf;
        pData->head = 0xabcdabcd;
        pData->cmd = 0xd3;
        pData->isOK = 0;//0：是  1：否
        pData->check = 0xff;
        u8 apsCnt = 0;
        af_dataSend(pApsdeInd->indInfo.dst_ep, &dstEp, ZCL_CLUSTER_TELINK_SDK_SWITCH_ONOFF_RSP, dataLen, pBuf, &apsCnt);

        ev_buf_free(pBuf);
    }
}
void afTest_rx_handler(void *arg)
{
    LOG("afTest_rx_handler");
#if 1
    apsdeDataInd_t *pApsdeInd = (apsdeDataInd_t *)arg;

    switch (pApsdeInd->indInfo.cluster_id)
    {
    case ZCL_CLUSTER_TELINK_SDK_TEST_CLEAR_REQ:
        g_afTest_rcvReqCnt = 0;
        afTest_testClearReqPrc(pApsdeInd);
        break;
    case ZCL_CLUSTER_TELINK_SDK_TEST_REQ://通用指令，所有的模块都应该有这个用来接收广播消息
        // g_afTest_rcvReqCnt++;
        // afTest_testReqPrc(pApsdeInd);
        break;
    case ZCL_CLUSTER_TELINK_SDK_SWITCH_ONOFF_REQ:
        g_afTest_rcvReqCnt++;
        afTest_testReqPrc(pApsdeInd);
        break;
    case ZCL_CLUSTER_TELINK_SDK_TEST_RSP:

        break;
    default:
        break;
    }

    /* Must be free here. */
    ev_buf_free((u8 *)arg);
#endif
}

/*********************************************************************
 * @fn      user_app_init
 *
 * @brief   This function initialize the application(Endpoint) information for this node.
 *
 * @param   None
 *
 * @return  None
 */
void user_app_init(void)
{
#ifdef ZCL_POLL_CTRL
    af_powerDescPowerModeUpdate(POWER_MODE_RECEIVER_COMES_PERIODICALLY);
#else
    af_powerDescPowerModeUpdate(POWER_MODE_RECEIVER_COMES_WHEN_STIMULATED);
#endif

    /* Initialize ZCL layer */
    /* Register Incoming ZCL Foundation command/response messages */
    zcl_init(sampleSwitch_zclProcessIncomingMsg);

    /* register endPoint */
    af_endpointRegister(SAMPLE_SWITCH_ENDPOINT, (af_simple_descriptor_t *)&sampleSwitch_simpleDesc, zcl_rx_handler, NULL);
    // af_endpointRegister(SAMPLE_SWITCH_ENDPOINT, (af_simple_descriptor_t *)&sampleSwitch_simpleDesc, app_switch_rx_handler, NULL);
#if 1 // lsh add AF_TEST_ENABLE
    /* A sample of AF data handler. */
    af_endpointRegister(SAMPLE_MY_ENDPOINT, (af_simple_descriptor_t *)&sampleTestDesc, afTest_rx_handler, NULL);
#endif
    /* Register ZCL specific cluster information */
    zcl_register(SAMPLE_SWITCH_ENDPOINT, SAMPLE_SWITCH_CB_CLUSTER_NUM, (zcl_specClusterInfo_t *)g_sampleSwitchClusterList);

#ifdef ZCL_OTA
    ota_init(OTA_TYPE_CLIENT, (af_simple_descriptor_t *)&sampleSwitch_simpleDesc, &sampleSwitch_otaInfo, &sampleSwitch_otaCb);
#endif
}

void led_init(void)
{
    light_init();
}

void app_task(void)
{
    app_key_handler();

    if (bdb_isIdle())
    {
#if PM_ENABLE
        if (!g_switchAppCtx.keyPressed)
        {
            drv_pm_lowPowerEnter();
        }
#endif
    }
}

static void sampleSwitchSysException(void)
{
    SYSTEM_RESET();
    // light_on();
    // while(1);
}

/*********************************************************************
 * @fn      user_init
 *
 * @brief   User level initialization code.
 *
 * @param   isRetention - if it is waking up with ram retention.
 *
 * @return  None
 */
void user_init(bool isRetention)
{
    /* Initialize LEDs*/
    led_init();

#if PA_ENABLE
    rf_paInit(PA_TX, PA_RX);
#endif

#if ZBHCI_EN
    zbhciInit();
#endif

#if PM_ENABLE
    drv_pm_wakeupPinConfig(g_switchPmCfg, sizeof(g_switchPmCfg) / sizeof(drv_pm_pinCfg_t));
#endif

    if (!isRetention)
    {
        /* Initialize Stack */
        stack_init();

        /* Initialize user application */
        user_app_init();

        /* Register except handler for test */
        sys_exceptHandlerRegister(sampleSwitchSysException);

        /* User's Task */
#if ZBHCI_EN
        ev_on_poll(EV_POLL_HCI, zbhciTask);
#endif
        ev_on_poll(EV_POLL_IDLE, app_task);

        /* Load the pre-install code from flash */
        if (bdb_preInstallCodeLoad(&g_switchAppCtx.tcLinkKey.keyType, g_switchAppCtx.tcLinkKey.key) == RET_OK)
        {
            g_bdbCommissionSetting.linkKey.tcLinkKey.keyType = g_switchAppCtx.tcLinkKey.keyType;
            g_bdbCommissionSetting.linkKey.tcLinkKey.key = g_switchAppCtx.tcLinkKey.key;
        }

        bdb_findBindMatchClusterSet(FIND_AND_BIND_CLUSTER_NUM, bdb_findBindClusterList);

        /* Initialize BDB */
        u8 repower = drv_pm_deepSleep_flag_get() ? 0 : 1;
        bdb_init((af_simple_descriptor_t *)&sampleSwitch_simpleDesc, &g_bdbCommissionSetting, &g_zbDemoBdbCb, repower);
        LOG("Initial success");
    }
    else
    {
        /* Re-config phy when system recovery from deep sleep with retention */
        mac_phyReconfig();
    }
}

#endif /* __PROJECT_TL_SWITCH__ */
