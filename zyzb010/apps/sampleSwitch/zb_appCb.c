/********************************************************************************************************
 * @file    zb_appCb.c
 *
 * @brief   This is the source file for zb_appCb
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

/**********************************************************************
 * LOCAL CONSTANTS
 */

/**********************************************************************
 * TYPEDEFS
 */

/**********************************************************************
 * LOCAL FUNCTIONS
 */
void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork);
void zbdemo_bdbCommissioningCb(u8 status, void *arg);
void zbdemo_bdbIdentifyCb(u8 endpoint, u16 srcAddr, u16 identifyTime);
void zbdemo_bdbFindBindSuccessCb(findBindDst_t *pDstInfo);

/**********************************************************************
 * LOCAL VARIABLES
 */
bdb_appCb_t g_zbDemoBdbCb =
    {
        zbdemo_bdbInitCb,
        zbdemo_bdbCommissioningCb,
        zbdemo_bdbIdentifyCb,
        zbdemo_bdbFindBindSuccessCb};

#ifdef ZCL_OTA
ota_callBack_t sampleSwitch_otaCb =
    {
        sampleSwitch_otaProcessMsgHandler,
};
#endif

/**********************************************************************
 * FUNCTIONS
 */
s32 sampleSwitch_bdbNetworkSteerStart(void *arg)
{
    bdb_networkSteerStart(); // 启动搜索网络

    return -1;
}

#if FIND_AND_BIND_SUPPORT
s32 sampleSwitch_bdbFindAndBindStart(void *arg)
{
    BDB_ATTR_GROUP_ID_SET(0x1234); // only for initiator
    bdb_findAndBindStart(BDB_COMMISSIONING_ROLE_INITIATOR);

    g_switchAppCtx.bdbFBTimerEvt = NULL;
    return -1;
}
#endif

/*********************************************************************
 * @fn      zbdemo_bdbInitCb
 *
 * @brief   application callback for bdb initiation
 *
 * @param   status - the status of bdb init BDB_INIT_STATUS_SUCCESS or BDB_INIT_STATUS_FAILURE
 *
 * @param   joinedNetwork  - 1: node is on a network, 0: node isn't on a network
 *
 * @return  None
 */
void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork)
{
    LOG("zbdemo_bdbInitCb\r\n");
    if (status == BDB_INIT_STATUS_SUCCESS)
    {
        /*
         **对于非出厂新设备：
         *从NV加载zcl数据，启动轮询速率，启动ota查询，bdb-networkSteerStart
         *
         *对于工厂新设备：
         *引导网络
         *
         */
        LOG("joinedNetwork %d\r\n",joinedNetwork);
        // joinedNetwork = 0;
        if (joinedNetwork)
        {
            zb_setPollRate(POLL_RATE * 3);

#ifdef ZCL_OTA
            ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif

#ifdef ZCL_POLL_CTRL
            sampleSwitch_zclCheckInStart();
#endif
        }
        else
        {
            u16 jitter = 0;
            do
            {
                jitter = zb_random() % 0x0fff;
            } while (jitter == 0);
            TL_ZB_TIMER_SCHEDULE(sampleSwitch_bdbNetworkSteerStart, NULL, jitter);
        }
    }
    else
    {
        LOG("status:%d\r\n",status);
    }
}

/*********************************************************************
 * @fn      zbdemo_bdbCommissioningCb
 *
 * @brief   application callback for bdb commissioning
 *
 * @param   status - the status of bdb commissioning
 *
 * @param   arg
 *
 * @return  None
 * 在执行完 BDB Commissioning之后，系统会回调bdbCommissioningcb函数，
 * 将commissioning的结果返回给用户，由用户决定下一步的动作
 */
