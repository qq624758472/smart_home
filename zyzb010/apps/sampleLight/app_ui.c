/********************************************************************************************************
 * @file    app_ui.c
 *
 * @brief   This is the source file for app_ui
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
#include "app_ui.h"

/**********************************************************************
 * LOCAL CONSTANTS
 */

/**********************************************************************
 * TYPEDEFS
 */

/**********************************************************************
 * GLOBAL VARIABLES
 */

/**********************************************************************
 * LOCAL FUNCTIONS
 */
void led_on(u32 pin)
{
    drv_gpio_write(pin, LED_ON);
}

void led_off(u32 pin)
{
    drv_gpio_write(pin, LED_OFF);
}

void led_init(void)
{
    led_off(LED_POWER);  // R
    led_off(LED_PERMIT); // G
}

void localPermitJoinState(void)
{
    static bool assocPermit = 0;
    if (assocPermit != zb_getMacAssocPermit()) // 获取本节点的入网允许状态
    {
        assocPermit = zb_getMacAssocPermit();
        if (assocPermit)
        {
            led_on(LED_PERMIT);
        }
        else
        {
            led_off(LED_PERMIT);
        }
    }
}

void buttonKeepPressed(u8 btNum)
{
    if (btNum == VK_SW1)
    {
        gLightCtx.state = APP_FACTORY_NEW_DOING;
        zb_factoryReset();
    }
    else if (btNum == VK_SW2)
    {
    }
}

void buttonShortPressed(u8 btNum)
{
    if (btNum == VK_SW1)
    {
        if (zb_isDeviceJoinedNwk())
        {
            gLightCtx.sta = !gLightCtx.sta;
            if (gLightCtx.sta)
            {
                sampleLight_onoff(ZCL_ONOFF_STATUS_ON);
            }
            else
            {
                sampleLight_onoff(ZCL_ONOFF_STATUS_OFF);
            }
        }
    }
    else if (btNum == VK_SW2)
    {
// /* toggle local permit Joining */
// static u8 duration = 0;
// duration = duration ? 0 : 0xff;
// zb_nlmePermitJoiningRequest(duration);//仅适用协调器和路由器
#if 0
        // lsh  按键发送数测试
        epInfo_t dstEpInfo;
        TL_SETSTRUCTCONTENT(dstEpInfo, 0);

        dstEpInfo.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
        dstEpInfo.dstEp = SAMPLE_TEST_ENDPOINT;
        dstEpInfo.dstAddr.shortAddr = 0xffff; // 广播地址
        dstEpInfo.profileId = HA_PROFILE_ID;
        dstEpInfo.txOptions = 0;
        dstEpInfo.radius = 0;
        LOG("");
        u8 buf[32] = {0};
        memset(buf, 0xaa, sizeof(buf));
        u8 seqNum = 0;
        af_dataSend(SAMPLE_LIGHT_ENDPOINT, &dstEpInfo, ZCL_CLUSTER_TELINK_SDK_TEST_REQ, 32, buf, &seqNum);
        LOG("af_dataSend success\r\n");
#endif
    }
}

void keyScan_keyPressedCB(kb_data_t *kbEvt)
{
    //	u8 toNormal = 0;
    u8 keyCode = kbEvt->keycode[0];
    //	static u8 lastKeyCode = 0xff;

    // buttonShortPressed(keyCode);//没用，先删了

    if (keyCode == VK_SW1)
    {
        gLightCtx.keyPressedTime = clock_time();
        gLightCtx.state = APP_FACTORY_NEW_SET_CHECK;
    }
    if (keyCode == VK_SW2)
    {
    #if 1
        // lsh  按键发送数测试
        epInfo_t dstEpInfo;
        TL_SETSTRUCTCONTENT(dstEpInfo, 0);

        dstEpInfo.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
        dstEpInfo.dstEp = SAMPLE_TEST_ENDPOINT;
        dstEpInfo.dstAddr.shortAddr = 0xffff; // 广播地址
        dstEpInfo.profileId = HA_PROFILE_ID;
        dstEpInfo.txOptions = 0;
        dstEpInfo.radius = 0;
        LOG(""); 
        u8 buf[32] = {0};
        memset(buf, 0xaa, sizeof(buf));
        u8 seqNum = 0;
        af_dataSend(SAMPLE_LIGHT_ENDPOINT, &dstEpInfo, ZCL_CLUSTER_TELINK_SDK_TEST_REQ, 32, buf, &seqNum);
        LOG("af_dataSend success\r\n");
#endif
    }
}

void keyScan_keyReleasedCB(u8 keyCode)
{
    gLightCtx.state = APP_STATE_NORMAL;
}

volatile u8 T_keyPressedNum = 0;
void app_key_handler(void)
{
    static u8 valid_keyCode = 0xff;

    if (gLightCtx.state == APP_FACTORY_NEW_SET_CHECK)
    {
        if (clock_time_exceed(gLightCtx.keyPressedTime, 5 * 1000 * 1000)) // 长按5秒离网
        {
            buttonKeepPressed(VK_SW1);
        }
    }

    if (kb_scan_key(0, 1))
    {
        T_keyPressedNum++;
        if (kb_event.cnt)
        {
            keyScan_keyPressedCB(&kb_event);
            if (kb_event.cnt == 1)
            {
                valid_keyCode = kb_event.keycode[0];
            }
        }
        else
        {
            keyScan_keyReleasedCB(valid_keyCode);
            valid_keyCode = 0xff;
        }
    }
}

#endif /* __PROJECT_TL_DIMMABLE_LIGHT__ */
