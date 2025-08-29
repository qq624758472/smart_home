/********************************************************************************************************
 * @file    sampleGateway.c
 *
 * @brief   This is the source file for sampleGateway
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

#if (__PROJECT_TL_GW__)

/**********************************************************************
 * INCLUDES
 */
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "gp.h"
#include "sampleGateway.h"
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
app_ctx_t g_appGwCtx;

//lsh
u16 clientShortAddr[64]; // 最大支持64个节点   子设备节点的短地址
u8 clientNum;  //子设备节点数量

#if ZBHCI_EN
extern mac_appIndCb_t macAppIndCbList;
#endif

#ifdef ZCL_OTA
// running code firmware information
ota_preamble_t sampleGW_otaInfo = {
    .fileVer = FILE_VERSION,
    .imageType = IMAGE_TYPE,
    .manufacturerCode = MANUFACTURER_CODE_TELINK,
};
#endif

// Must declare the application call back function which used by ZDO layer
const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf,                // start device cnf cb
    NULL,                              // reset cnf cb
    sampleGW_devAnnHandler,            // device announce indication cb
    sampleGW_leaveIndHandler,          // leave ind cb
    sampleGW_leaveCnfHandler,          // leave cnf cb
    sampleGW_nwkUpdateIndicateHandler, // nwk update ind cb
    NULL,                              // permit join ind cb
    NULL,                              // nlme sync cnf cb
    sampleGW_tcJoinIndHandler,         // tc join ind cb
    sampleGW_tcFrameCntReachedHandler, // tc detects that the frame counter is near limit
};

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

    .touchlinkEnable = 0, // disable touch link for coordinator
};

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
 * 此函数用于初始化ZigBee堆栈和相关配置文件。如果在此应用程序中启用了HA/ZLL配置文件，则应在此处注册相关集群。
 *
 * @param   None
 *
 * @return  None
 */
void stack_init(void)
{
    zb_init();

#if ZBHCI_EN
    zb_macCbRegister((mac_appIndCb_t *)&macAppIndCbList);
#endif
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

// lsh uart test
typedef struct
{
    u32 dataLen;
    u8 dataPayload[1];
} uart_rxData_t;

u8 lshTest_uartRxBuf[8] = {0};
void lsh_uart_test_uartRcvHandler()
{
    u8 sendData = 0xbb;
    /*
     * the format of the uart rx data: length(4 Bytes) + payload
     *
     * */
    uart_rxData_t *rxData = (uart_rxData_t *)lshTest_uartRxBuf;

    if (rxData->dataPayload[0] == 0)
    {
        sendData = 0xef;
        drv_uart_tx_start(&sendData, 1);
    }
    else
        drv_uart_tx_start(&sendData, 1);
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
    af_nodeDescManuCodeUpdate(MANUFACTURER_CODE_TELINK);

    /* Initialize ZCL layer */
    /* Register Incoming ZCL Foundation command/response messages  注册传入的ZCL Foundation命令/响应消息*/
    zcl_init(sampleGW_zclProcessIncomingMsg);

    /* Register endPoint zcl_rx_handler：接收处理函数，sampleGW_dataSendConfirm：发送确认回调函数*/
    af_endpointRegister(SAMPLE_GW_ENDPOINT, (af_simple_descriptor_t *)&sampleGW_simpleDesc, zcl_rx_handler, sampleGW_dataSendConfirm);
#if AF_TEST_ENABLE
    /* A sample of AF data handler. */
    // af_endpointRegister类似绑定端口号
    af_endpointRegister(SAMPLE_TEST_ENDPOINT, (af_simple_descriptor_t *)&sampleTestDesc, afTest_rx_handler, afTest_dataSendConfirm);
#endif

    /* Register ZCL specific cluster information 注册ZCL特定集群信息*/
    zcl_register(SAMPLE_GW_ENDPOINT, SAMPLE_GW_CB_CLUSTER_NUM, (zcl_specClusterInfo_t *)g_sampleGwClusterList);

#ifdef ZCL_GREEN_POWER
    gp_init();
#endif

#ifdef ZCL_OTA
    ota_init(OTA_TYPE_SERVER, (af_simple_descriptor_t *)&sampleGW_simpleDesc, &sampleGW_otaInfo, NULL);
#endif


    //lsh add
    memset(clientShortAddr, 0, sizeof(u16) * 64);
    clientNum = 0;
}

void led_init(void)
{
    // led_off(LED_PERMIT);
    light_init();
}

void app_task(void)
{
    static bool assocPermit = 0;
    if (assocPermit != zb_getMacAssocPermit())
    {
        assocPermit = zb_getMacAssocPermit();
        if (assocPermit)
        {
            LOG("assoc permit");
            // led_on(LED_PERMIT);
        }
        else
        {
            LOG("assoc NOT permit");
            // led_off(LED_PERMIT);
        }
    }

    if (BDB_STATE_GET() == BDB_STATE_IDLE)
    {
        // LOG("");
        app_key_handler();
    }
}

static void sampleGwSysException(void)
{
    SYSTEM_RESET();

    //	while(1){
    //		gpio_toggle(LED_POWER);
    //		WaitMs(100);
    //	}
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
    (void)isRetention;

#if defined(MCU_CORE_8258) || defined(MCU_CORE_8278) || defined(MCU_CORE_B91)
    extern u8 firmwareCheckWithUID(void);
    if (firmwareCheckWithUID())
    {
        while (1)
            ;
    }
#endif

    /* Initialize LEDs*/
    //led_init();

#if PA_ENABLE
    /* external RF PA used */
    rf_paInit(PA_TX, PA_RX);
#endif

    /* Initialize Stack */
    stack_init();

    sys_exceptHandlerRegister(sampleGwSysException);

    /* Initialize user application */
    user_app_init();

    /* User's Task */
#if ZBHCI_EN
    /*
     * define ZBHCI_USB_PRINT, ZBHCI_USB_CDC or ZBHCI_UART as 1 in app_cfg.h
     * if needing to enable ZBHCI_EN
        在app_cfg.h中将ZBHCI_USB_PRINT、ZBHCI_USB-CDC或ZBHCI_UART定义为1
        如果需要启用ZBHCI_EN
     *
     * */
    zbhciInit();
    ev_on_poll(EV_POLL_HCI, zbhciTask);
#endif
    ev_on_poll(EV_POLL_IDLE, app_task);

    // af_nodeDescStackRevisionSet(20);

    /*
     * bdb initialization start,
     * once initialization is done, the g_zbDemoBdbCb.bdbInitCb() will be called
     *
     * */
    bdb_init((af_simple_descriptor_t *)&sampleGW_simpleDesc, &g_bdbCommissionSetting, &g_zbDemoBdbCb, 1);
    LOG("Gateway init Finish");
}

#endif /* __PROJECT_TL_GW__ */