u8 sleepCnt = 0;
void zbdemo_bdbCommissioningCb(u8 status, void *arg)
{
    LOG("zbdemo_bdbCommissioningCb\r\n");
    if (status == BDB_COMMISSION_STA_SUCCESS)
    {
        LOG("");
        zb_setPollRate(POLL_RATE * 3);

#ifdef ZCL_POLL_CTRL
        sampleSwitch_zclCheckInStart();
#endif

        light_blink_start(2, 200, 200);

#ifdef ZCL_OTA
        ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif

#if FIND_AND_BIND_SUPPORT
        // start Finding & Binding
        if (!g_switchAppCtx.bdbFBTimerEvt)
        {
            g_switchAppCtx.bdbFBTimerEvt = TL_ZB_TIMER_SCHEDULE(sampleSwitch_bdbFindAndBindStart, NULL, 50);
        }
#endif
    }
    else if (status == BDB_COMMISSION_STA_IN_PROGRESS)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_NOT_AA_CAPABLE)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_NO_NETWORK)
    {
        LOG("");
        /*if(sleepCnt++ >= 5){
            sleepCnt = 0;
            zb_setPollRate(0);
        }else*/
        {
            u16 jitter = 0;
            do
            {
                jitter = zb_random() % 0x0fff;
            } while (jitter == 0);
            TL_ZB_TIMER_SCHEDULE(sampleSwitch_bdbNetworkSteerStart, NULL, jitter);
        }
    }
    else if (status == BDB_COMMISSION_STA_TARGET_FAILURE)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_FORMATION_FAILURE)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_NO_IDENTIFY_QUERY_RESPONSE)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_BINDING_TABLE_FULL)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_NO_SCAN_RESPONSE)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_NOT_PERMITTED)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_TCLK_EX_FAILURE)
    {
        LOG("");
    }
    else if (status == BDB_COMMISSION_STA_PARENT_LOST)
    {
        LOG("");
        /*
         * Becoming an orphan node now.
         * Attempt to join network by invoking rejoin request,
         * internal will start an rejoin backoff timer
         * based on 'config_rejoin_backoff_time' once rejoin failed.
         * *现在成为孤儿节点。
         * 通过调用重新加入请求尝试加入网络，
         *内部将启动重新加入退避定时器
         *一旦重新加入失败，则基于“config_rejung_backofftime”。
         */
        // zb_rejoin_mode_set(REJOIN_INSECURITY);
        zb_rejoinReq(NLME_REJOIN_METHOD_REJOIN, zb_apsChannelMaskGet());
    }
    else if (status == BDB_COMMISSION_STA_REJOIN_FAILURE)
    {
        LOG("");
        zb_factoryReset();//恢复出厂设置后，必须手动reset才能重新启动， 但是能正常加入网络。
        SYSTEM_RESET();//重启芯片
    }
}

extern void sampleSwitch_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 identifyTime);
void zbdemo_bdbIdentifyCb(u8 endpoint, u16 srcAddr, u16 identifyTime)
{
    LOG("zbdemo_bdbIdentifyCb\r\n");
#if FIND_AND_BIND_SUPPORT
    sampleSwitch_zclIdentifyCmdHandler(endpoint, srcAddr, identifyTime);
#endif
}

/*********************************************************************
 * @fn      zbdemo_bdbFindBindSuccessCb
 *
 * @brief   application callback for finding & binding
 *
 * @param   pDstInfo
 *
 * @return  None
 */
void zbdemo_bdbFindBindSuccessCb(findBindDst_t *pDstInfo)
{
    LOG("zbdemo_bdbFindBindSuccessCb\r\n");
#if FIND_AND_BIND_SUPPORT
    epInfo_t dstEpInfo;
    TL_SETSTRUCTCONTENT(dstEpInfo, 0);

    dstEpInfo.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dstEpInfo.dstAddr.shortAddr = pDstInfo->addr;
    dstEpInfo.dstEp = pDstInfo->endpoint;
    dstEpInfo.profileId = HA_PROFILE_ID;

    zcl_identify_identifyCmd(SAMPLE_SWITCH_ENDPOINT, &dstEpInfo, FALSE, 0, 0);
#endif
}

#ifdef ZCL_OTA
void sampleSwitch_otaProcessMsgHandler(u8 evt, u8 status)
{
    // printf("sampleSwitch_otaProcessMsgHandler: status = %x\n", status);
    if (evt == OTA_EVT_START)
    {
        if (status == ZCL_STA_SUCCESS)
        {
            zb_setPollRate(QUEUE_POLL_RATE);
        }
        else
        {
        }
    }
    else if (evt == OTA_EVT_COMPLETE)
    {
        zb_setPollRate(POLL_RATE * 3);

        if (status == ZCL_STA_SUCCESS)
        {
            ota_mcuReboot();
        }
        else
        {
            ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
        }
    }
}
#endif

/*********************************************************************
 * @fn      sampleSwitch_leaveCnfHandler
 *
 * @brief   Handler for ZDO Leave Confirm message.
 *
 * @param   pRsp - parameter of leave confirm
 *
 * @return  None
 */
void sampleSwitch_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf)
{
    if (pLeaveCnf->status == SUCCESS)
    {
        // SYSTEM_RESET();
    }
}

/*********************************************************************
 * @fn      sampleSwitch_leaveIndHandler
 *
 * @brief   Handler for ZDO leave indication message.
 *
 * @param   pInd - parameter of leave indication
 *
 * @return  None
 */
void sampleSwitch_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    // printf("sampleSwitch_leaveIndHandler, rejoin = %d\n", pLeaveInd->rejoin);
    // printfArray(pLeaveInd->device_address, 8);
}

#endif /* __PROJECT_TL_SWITCH__ */
