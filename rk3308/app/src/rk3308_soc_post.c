/******************************************************************************
* COPYRIGHT Beijing UCAS Space Technology Co.,Ltd
*******************************************************************************

*******************************************************************************
* 文件名称: prj0878_soc_post.c
* 功能描述: 开机自检程序(Power-on Self Test Program)
* 使用说明:
* 文件作者:
* 编写日期: 2022/1/22
* 修改历史:
* 修改版本  修改日期     修改人        修改内容
* -----------------------------------------------------------------------------
* 01a      2022/1/22    zhanghao     创建基础版本
*******************************************************************************/

/******************************* 包含文件声明 ********************************/
#include <stdio.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <mtd/mtd-abi.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "drv_common.h"
#include "prj0878_soc_post.h"
#include "i2c_access.h"
#include "net.h"

#include "op_dma/op_dma.h"
#include "ds18b20/ds.h"

#include "uart/uart.h"

#define SET_CAN0_RATE "ip link set can0 up type can bitrate 500000"
#define CAN0_ENABLE "ifconfig can0 up"
#define CAN0_DISEN "ifconfig can0 down"
#define SET_CAN1_RATE "ip link set can1 up type can bitrate 500000"
#define CAN1_ENABLE "ifconfig can1 up"
#define CAN1_DISEN "ifconfig can1 down"

#define SET_CAN0_RATE_CTRL "canconfig can0 bitrate 500000"
#define SET_CAN1_RATE_CTRL "canconfig can1 bitrate 500000"

void enable(unsigned char type, unsigned char data);
void flash_resume(unsigned int value);
void recov_qspi_ctrlor_rd();
void update_print(void);

pthread_t thrdUpdate_id;
// 看门狗
int gDogFeedCtrl = 1;

// can接口相关全局变量
int g_canInit = 0;
pthread_t g_thrd_id = 0, g_canrx_thrd_id[2] = {0, 0};
pthread_t g_rtc_read_thrd_id = 0;
can_mng stCanMng[2];
int g_canid = 0;
int caiji_cnt = 0;

// RTC中断相关全局变量
int g_rtcHdl = -1;
pps_t *pg_PPSReg = NULL;
int rtc_irq_fd;
unsigned int intSlotCnt = 0;
unsigned int intPpsCnt = 0;

// MRAM全局变量
void *g_mram_addr = NULL;

/*******************************************************************************
* 函数名称: gpio_export
* 功    能: GPIO导出声明函数
* 函数类型: 函数
* 参    数: 参数名     数据类型        出/入参      含义
            pin        int             入参         GPIO管脚ID
* 函数返回:
* 说    明: 无
*******************************************************************************/

int gpio_export(int pin)
{
    char buffer[64];
    int len;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0)
    {
        LOG("Failed to open export for writing!\n");
        return (-1);
    }

    memset(buffer, 0, sizeof(buffer));
    len = snprintf(buffer, sizeof(buffer), "%d", pin);
    if (write(fd, buffer, len) < 0)
    {
        LOG("Failed to export gpio=%d!\n", pin);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/*******************************************************************************
* 函数名称: gpio_unexport
* 功    能: GPIO撤销声明函数
* 函数类型: 函数
* 参    数: 参数名     数据类型        出/入参      含义
            pin        int             入参         GPIO管脚ID
* 函数返回:
* 说    明: 无
*******************************************************************************/

int gpio_unexport(int pin)
{
    char buffer[64];
    int len;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0)
    {
        LOG("Failed to open unexport for writing!\n");
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));
    len = snprintf(buffer, sizeof(buffer), "%d", pin);
    if (write(fd, buffer, len) < 0)
    {
        LOG("Failed to unexport gpio=%d!\n", pin);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/*******************************************************************************
* 函数名称: gpio_direction
* 功    能: GPIO方向控制函数
* 函数类型: 函数
* 参    数: 参数名     数据类型        出/入参      含义
            pin        int             入参         GPIO管脚ID
            dir        int             入参         方向，0 ：IN   1: OUT
* 函数返回:  成功 0
             失败 错误码
* 说    明: 无
*******************************************************************************/

int gpio_direction(int pin, int dir)
{
    static const char dir_str[] = "in\0out";
    char path[64];
    int fd;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0)
    {
        LOG("Failed to open gpio direction for writing!\n");
        return -1;
    }

    if (write(fd, &dir_str[dir == 0 ? 0 : 3], dir == 0 ? 2 : 3) < 0)
    {
        LOG("Failed to set direction!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/*******************************************************************************
* 函数名称: gpio_read
* 功    能: GPIO读函数
* 函数类型: 函数
* 参    数: 参数名     数据类型        出/入参      含义
            pin        int             入参         GPIO管脚ID
* 函数返回:  GPIO管脚状态 0 / 1
* 说    明: 无
*******************************************************************************/

int gpio_read(int pin)
{
    char path[64];
    char value_str[3];
    int fd;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        LOG("Failed to open gpio=%d value for reading!\n", pin);
        return -1;
    }

    if (read(fd, value_str, 3) < 0)
    {
        LOG("Failed to read gpio=%d value!\n", pin);
        close(fd);
        return -1;
    }

    close(fd);
    return (atoi(value_str));
}

/*******************************************************************************
* 函数名称: gpio_write
* 功    能: GPIO写函数
* 函数类型: 函数
* 参    数: 参数名     数据类型        出/入参      含义
            pin        int             入参         GPIO管脚ID
            value      int             入参         写入数值，0/1
* 函数返回:  成功 0
             失败 错误码
* 说    明: 无
*******************************************************************************/

int gpio_write(int pin, int value)
{
    static const char values_str[] = "01";
    char path[64];
    int fd;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0)
    {
        LOG("Failed to open gpio=%d value for writing!\n", pin);
        return -1;
    }
#if 0
	if (write(fd, &values_str[value == 0 ? 0 : 1], 1) < 0)
	{
        LOG("Failed to write gpio=%d value=%d!\n", pin, value);
        close(fd);
        return -1;
    	}
#else
    if (value == 0x1)
    {
        write(fd, "1", 2);
    }
    else if (value == 0x0)
    {
        write(fd, "0", 2);
    }
    else
    {
    }
#endif

    close(fd);
    return 0;
}

void rtc_irq_proc(int signum)
{
    unsigned int tmp = 0;
    // 时间片中断
    if (pg_PPSReg->IntRsp_R16 & INT_RSP_TSLOT)
    {
        // 通过中断清除寄存器写入值，清除中断响应寄存器的相应位
        pg_PPSReg->IntClr_R14 = INT_CLR_TSLOT;
        LOG("RTC ...  Local time:\n");
        LOG("Sec       = (%u)\n", ((pg_PPSReg->LocSecH_R18 << 16) + pg_PPSReg->LocSecL_R17));
        tmp = (pg_PPSReg->LocUsH_R20 << 16) + pg_PPSReg->LocUsL_R19;
        LOG("     uSec       = (%u)\n", ((tmp * 100) / 125));
        intSlotCnt += 1;

        if (intSlotCnt % 100 == 0)
        {
            LOG("rtc_irq_proc: intSlotCnt=%d!\n", intSlotCnt);
        }
    }

    // PPS中断
    if (pg_PPSReg->IntRsp_R16 & INT_RSP_PPS)
    {
        // 通过中断清除寄存器写入值，清除中断响应寄存器的相应位
        pg_PPSReg->IntClr_R14 = INT_CLR_PPS;
        LOG("PPS ***************************************************88 ...  PPS time:\n");
        LOG("Sec       = (%u)\n", ((pg_PPSReg->LocSecH_R18 << 16) + pg_PPSReg->LocSecL_R17));
        LOG("     uSec       = (%u)\n", ((pg_PPSReg->LocUsH_R20 << 16) + pg_PPSReg->LocUsL_R19));
        intPpsCnt += 1;

        if (intPpsCnt % 100 == 0)
        {
            LOG("rtc_irq_proc: intPpsCnt=%d!\n", intPpsCnt);
        }
    }
}

void *rtc_irq_read_thrd(void *arg)
{
    int ret = 0;
    char value_read[3];
    char len;
    pthread_detach(pthread_self());
    while (1)
    {
        ret = read(rtc_irq_fd, value_read, len);
        rtc_irq_proc(0);
        usleep(10000);
    }
    return NULL;
}

void pps_open(unsigned char type)
{

    //	unsigned int *rtcAddr, data;
    int Oflags;
    int rtcEnHdl = -1;
    char path[64];
    int ret = -1;

#if 1
#if 0
    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", PL_GPO_RTC);
    if (-1 == access(path, F_OK))
    {
        gpio_export(PL_GPO_RTC);
        gpio_direction(PL_GPO_RTC, GPIO_DIR_OUT);
    }

    gpio_write(PL_GPO_RTC, 1);
#endif
#else

    // RTC使能接口使用的交互区映射
    rtcAddr = (unsigned int *)devm_map(PPS_RTC_EN_BASE, PPS_MEM_LEN, &rtcEnHdl);
    if (rtcAddr == NULL)
    {
        LOG("rtc enable mmap fail!\n");
        return;
    }

    data = *rtcAddr;
    *rtcAddr = data | 0x8; // RTC功能打开

    usleep(10);
    LOG("enable reg=0x%08x\n", *rtcAddr);

    devm_unmap((void *)rtcAddr, PPS_MEM_LEN, &rtcEnHdl);
#endif
    // RTC模块交互区映射
    if (type == 0)
    {
        pg_PPSReg = (pps_t *)devm_map(PPS_MEM_BASE1, PPS_MEM_LEN, &g_rtcHdl);
    }
    else
    {
        pg_PPSReg = (pps_t *)devm_map(PPS_MEM_BASE2, PPS_MEM_LEN, &g_rtcHdl);
    }
    if (pg_PPSReg == NULL)
    {
        LOG("rtc mmap fail!\n");
        return;
    }
    memset(pg_PPSReg, 0, sizeof(pps_t));
    LOG("%s:%d pg_PPSReg=%p\n", __FUNCTION__, __LINE__, pg_PPSReg);
    // 尝试对该内存进行加锁

#if 0
	//rtc中断初始化 
	signal(SIGIO, rtc_irq_proc);	
	rtc_irq_fd = open("/dev/rtc_irq_gkhy", O_RDWR);
	if (rtc_irq_fd < 0)
	{
		LOG("/dev/rtc_irq_gkhy can't open!\n");
	}
 
	fcntl(rtc_irq_fd, F_SETOWN, getpid());
	Oflags = fcntl(rtc_irq_fd, F_GETFL);	
	fcntl(rtc_irq_fd, F_SETFL, Oflags | FASYNC);
#else
    if (g_rtc_read_thrd_id == 0)
    {
        int err = pthread_create(&g_rtc_read_thrd_id, NULL, rtc_irq_read_thrd, NULL);
        if (err != 0)
        {
            LOG("Create can_sample_thrd fail: %s\n", strerror(err));
            return;
        }
        LOG("can_sample_thrd(g_thrd_id=0x%08x) create success!\n", (unsigned int)g_rtc_read_thrd_id);
    }

    rtc_irq_fd = open("/dev/rtc1_irq", O_RDWR);
    if (rtc_irq_fd < 0)
    {
        LOG("/dev/rtc_irq_gkhy can't open!\n");
    }
#endif
}

void pps_close()
{
    devm_unmap((void *)pg_PPSReg, PPS_MEM_LEN, &g_rtcHdl);
    pg_PPSReg = NULL;
}

void pps_dump()
{
    if (pg_PPSReg == NULL)
        return;

    LOG("--------------- PPS REG INFO --------------\n");
    LOG("Ctrl_R0 = 0x%04x(%u)\n", pg_PPSReg->Ctrl_R0, pg_PPSReg->Ctrl_R0);
    LOG("Earth timing:\n");
    LOG("  SecH_R2          = 0x%04x(%u)\n", pg_PPSReg->SecH_R2, pg_PPSReg->SecH_R2);
    LOG("  SecL_R1          = 0x%04x(%u)\n", pg_PPSReg->SecL_R1, pg_PPSReg->SecL_R1);
    LOG("  UsH_R6           = 0x%04x(%u)\n", pg_PPSReg->UsH_R6, pg_PPSReg->UsH_R6);
    LOG("  UsL_R5           = 0x%04x(%u)\n", pg_PPSReg->UsL_R5, pg_PPSReg->UsL_R5);
    LOG("Center timing:\n");
    LOG("  SecDifH_R4       = 0x%04x(%u)\n", pg_PPSReg->SecDifH_R4, pg_PPSReg->SecDifH_R4);
    LOG("  SecDifL_R3       = 0x%04x(%u)\n", pg_PPSReg->SecDifL_R3, pg_PPSReg->SecDifL_R3);
    LOG("  UsDifH_R8        = 0x%04x(%u)\n", pg_PPSReg->UsDifH_R8, pg_PPSReg->UsDifH_R8);
    LOG("  UsDifL_R7        = 0x%04x(%u)\n", pg_PPSReg->UsDifL_R7, pg_PPSReg->UsDifL_R7);
    LOG("Average timing:\n");
    LOG("  AvgTimInt_R9     = 0x%04x(%u)\n", pg_PPSReg->AvgTimInt_R9, pg_PPSReg->AvgTimInt_R9);
    LOG("  AvgTimSecDif_R10 = 0x%04x(%u)\n", pg_PPSReg->AvgTimSecDif_R10, pg_PPSReg->AvgTimSecDif_R10);
    LOG("  AvgTimUsDifH_R12 = 0x%04x(%u)\n", pg_PPSReg->AvgTimUsDifH_R12, pg_PPSReg->AvgTimUsDifH_R12);
    LOG("  AvgTimUsDifL_R11 = 0x%04x(%u)\n", pg_PPSReg->AvgTimUsDifL_R11, pg_PPSReg->AvgTimUsDifL_R11);
    LOG("Interrupt Registors:\n");
    LOG("  IntEn_R13        = 0x%04x(%u)\n", pg_PPSReg->IntEn_R13, pg_PPSReg->IntEn_R13);
    LOG("  IntClr_R14       = 0x%04x(%u)\n", pg_PPSReg->IntClr_R14, pg_PPSReg->IntClr_R14);
    LOG("  IntIntv_R15      = 0x%04x(%u)\n", pg_PPSReg->IntIntv_R15, pg_PPSReg->IntIntv_R15);
    LOG("  IntRsp_R16       = 0x%04x(%u)\n", pg_PPSReg->IntRsp_R16, pg_PPSReg->IntRsp_R16);
    LOG("Local time:\n");
    LOG("  LocSecH_R18      = 0x%04x(%u)\n", pg_PPSReg->LocSecH_R18, pg_PPSReg->LocSecH_R18);
    LOG("  LocSecL_R17      = 0x%04x(%u)\n", pg_PPSReg->LocSecL_R17, pg_PPSReg->LocSecL_R17);
    LOG("  LocUsH_R20       = 0x%04x(%u)\n", pg_PPSReg->LocUsH_R20, pg_PPSReg->LocUsH_R20);
    LOG("  LocUsL_R19       = 0x%04x(%u)\n", pg_PPSReg->LocUsL_R19, pg_PPSReg->LocUsL_R19);
}

void pps_test(unsigned char type, unsigned char id, unsigned int data)
{
    pps_t ppsStRd = {0};

    if (pg_PPSReg == NULL)
        return;

    // 读取出来
    // memcpy((void *)&ppsStRd, (void *)pg_PPSReg, sizeof(pps_t));
    ppsStRd.Ctrl_R0 = pg_PPSReg->Ctrl_R0;
    usleep(10);

    ppsStRd.IntEn_R13 = pg_PPSReg->IntEn_R13;
    usleep(10);
    ppsStRd.IntClr_R14 = pg_PPSReg->IntClr_R14;
    usleep(10);
    ppsStRd.IntIntv_R15 = pg_PPSReg->IntIntv_R15;
    usleep(10);
    ppsStRd.IntRsp_R16 = pg_PPSReg->IntRsp_R16;
    usleep(10);

    pps_dump();
    LOG("pps -> type= %x  data=%d\n", type, data);
    if (type == 0) // 地面校时
    {
        // 设置秒、毫秒数值，写入PS-PL交互区
        pg_PPSReg->SecL_R1 = 5;
        pg_PPSReg->SecH_R2 = 0;
        pg_PPSReg->UsL_R5 = 1800;
        pg_PPSReg->UsH_R6 = 0;

        usleep(1000);

        // 控制使能
        ppsStRd.Ctrl_R0 = (ppsStRd.Ctrl_R0 & PPS_CTRL_TIM_EN_MASK) | PPS_CTRL_ETH_TIM_EN;
        pg_PPSReg->Ctrl_R0 = ppsStRd.Ctrl_R0;
    }
    else if (type == 1) // 集中校时
    {
        // 设置秒、毫秒数值，写入PS-PL交互区
        pg_PPSReg->SecDifL_R3 = 0xa;
        pg_PPSReg->SecDifH_R4 = 0x0;
        pg_PPSReg->UsDifL_R7 = 18000;
        pg_PPSReg->UsDifH_R8 = 0;

        usleep(1000);

        // 控制使能
        ppsStRd.Ctrl_R0 = (ppsStRd.Ctrl_R0 & PPS_CTRL_TIM_EN_MASK) | PPS_CTRL_CEN_TIM_EN;
        if (id == 0) // 逆向
        {
            pg_PPSReg->Ctrl_R0 = ppsStRd.Ctrl_R0 & (~PPS_CTRL_CEN_TIM_FORW);
        }
        else // 正向
        {
            pg_PPSReg->Ctrl_R0 = ppsStRd.Ctrl_R0 | PPS_CTRL_CEN_TIM_FORW;
        }
    }
    else if (type == 6) // 集中校时
    {
        // 设置秒、毫秒数值，写入PS-PL交互区
        pg_PPSReg->SecDifL_R3 = 0x0;
        pg_PPSReg->SecDifH_R4 = 0x0;
        pg_PPSReg->UsDifL_R7 = (data % 65536);
        pg_PPSReg->UsDifH_R8 = (data / 65536);

        usleep(1000);
        // 控制使能
        LOG("RTC ...  Local time:\n");
        LOG("Sec       = (%u)\n", ((pg_PPSReg->LocSecH_R18 << 16) + pg_PPSReg->LocSecL_R17));
        unsigned int tmp = (pg_PPSReg->LocUsH_R20 << 16) + pg_PPSReg->LocUsL_R19;
        LOG("     uSec       = (%u)\n", ((tmp * 100) / 125));
        //    ppsStRd.Ctrl_R0 = (ppsStRd.Ctrl_R0 & PPS_CTRL_TIM_EN_MASK) | PPS_CTRL_CEN_TIM_EN;

        if (id == 0) // 逆向
        {
            pg_PPSReg->Ctrl_R0 = ((ppsStRd.Ctrl_R0 & (~PPS_CTRL_CEN_TIM_FORW)) | PPS_CTRL_CEN_TIM_EN);
        }
        else // 正向
        {
            pg_PPSReg->Ctrl_R0 = (ppsStRd.Ctrl_R0 | PPS_CTRL_CEN_TIM_FORW | PPS_CTRL_CEN_TIM_EN);
        }
    }
    else if (type == 2) // 均匀校时
    {
        // 设置秒、毫秒数值，写入PS-PL交互区
        pg_PPSReg->AvgTimInt_R9 = 0x10;
        pg_PPSReg->AvgTimSecDif_R10 = 0x10;
        pg_PPSReg->AvgTimUsDifL_R11 = 1800;
        pg_PPSReg->AvgTimUsDifH_R12 = 0;

        usleep(1000);

        // 控制使能
        ppsStRd.Ctrl_R0 = (ppsStRd.Ctrl_R0 & PPS_CTRL_TIM_EN_MASK) | PPS_CTRL_AVG_TIM_EN;
        if (id == 0) // 逆向
        {
            pg_PPSReg->Ctrl_R0 = ppsStRd.Ctrl_R0 & (~PPS_CTRL_AVG_TIM_FORW);
        }
        else // 正向
        {
            pg_PPSReg->Ctrl_R0 = ppsStRd.Ctrl_R0 | PPS_CTRL_AVG_TIM_FORW;
        }
    }
    else if (type == 3) // PPS 校时以及PPSn输出使能，id低4bit表示4路PPS使能情况
    {
        unsigned short ppsOutEn = id;

        // PPS校时使能
        if (data)
            ppsStRd.Ctrl_R0 = (ppsStRd.Ctrl_R0 & PPS_CTRL_TIM_EN_MASK) | PPS_CTRL_PPS_TIM_EN;
        else
            ppsStRd.Ctrl_R0 = (ppsStRd.Ctrl_R0 & PPS_CTRL_TIM_EN_MASK) & (~PPS_CTRL_PPS_TIM_EN);

        // PPS输出使能控制
        ppsOutEn = (ppsOutEn << 6) & 0x03c0;
        pg_PPSReg->Ctrl_R0 = (ppsStRd.Ctrl_R0 & PPS_CTRL_PPS_OEN_MASK) | ppsOutEn;
    }
    else if (type == 0x10) // 时间片中断
    {
        /*
        1)	对reg15时间片中断间隔寄存器赋值,即给出时间片中断的时间间隔，
            时间片可调，间隔为5ms*（该寄存器值+1）
        2)	对reg13[0]中断使能寄存器赋值
            (以下两步在中断子程序中执行)
        3)	等待中断响应，并读取reg16中断响应寄存器，判断reg16[0]是否被置起
        4)	对reg14[0]中断清除寄存器赋值，即清除中断响应寄存器reg16
        */
        pg_PPSReg->IntIntv_R15 = (unsigned short)id; // id单位为5ms
        pg_PPSReg->IntEn_R13 = INT_EN_TSLOT;

        intSlotCnt = 0;
    }
    else if (type == 0x11) // PPS中断
    {
        /*
        1)	对reg13[1]赋值
        2)	等待中断响应，并读取reg16，判断reg16[0]是否被置起
        3)	对reg14[1]赋值，即清除中断响应寄存器reg16
        */
        LOG("pg_PPSReg->IntEn_R13 = %d\n", pg_PPSReg->IntEn_R13);
        pg_PPSReg->IntEn_R13 = INT_EN_PPS;
        LOG("pg_PPSReg->IntEn_R13 = %d\n", pg_PPSReg->IntEn_R13);
        intPpsCnt = 0;
    }
    else if (type == 0xff) // 关闭所有校时功能，关闭中断
    {

        pg_PPSReg->Ctrl_R0 = 0;
        pg_PPSReg->IntEn_R13 = 0;
    }

    usleep(100);
    pps_dump();
}

void *dogfeed_thrd(void *arg)
{
    pthread_detach(pthread_self());

    // 切换使能，喂狗信号由PS输出
    gpio_write(GPIO_DOG_EN, 1);
    usleep(10);
    // gpio_write(GPIO_DOG_EN, 1);
    // usleep(10);

    while (1)
    {
        if (gDogFeedCtrl == 0)
        {
            usleep(50000);
            continue;
        }

        gpio_write(GPIO_DOG_FEED, 0);
        usleep(200000);
        gpio_write(GPIO_DOG_FEED, 1);
        usleep(200000);
    }

    return NULL;
}

int dogfeed_thrd_create(void)
{
    int err;
    pthread_t thrd_id;

    err = pthread_create(&thrd_id, NULL, dogfeed_thrd, NULL);
    if (err != 0)
    {
        LOG("Create dogfeed_thrd fail: %s\n", strerror(err));
    }

    return err;
}

// 双boot启动切换测试
void boot_test()
{
    int ret;
    char path[64];

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", GPIO_DOG_EN);
    if (-1 == access(path, F_OK))
    {
        gpio_export(GPIO_DOG_EN);
        gpio_direction(GPIO_DOG_EN, GPIO_DIR_OUT);
    }

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", GPIO_DOG_FEED);
    if (-1 == access(path, F_OK))
    {
        gpio_export(GPIO_DOG_FEED);
        gpio_direction(GPIO_DOG_FEED, GPIO_DIR_OUT);
    }

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", GPIO_BOOT_SW);
    if (-1 == access(path, F_OK))
    {
        gpio_export(GPIO_BOOT_SW);
        gpio_direction(GPIO_BOOT_SW, GPIO_DIR_OUT);
    }

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", GPIO_BOOT_STATE);
    if (-1 == access(path, F_OK))
    {
        gpio_export(GPIO_BOOT_STATE);
        gpio_direction(GPIO_BOOT_STATE, GPIO_DIR_IN);
    }

    // 创建独立的喂狗线程
    ret = dogfeed_thrd_create();
    if (ret == 0)
        LOG("dogfeed_thred create success!\n");

    return;
}

void dogfeed_ctrl(unsigned char data)
{
    gDogFeedCtrl = data;
    if (data == 0)
    {
        LOG("Watchdog is not feed.\n");
    }
    else
    {
        LOG("Watchdog is feed.\n");
    }
}

// boot切换控制，需要两次下降沿才能正常切换
void boot_sw()
{
    char path[64];

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", GPIO_BOOT_SW);
    if (-1 == access(path, F_OK))
    {
        gpio_export(GPIO_BOOT_SW);
        gpio_direction(GPIO_BOOT_SW, GPIO_DIR_OUT);
    }

    // gpio_write(GPIO_BOOT_SW, 0);
    // usleep(200000);
    gpio_write(GPIO_BOOT_SW, 1);
    usleep(1000);
    gpio_write(GPIO_BOOT_SW, 0);
    usleep(1000);
    gpio_write(GPIO_BOOT_SW, 1);
    usleep(1000);
    gpio_write(GPIO_BOOT_SW, 0);
    usleep(1000);
    LOG("boot_sw impulse create!\n");
    // usleep(100000);
    // system_cmd("reboot");
}

int boot_show()
{
    int state;
    char path[64];

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", GPIO_BOOT_STATE);
    if (-1 == access(path, F_OK))
    {
        gpio_export(GPIO_BOOT_STATE);
        gpio_direction(GPIO_BOOT_STATE, GPIO_DIR_IN);
    }

    state = gpio_read(GPIO_BOOT_STATE);
    LOG("Current boot region is %d.\n", state);
    return state;
}

// led灯测试
void led_test(unsigned char data)
{
    char path[64];

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", GPIO_LED_TEST);
    if (-1 == access(path, F_OK))
    {
        gpio_export(GPIO_LED_TEST);
        gpio_direction(GPIO_LED_TEST, GPIO_DIR_OUT);
    }

    gpio_write(GPIO_LED_TEST, (int)data);

    return;
}

// led灯测试
void ttl_out(unsigned char data)
{
    char path[64];
#if 0	
	memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", PL_GPO_TTL);
	if(-1 == access(path,F_OK))
	{
		gpio_export(PL_GPO_TTL);
    	gpio_direction(PL_GPO_TTL, GPIO_DIR_OUT);
	}

	gpio_write(PL_GPO_TTL, (int)data);
#else
    enable(5, 0);
    if (data == 0x1)
    {
        enable((PL_GPO_TTL - PL_GPO_BASE), 1);
    }
    else if (data == 0x0)
    {
        enable((PL_GPO_TTL - PL_GPO_BASE), 0);
    }
    else
    {
        LOG("data value error\n");
        return;
    }
#endif
    return;
}

void eeprom_write_func(unsigned char id, unsigned short slave_addr, unsigned char reg, unsigned char value)
{
    int ret;
    ret = i2c_open(id);
    if (ret < 0)
    {
        LOG("open i2c%d failed!\n", id);
        return;
    }

    i2c_eeprom_Write(slave_addr, reg, value);

    usleep(200000);
    i2c_fd_close(ret);
}

void eeprom_read_func(unsigned char id, unsigned short slave_addr, unsigned char reg, unsigned char value)
{
    int ret;
    int buf_len = value;
    unsigned char outbuf[128];
    ret = i2c_open(id);
    if (ret < 0)
    {
        LOG("open i2c%d failed!\n", id);
        return;
    }

    i2c_eeprom_Read(slave_addr, reg, buf_len, outbuf);

    for (int i = 0; i < buf_len; i++)
    {
        LOG("%02x  ", outbuf[i]);
    }

    usleep(200000);
    i2c_fd_close(ret);
}

void i2c_test(unsigned char type, unsigned char id, unsigned char addr, unsigned char reg)
{
    int ret;
    unsigned char rdBuf[10] = {0};
    unsigned char wtch = 0x55;

    ret = i2c_open(id);
    if (ret < 0)
    {
        LOG("open i2c%d failed!\n", id);
        return;
    }
#if 0
	if(type == 0) //read
	{
		LOG("I2C read: I2Cid=%d, addr=%d, reg=%d\n", id, addr, reg);
		i2c_Read(addr, reg, 10, rdBuf);
		print_data((char *)rdBuf, 10);
	}
	else //write
	{
		i2c_Write(addr, reg, wtch);
	}
#else
    LOG("I2C :type(read 0 write 1)=%d I2Cid=%d, addr=%d, reg=%d\n", type, id, addr, reg);
    if (type == 0) // read
    {
        unsigned char readBuf[14] = {0};
        i2cIPMB_Read(addr, 14, readBuf);
        print_data((char *)readBuf, 14);
    }
    else // write
    {
        unsigned char writeBuf[] = {0xeb, 0x90, 0x00, 0xe5, 0x00, 0x00, 0xe5};
        i2cIPMB_Write(addr, sizeof(writeBuf), writeBuf);
    }
#endif

    usleep(200000);
    i2c_close();
}

void i2c_test_t(unsigned char type, unsigned char id, unsigned char addr, unsigned char reg, unsigned char data)
{
    int ret;
    unsigned char rdBuf[2] = {0};

    ret = i2c_open(id);
    if (ret < 0)
    {
        LOG("open i2c%d failed!\n", id);
        return;
    }

    if (type == 0) // read
    {
        LOG("I2C read: I2Cid=%d, addr=%d, reg=%d\n", id, addr, reg);
        i2c_Read(addr, reg, 2, rdBuf);
        print_data((char *)rdBuf, 2);
    }
    else // write
    {
        i2c_Write(addr, reg, data);
    }

    usleep(200000);
    i2c_close();
}

int BoardIPMBTestFunc(unsigned char id, unsigned CHMC_ADDR)
{
    int ret = 0;
    unsigned char sumCheck = 0;
    unsigned char IpmbDigBuf[] = {0xeb, 0x90, 0x00, 0xe5, 0x00, 0x00, 0xe5};
    unsigned char IpmbGCBuf[] = {0xeb, 0x90, 0x00, 0xe3, 0x00, 0x00, 0xe3};
    unsigned char readBuf[64] = {0};

    ret = i2c_open(id);
    if (ret < 0)
    {
        LOG("open i2c%d failed!\n", id);
        return;
    }
    memset(readBuf, 0x0, sizeof(readBuf));
    usleep(60000);
    i2cIPMB_Write(CHMC_ADDR, sizeof(IpmbDigBuf), IpmbDigBuf);
    usleep(60000);
    i2cIPMB_Read(CHMC_ADDR, IpmbDigLen, readBuf);
    memcpy(IpmbGCtest.DigitalVar, readBuf + 6, (IpmbDigLen - 7));
    LOG("\n\r****IpmbDigLen print*****\n\r");
    print_data((char *)readBuf, IpmbDigLen);
    for (int cnt = 2; cnt < IpmbDigLen - 1; cnt++)
    {
        sumCheck += readBuf[cnt];
    }
    LOG("\n\rIpmbDigLen sum : 0x%x *****\n\r", sumCheck);
    usleep(100000);
    memset(readBuf, 0x0, sizeof(readBuf));
    i2cIPMB_Write(CHMC_ADDR, sizeof(IpmbGCBuf), IpmbGCBuf);
    usleep(60000);
    i2cIPMB_Read(CHMC_ADDR, IpmbGcLen, readBuf);
    memcpy(IpmbGCtest.GC_Var, readBuf + 6, (IpmbGcLen - 7));
    LOG("\n\r****IpmbGcLen print*****\n\r");
    print_data((char *)readBuf, IpmbGcLen);
    for (int cnt = 2; cnt < IpmbGcLen - 1; cnt++)
    {
        sumCheck += readBuf[cnt];
    }
    LOG("\n\rIpmbGcLen sum : 0x%x *****\n\r", sumCheck);
    usleep(100000);
    usleep(60000);
    i2c_close();
}

static void pabort(const char *s)
{
    perror(s);
    abort();
}

int spi_readwrte(int fd, unsigned char *txbuf, unsigned char *rxbuf, int len)
{
    struct spi_ioc_transfer tr =
        {
            .tx_buf = (unsigned long)txbuf, // 发送缓存区
            .rx_buf = (unsigned long)rxbuf, // 接收缓存区
            .len = len,
            .delay_usecs = 1000, // 发送时间间隔
        };

    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
        pabort("can't send spi message");

    for (ret = 0; ret < len / 4; ret++)
    {
        if (!(ret % 6))
            puts("");
        LOG("%.2X ", rxbuf[ret]);
    }

    return 0;
}

// spi测试函数
void spi_test()
{
    int fd;
    char *device = "/dev/spidev1.0";
    unsigned char tBuf[32] = {0};
    unsigned char rBuf[32] = {0};

    memset(tBuf, 0, 32);
    memset(rBuf, 0, 32);

    fd = open(device, O_RDWR);
    if (fd < 0)
        LOG("can't open device\n");
    else
        LOG("SPI - Open Succeed. Start Init SPI...\n");

    // 设置模式
    int ret = ioctl(fd, SPI_IOC_WR_MODE, SPI_CPHA | SPI_CPOL);
    if (ret == -1)
        pabort("can't set spi mode");

    ret = ioctl(fd, SPI_IOC_RD_MODE, SPI_CPHA | SPI_CPOL);
    if (ret == -1)
        pabort("can't get spi mode");

    // 设置一次多少位
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, SPI_CPHA | SPI_CPOL);
    if (ret == -1)
        pabort("can't set bits per word");

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, SPI_CPHA | SPI_CPOL);
    if (ret == -1)
        pabort("can't get bits per word");

    // 设置最大速度
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, SPI_CPHA | SPI_CPOL);
    if (ret == -1)
        pabort("can't set max speed hz");

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, SPI_CPHA | SPI_CPOL);
    if (ret == -1)
        pabort("can't get max speed hz");

    spi_readwrte(fd, tBuf, rBuf, 32);

    return;
}


/*************************************************************************
 * 函数名称: can_if_init
 * 功           能: CAN总线初始化，包括速率和过滤规则的设置
 * 参           数: *pCanMng
 * 返   回   值: 成功返回OK,失败返回ERR;
 * 说    明: 无
 **************************************************************************/

int can_if_init(can_mng *pCanMng)
{
    char *interface0 = "can0";
    char *interface1 = "can1";
    char *iface = NULL;
    int family = PF_CAN, type = SOCK_RAW, proto = CAN_RAW;
    struct ifreq ifr = {0};
    struct sockaddr_can addr;

    if (pCanMng->id == 0)
    {
        system_cmd(CAN0_DISEN);
        system_cmd(SET_CAN0_RATE);
        system_cmd(CAN0_ENABLE);

        iface = interface0;
    }
    else
    {
        system_cmd(CAN1_DISEN);
        system_cmd(SET_CAN1_RATE);
        system_cmd(CAN1_ENABLE);

        iface = interface1;
    }

    // LOG("%s, family=%d, type=%d, proto=%d\n", iface, family, type, proto);

    if ((pCanMng->sockid = socket(family, type, proto)) < 0)
    {
        LOG("%s socket create fail!\n", iface);
        return ERR;
    }

    strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
    if (ioctl(pCanMng->sockid, SIOCGIFINDEX, &ifr))
    {
        LOG("%s: ioctl fail!\n", iface);
        return ERR;
    }

    addr.can_family = family;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(pCanMng->sockid, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG("%s: bind fail!\n", iface);
        return ERR;
    }

    pCanMng->TxFm.can_id = 0x246;
    pCanMng->TxFm.can_dlc = 8;
    pCanMng->TxFm.data[0] = 0x40;
    pCanMng->TxFm.data[1] = 0x41;
    pCanMng->TxFm.data[2] = 0x42;
    pCanMng->TxFm.data[3] = 0x43;
    pCanMng->TxFm.data[4] = 0x44;
    pCanMng->TxFm.data[5] = 0x45;
    pCanMng->TxFm.data[6] = 0x46;
    pCanMng->TxFm.data[7] = 0x47;

    return OK;
}

// can接口测试函数
void can_test(unsigned char id, int maxPkt)
{
    int ret;
    /* can接口管理信息 */
    can_mng stCanMng;
    int nbytes;

    memset(&stCanMng, 0, sizeof(can_mng));
    if (id == 0)
    {
        stCanMng.id = 0; /* can0 */
        ret = can_if_init(&stCanMng);
        if (ret == ERR)
        {
            LOG("can%d init error\n", stCanMng.id);
        }
    }
    else
    {
        stCanMng.id = 1; /* can1 */
        ret = can_if_init(&stCanMng);
        if (ret == ERR)
        {
            LOG("can%d init error\n", stCanMng.id);
        }
    }

    for (int i = 0; i < maxPkt;)
    {
        nbytes = read(stCanMng.sockid, &stCanMng.RxFm, sizeof(struct can_frame));
        if (nbytes < 0)
        {
            LOG("can%d read error!\n", stCanMng.id);
            continue;
        }
        else
        {
            i++;
        }

        // 环回发送收到的数据
        nbytes = write(stCanMng.sockid, &stCanMng.RxFm, sizeof(struct can_frame)); // 发送frame[0]
        if (nbytes != sizeof(struct can_frame))
        {
            LOG("can%d write error!\n", stCanMng.id);
        }
    }

    usleep(10000);
    close(stCanMng.sockid);
}

int can_if_init_ctrl(can_mng *pCanMng)
{
    char *interface0 = "can0";
    char *interface1 = "can1";
    char *iface = NULL;
    int family = PF_CAN, type = SOCK_RAW, proto = CAN_RAW;
    struct ifreq ifr = {0};
    struct sockaddr_can addr;

    if (pCanMng->id == 0)
    {
        system_cmd(CAN0_DISEN);
        system_cmd(SET_CAN0_RATE_CTRL);
        system_cmd(CAN0_ENABLE);

        iface = interface0;
    }
    else
    {
        system_cmd(CAN1_DISEN);
        system_cmd(SET_CAN1_RATE_CTRL);
        system_cmd(CAN1_ENABLE);

        iface = interface1;
    }

    // LOG("%s, family=%d, type=%d, proto=%d\n", iface, family, type, proto);

    if ((pCanMng->sockid = socket(family, type, proto)) < 0)
    {
        LOG("%s socket create fail!\n", iface);
        return ERR;
    }

    strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
    if (ioctl(pCanMng->sockid, SIOCGIFINDEX, &ifr))
    {
        LOG("%s: ioctl fail!\n", iface);
        return ERR;
    }

    addr.can_family = family;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(pCanMng->sockid, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG("%s: bind fail!\n", iface);
        return ERR;
    }

    return OK;
}

void *can_sample_thrd(void *arg)
{
    unsigned char RxCanData[8];
    can_msg rxCanMsg = {0};
    int id;
    int i_cnt = 0;
    unsigned char nbytes;
    int rxCnt = 0;
    pthread_detach(pthread_self());

    // 采集请求包轮循发送，并对响应结果包接收打印
    id = (*(int *)arg);
    LOG("readcan id %d........\n\r", id);
    while (1)
    {
        nbytes = read(stCanMng[id].sockid, &stCanMng[id].RxFm, sizeof(struct can_frame));
        if (nbytes < 0)
        {
            LOG("can%d read error!\n", stCanMng[id].id);
            continue;
        }
        else
        {
            // 接收包统计
            rxCnt++;
            memcpy(&RxCanData[0], stCanMng[id].RxFm.data, 8);
            memcpy(&rxCanMsg, stCanMng[id].RxFm.data, 8);
            if ((((stCanMng[id].RxFm.can_id >> 3) & 0x1f) == 0x11) || (((stCanMng[id].RxFm.can_id >> 3) & 0x1f) == 0x12))
            {
                unsigned char frame = rxCanMsg.FrameId;
                if (frame == 0x00)
                {
                    LOG("---------------------------------------------%d-----*******ID:%x***********LEN:%d******\n\r", rxCnt, stCanMng[id].RxFm.can_id, stCanMng[id].RxFm.can_dlc);
                    caiji_cnt = 0;
                }
                LOG("-----------0x%x--%d----------\n\r", stCanMng[id].RxFm.can_id, frame);
                for (i_cnt = 2; i_cnt < stCanMng[id].RxFm.can_dlc; i_cnt++)
                {
                    LOG("***channel:%d *****RxCanData[%d]=%d\n", caiji_cnt, i_cnt, RxCanData[i_cnt]);
                    caiji_cnt++;
                }
            }
            else
            {
                LOG("---------------------------------------------%d-----*******ID:%x***********LEN:%d******\n\r", rxCnt, stCanMng[id].RxFm.can_id, stCanMng[id].RxFm.can_dlc);
                for (i_cnt = 0; i_cnt < stCanMng[id].RxFm.can_dlc; i_cnt++)
                {
                    LOG("RxCanData[%d]=0x%02x\n", i_cnt, RxCanData[i_cnt]);
                }
            }
        }
        usleep(20);
    }

    return NULL;
}

void print_can(int id, struct can_frame *pCanFrm)
{
    LOG("Fram%d, cid=0x%03x, len=%d, msg=", id, pCanFrm->can_id, pCanFrm->can_dlc);
    for (int i = 0; i < pCanFrm->can_dlc; i++)
    {
        LOG("%02x ", pCanFrm->data[i]);
    }
    LOG("\n");
}

// CAN接口接收线程，对所有CAN帧信息进行打印
void *can_rx_thrd0()
{
    unsigned char nbytes;
    unsigned int rxcnt = 0;

    pthread_detach(pthread_self());

    while (1)
    {
        nbytes = read(stCanMng[0].sockid, &stCanMng[0].RxFm, sizeof(struct can_frame));
        if (nbytes < 0)
        {
            LOG("can0 rx error!\n");
        }
        else
        {
            LOG("can0 rx: ");
            print_can(rxcnt, &stCanMng[0].RxFm);

            rxcnt += 1;
        }
    }

    return NULL;
}

void *can_rx_thrd1()
{
    unsigned char nbytes;
    unsigned int rxcnt = 0;

    pthread_detach(pthread_self());

    while (1)
    {
        nbytes = read(stCanMng[1].sockid, &stCanMng[1].RxFm, sizeof(struct can_frame));
        if (nbytes < 0)
        {
            LOG("can1 rx error!\n");
        }
        else
        {
            LOG("can1 rx: ");
            print_can(rxcnt, &stCanMng[1].RxFm);

            rxcnt += 1;
        }
    }

    return NULL;
}

// void can_ctrl(unsigned char id, unsigned char type,
//               unsigned char OCchId, unsigned char sw)
void can_ctrl(unsigned char id, unsigned char type,
              unsigned int OCchId, unsigned char sw)
{
    int ret, nbytes;
    can_msg txCanMsg = {0}, rxCanMsg = {0};
    int rxCnt = 0;
    stCanId CanIdMsg;
    unsigned int *pCanid;

    if (g_canInit == 0)
    {
        for (int i = 0; i < 2; i++)
        {
            memset(&stCanMng[i], 0, sizeof(can_mng));
            stCanMng[i].id = i; /* can0 */
            ret = can_if_init_ctrl(&stCanMng[i]);
            if (ret == ERR)
            {
                LOG("can%d init error.\n", i);
                return;
            }
        }

        g_canInit = 1;
    }

    // 创建独立的接收线程
    if ((g_canrx_thrd_id[0] == 0) && (type >= 10))
    {
        ret = pthread_create(&g_canrx_thrd_id[0], NULL, can_rx_thrd0, 0);
        if (ret != 0)
        {
            LOG("Create can_rx_thrd0 fail: %s\n", strerror(ret));
            g_canrx_thrd_id[0] = 0;
            return;
        }

        ret = pthread_create(&g_canrx_thrd_id[1], NULL, can_rx_thrd1, 0);
        if (ret != 0)
        {
            LOG("Create can_rx_thrd1 fail: %s\n", strerror(ret));
            g_canrx_thrd_id[1] = 0;
            return;
        }

        LOG("can_rx_thrd0 and can_rx_thrd1 create success!\n");
    }
    // can帧数据内容部分先填充

    if (type == 0) // 使能/禁止MCU主备切换指令
    {
        txCanMsg.MsgType = CAN_MSG_TYPE_SW_EN;
        txCanMsg.FrameId = 0xaa;
        txCanMsg.Data[0] = 0x0;
        txCanMsg.Data[1] = sw;

        if (id < 2)
        {
            stCanMng[id].TxFm.can_id = CAN_ID_OCA_BD_ADDR;
            memcpy(stCanMng[id].TxFm.data, &txCanMsg, sizeof(txCanMsg));
            stCanMng[id].TxFm.can_dlc = 4;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame));
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }
    }
    else if (type == 1) // OC控制指令
    {
        txCanMsg.MsgType = CAN_MSG_TYPE_CTRL;
        txCanMsg.FrameId = (unsigned char)OCchId; // OCchId取值范围1~42
        txCanMsg.Data[0] = 0x0;
        txCanMsg.Data[1] = sw;

        if (id < 2)
        {
            stCanMng[id].TxFm.can_id = CAN_ID_OCA_BD_ADDR;
            memcpy(stCanMng[id].TxFm.data, &txCanMsg, sizeof(txCanMsg));
            stCanMng[id].TxFm.can_dlc = 4;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }
    }
    else if (type == 2) // 采集请求
    {
        txCanMsg.MsgType = CAN_MSG_TYPE_SMP_REQ | ((unsigned char)OCchId); // OCchId表示组号，取值1~6
        txCanMsg.FrameId = 0xaa;
        txCanMsg.Data[0] = 0xaa;
        txCanMsg.Data[1] = 0xaa;

        if (id < 2)
        {
            stCanMng[id].TxFm.can_id = CAN_ID_OCA_BD_ADDR;
            memcpy(stCanMng[id].TxFm.data, &txCanMsg, sizeof(txCanMsg));
            stCanMng[id].TxFm.can_dlc = 4;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error! nbytes=%d\n", stCanMng[id].id, nbytes);
            }
        }

        usleep(100000);

        while (1)
        {
            nbytes = read(stCanMng[id].sockid, &stCanMng[id].RxFm, sizeof(struct can_frame));
            if (nbytes < 0)
            {
                LOG("can%d read error!\n", stCanMng[id].id);
                continue;
            }
            else
            {
                // 接收包统计
                rxCnt++;

                memcpy(&rxCanMsg, stCanMng[id].RxFm.data, 8);
                if ((rxCanMsg.MsgType & 0xf0) == CAN_MSG_TYPE_SMP_RSP)
                {
                    unsigned char group = rxCanMsg.MsgType & 0x0f;
                    unsigned char frame = rxCanMsg.FrameId;
                    float volt[6];
                    switch (group)
                    {
                    case 1: //
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                        if (frame > 2)
                        {
                            LOG("frame=%d error!\n", frame);
                        }

                        LOG("------ group=%d, frame=%d Voltage value -----\n", group, frame);
                        for (int j = 0; j < 6; j++)
                        {
                            volt[j] = (float)rxCanMsg.Data[j] / 100 * 2;
                            LOG("Voltage[%d]=%.2f, byte=0x%02x\n", j, volt[j], rxCanMsg.Data[j]);
                        }

                        break;

                    case 6:
                        if (frame > 4)
                        {
                            LOG("frame=%d error!\n", frame);
                        }

                        if ((frame == 0) || (frame == 1))
                        {
                            LOG("------ group=%d, frame=%d Current value -----\n", group, frame);
                            for (int j = 0; j < 6; j++)
                            {
                                volt[j] = (unsigned int)rxCanMsg.Data[j];
                                LOG("Current[%d]=%.2f, byte=0x%02x\n", j, volt[j], rxCanMsg.Data[j]);
                            }
                        }
                        else if (frame == 2)
                        {
                            volt[0] = (float)rxCanMsg.Data[0] / 100;
                            volt[1] = (float)rxCanMsg.Data[1] / 100;
                            LOG("------ group=%d, frame=%d Temperature value -----\n", group, frame);
                            LOG("Temperature[0]=%.2f\n", volt[0]);
                            LOG("Temperature[1]=%.2f\n", volt[1]);
                            LOG("OC byte0 state=0x%02x\n", rxCanMsg.Data[2]);
                            LOG("OC byte1 state=0x%02x\n", rxCanMsg.Data[3]);
                            LOG("OC byte2 state=0x%02x\n", rxCanMsg.Data[4]);
                            LOG("OC byte3 state=0x%02x\n", rxCanMsg.Data[5]);
                        }
                        else
                        {
                            LOG("------ group=%d, frame=%d OC value -----\n", group, frame);
                            LOG("OC byte0 state=0x%02x\n", rxCanMsg.Data[0]);
                            LOG("OC byte1 state=0x%02x\n", rxCanMsg.Data[1]);
                            LOG("OC byte2 state=0x%02x\n", rxCanMsg.Data[2]);
                            LOG("OC byte3 state=0x%02x\n", rxCanMsg.Data[3]);
                            LOG("OC byte4 state=0x%02x\n", rxCanMsg.Data[4]);
                            LOG("OC byte5 state=0x%02x\n", rxCanMsg.Data[5]);
                        }

                        break;

                    default:
                        LOG("MsgType=0x%02x error!\n", rxCanMsg.MsgType);
                        break;
                    }

                    if ((group > 0) && (group < 6) && (rxCnt == 3))
                        break;

                    if ((group == 6) && (rxCnt == 5))
                        break;
                }
                else
                {
                    break;
                }

            } // can read

        } // while
    }
    else if (type == 10) // 遥测请求
    {
        // stCanId CanIdMsg;
        // unsigned short *pCanid;

        if (id < 2)
        {
            CanIdMsg.Rsv = 0;
            CanIdMsg.BusPos = 1;
            CanIdMsg.FramType = 3;
            CanIdMsg.NodAddr = 0x13;
            CanIdMsg.BusId = id & 0x1;
            CanIdMsg.SglOrMul = 0;
            pCanid = (unsigned int *)&CanIdMsg;
            LOG("test: canid=0x%x", *pCanid);

            if (id == 0)
            {
                stCanMng[id].TxFm.can_id = 0x798;
            }
            else
            {
                stCanMng[id].TxFm.can_id = 0x79c;
            }
            stCanMng[id].TxFm.can_dlc = 8;
            stCanMng[id].TxFm.data[0] = 0;
            stCanMng[id].TxFm.data[1] = 0xff;
            stCanMng[id].TxFm.data[2] = 0;
            stCanMng[id].TxFm.data[3] = 0;
            stCanMng[id].TxFm.data[4] = 0;
            stCanMng[id].TxFm.data[5] = 0;
            stCanMng[id].TxFm.data[6] = 0;
            stCanMng[id].TxFm.data[7] = 0;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d tx error! nbytes=%d\n", stCanMng[id].id, nbytes);
                return;
            }
            LOG("can%d tx OK:\n", stCanMng[id].id);
            print_can(0, &stCanMng[id].TxFm);
        }

#if 0
		usleep(10000);

		while(1)
		{
			nbytes = read(stCanMng[id].sockid, &stCanMng[id].RxFm, sizeof(struct can_frame));
			if (nbytes < 0)
			{
				LOG("can%d read error!\n", stCanMng[id].id);
				continue;
			}
			else
			{
				//接收包统计
				rxCnt++;
				if(rxCnt == 1) LOG("can%d recv:\n", stCanMng[id].id);
				print_can(rxCnt, &stCanMng[id].RxFm);

				if((stCanMng[id].RxFm.can_id&0x3) == 0x3)//复帧的末帧，则接收完毕
				{
					break;
				}
			} //can read

		} //while
#endif
    }
    else if (type == 11) // 遥控指令
    {
        // stCanId CanIdMsg;
        // unsigned short *pCanid;

        if (id < 2)
        {
            CanIdMsg.Rsv = 0;
            CanIdMsg.BusPos = 1;
            CanIdMsg.FramType = 3;
            CanIdMsg.NodAddr = 0x13;
            CanIdMsg.BusId = id & 0x1;
            CanIdMsg.SglOrMul = 0;
            pCanid = (unsigned int *)&CanIdMsg;
            LOG("test: canid=0x%x", *pCanid);

            if (id == 0)
            {
                stCanMng[id].TxFm.can_id = 0x798;
            }
            else
            {
                stCanMng[id].TxFm.can_id = 0x79c;
            }
            stCanMng[id].TxFm.can_dlc = 8;
            stCanMng[id].TxFm.data[0] = 0x05;
            stCanMng[id].TxFm.data[1] = (unsigned char)OCchId;
            stCanMng[id].TxFm.data[2] = 0xec;
            stCanMng[id].TxFm.data[3] = 0x8c;
            stCanMng[id].TxFm.data[4] = 0x84;
            stCanMng[id].TxFm.data[5] = sw;
            stCanMng[id].TxFm.data[6] = 0;
            stCanMng[id].TxFm.data[7] = 0;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d tx error! nbytes=%d\n", stCanMng[id].id, nbytes);
                return;
            }
            LOG("can%d tx OK:\n", stCanMng[id].id);
            print_can(0, &stCanMng[id].TxFm);
        }
    }
    else if (type == 12) // CAN控制器复位指令
    {
        // stCanId CanIdMsg;
        // unsigned short *pCanid;

        if (id < 2)
        {
            CanIdMsg.Rsv = 0;
            CanIdMsg.BusPos = 1;
            CanIdMsg.FramType = 2;
            CanIdMsg.NodAddr = 0x13;
            CanIdMsg.BusId = id & 0x1;
            CanIdMsg.SglOrMul = 0;
            pCanid = (unsigned int *)&CanIdMsg;
            LOG("test: canid=0x%x", *pCanid);

            if (id == 0)
            {
                stCanMng[id].TxFm.can_id = 0x698;
            }
            else
            {
                stCanMng[id].TxFm.can_id = 0x69c;
            }
            stCanMng[id].TxFm.can_dlc = 2;
            stCanMng[id].TxFm.data[0] = 0xff;
            stCanMng[id].TxFm.data[1] = 0;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame) - 6); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame) - 6)
            {
                LOG("can%d tx error! nbytes=%d\n", stCanMng[id].id, nbytes);
                return;
            }
            LOG("can%d tx OK:\n", stCanMng[id].id);
            print_can(0, &stCanMng[id].TxFm);
        }
    }
    else if (type == 20) // 与测控板CAN通信协议
    {
        if (id < 2)
        {

            stCanMng[id].TxFm.can_id = 0x330;
            stCanMng[id].TxFm.can_dlc = 8;
            stCanMng[id].TxFm.data[0] = (unsigned char)(OCchId >> 24);
            stCanMng[id].TxFm.data[1] = (unsigned char)(OCchId >> 16);
            stCanMng[id].TxFm.data[2] = (unsigned char)(OCchId >> 8);
            stCanMng[id].TxFm.data[3] = (unsigned char)OCchId;
            stCanMng[id].TxFm.data[4] = 0x00;
            stCanMng[id].TxFm.data[5] = 0x00;
            stCanMng[id].TxFm.data[6] = 0x00;
            stCanMng[id].TxFm.data[7] = 0x00;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d tx error! nbytes=%d\n", stCanMng[id].id, nbytes);
                return;
            }
            LOG("can%d tx OK:\n", stCanMng[id].id);
            print_can(0, &stCanMng[id].TxFm);
        }

#if 0
		usleep(10000);

		while(1)
		{
			nbytes = read(stCanMng[id].sockid, &stCanMng[id].RxFm, sizeof(struct can_frame));
			if (nbytes < 0)
			{
				LOG("can%d read error!\n", stCanMng[id].id);
				continue;
			}
			else
			{
				rxCnt++;
				if(rxCnt == 1) LOG("can%d recv:\n", stCanMng[id].id);
				print_can(rxCnt, &stCanMng[id].RxFm);

				if(rxCnt >= 10)	break;
			}

		} //while
#endif
    }
    else
    {
        LOG("type=%d error!\n", type);
    }
}

int cancktest_yaokongpraFunc(unsigned char *data, unsigned int temp)
{
    data[0] = (unsigned char)(temp >> 16);
    data[1] = (unsigned char)(temp >> 8);
    data[2] = (unsigned char)(temp);
    return 0;
}

void canck_test(unsigned char id, unsigned char type, unsigned char num, int sw)
{
    int err, ret, nbytes;
    can_msg txCanMsg = {0};
    unsigned char txCanData[8];
    unsigned char txMuxData[136];
    unsigned short frametype;
    unsigned short nodeadd = 0;
    unsigned short ABflag = 0;
    unsigned short danfuframeflag = 0;
    unsigned char i_temp = 0, sum = 0;
    int tx_cnt = 136;
    int num_cnt = 0;

    if (g_canInit == 0)
    {
        memset(&stCanMng[id], 0, sizeof(can_mng));
        stCanMng[id].id = id; /* can0 */
        ret = can_if_init_ctrl(&stCanMng[id]);
        if (ret == ERR)
        {
            LOG("can%d init error.\n", id);
            return;
        }

        g_canid = id;
#if 1
        err = pthread_create(&g_thrd_id, NULL, can_sample_thrd, &g_canid);
        if (err != 0)
        {
            LOG("Create can_sample_thrd fail: %s\n", strerror(err));
            return;
        }
#endif
        LOG("can_sample_thrd(g_thrd_id=0x%08x) create success!\n", (unsigned int)g_thrd_id);

        g_canInit = 1;
    }

    // can帧数据内容部分先填充
    if (type == 0x8A) // 使能/禁止MCU主备切换指令
    {
        txCanMsg.MsgType = CAN_MSG_TYPE_SW_EN;
        txCanMsg.FrameId = 0xaa;
        txCanMsg.Data[0] = 0x00;
        txCanMsg.Data[1] = num;
        txCanMsg.Data[2] = num;
        txCanMsg.Data[3] = 0xaa;
        txCanMsg.Data[4] = 0xaa;
        txCanMsg.Data[5] = 0xaa;
        if (id < 2)
        {
            frametype = 0x3;
            nodeadd = 0x11;
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            memcpy(stCanMng[id].TxFm.data, &txCanMsg, sizeof(txCanMsg));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }
    }
    else if (type == 0x8D)
    {
        txCanMsg.MsgType = CAN_MSG_TYPE_SMP_REQ | num;
        txCanMsg.FrameId = 0xaa;
        txCanMsg.Data[0] = 0xaa;
        txCanMsg.Data[1] = 0xaa;
        txCanMsg.Data[2] = 0x54;
        txCanMsg.Data[3] = 0xaa;
        txCanMsg.Data[4] = 0xaa;
        txCanMsg.Data[5] = 0xaa;
        if (id < 2)
        {
            frametype = 0x3;
            nodeadd = 0x11;
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            memcpy(stCanMng[id].TxFm.data, &txCanMsg, sizeof(txCanMsg));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error! nbytes=%d\n", stCanMng[id].id, nbytes);
            }
        }
    }
    else if (type == 0x8e) // CAN OC
    {
        txCanData[0] = 0x21;
        txCanData[1] = 0x00;
        txCanData[2] = 0x00;
        txCanData[3] = 0x20;
        txCanData[4] = 0x20;
        txCanData[5] = 0xAA;
        txCanData[6] = 0xAA;
        txCanData[7] = 0xAA;
        if (id < 2)
        {
            frametype = 0x3;
            nodeadd = 0x11;
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            if (num > 43)
            {
                return;
            }
            else
            {
                txCanData[1] = num;
            }
            //			cancktest_yaokongpraFunc(&txCanData[3],sw);
            memcpy(stCanMng[id].TxFm.data, &txCanData[0], sizeof(txCanData));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }

        usleep(50);
    }
    else if (type == 0x8f) // CAN OC 脉冲控制
    {
        txCanData[0] = 0x20;
        txCanData[1] = 0x00;
        txCanData[2] = 0x00;
        txCanData[3] = 0x01;
        txCanData[4] = 0x01;
        txCanData[5] = 0xAA;
        txCanData[6] = 0xAA;
        txCanData[7] = 0xAA;
        if (id < 2)
        {
            frametype = 0x3;
            nodeadd = 0x11;
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            if (num > 43)
            {
                return;
            }
            else
            {
                txCanData[1] = num;
            }
            if ((sw != 1) && (sw != 0))
            {
                LOG("sw error!\n");
                return;
            }
            txCanData[2] = 0x00;
            txCanData[3] = sw;
            txCanData[4] = sw;
            //			cancktest_yaokongpraFunc(&txCanData[3],sw);
            memcpy(stCanMng[id].TxFm.data, &txCanData[0], sizeof(txCanData));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }

        usleep(50);
    }
    else if (type == 0x9A) //  使能/禁止MCU主备切换指令
    {
        txCanMsg.MsgType = CAN_MSG_TYPE_SW_EN;
        txCanMsg.FrameId = 0xaa;
        txCanMsg.Data[0] = 0x00;
        txCanMsg.Data[1] = num;
        txCanMsg.Data[2] = num;
        txCanMsg.Data[3] = 0xaa;
        txCanMsg.Data[4] = 0xaa;
        txCanMsg.Data[5] = 0xaa;
        if (id < 2)
        {
            frametype = 0x3;
            nodeadd = 0x12;
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            memcpy(stCanMng[id].TxFm.data, &txCanMsg, sizeof(txCanMsg));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }
    }
    else if (type == 0x9D) // OC 请求
    {
        txCanMsg.MsgType = CAN_MSG_TYPE_SMP_REQ | num; // OCchId表示组号，取值1~6
        txCanMsg.FrameId = 0xaa;
        txCanMsg.Data[0] = 0xaa;
        txCanMsg.Data[1] = 0xaa;
        txCanMsg.Data[2] = 0x54;
        txCanMsg.Data[3] = 0xaa;
        txCanMsg.Data[4] = 0xaa;
        txCanMsg.Data[5] = 0xaa;
        if (id < 2)
        {
            frametype = 0x3;
            nodeadd = 0x12;
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            memcpy(stCanMng[id].TxFm.data, &txCanMsg, sizeof(txCanMsg));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error! nbytes=%d\n", stCanMng[id].id, nbytes);
            }
        }
    }
    else if (type == 0x9e) // CAN OC 脉冲控制
    {
        txCanData[0] = 0x21;
        txCanData[1] = 0x00;
        txCanData[2] = 0x00;
        txCanData[3] = 0x20;
        txCanData[4] = 0x20;
        txCanData[5] = 0xAA;
        txCanData[6] = 0xAA;
        txCanData[7] = 0xAA;
        if (id < 2)
        {
            frametype = 0x3;
            nodeadd = 0x12;
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            if (num > 43)
            {
                return;
            }
            else
            {
                txCanData[1] = num;
            }
            //			cancktest_yaokongpraFunc(&txCanData[3],sw);
            memcpy(stCanMng[id].TxFm.data, &txCanData[0], sizeof(txCanData));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }

        usleep(50);
    }
    else if (type == 0x9f) // CAN OC 电平控制
    {
        txCanData[0] = 0x20;
        txCanData[1] = 0x00;
        txCanData[2] = 0x00;
        txCanData[3] = 0x01;
        txCanData[4] = 0x01;
        txCanData[5] = 0xAA;
        txCanData[6] = 0xAA;
        txCanData[7] = 0xAA;
        if (id < 2)
        {
            frametype = 0x3;
            nodeadd = 0x12;
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            if (num > 43)
            {
                return;
            }
            else
            {
                txCanData[1] = num;
            }
            if ((sw != 1) && (sw != 0))
            {
                LOG("sw error!\n");
                return;
            }
            txCanData[2] = 0x00;
            txCanData[3] = sw;
            txCanData[4] = sw;
            //			cancktest_yaokongpraFunc(&txCanData[3],sw);
            memcpy(stCanMng[id].TxFm.data, &txCanData[0], sizeof(txCanData));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }

        usleep(50);
    }
    else if (type == 0xAA) // reset cmd
    {
        txCanData[0] = 0x0A;
        txCanData[1] = 0x2A;
        txCanData[2] = 0xA8;
        txCanData[3] = 0x58;
        txCanData[4] = 0x0;
        txCanData[5] = 0x0;
        txCanData[6] = 0x0;
        txCanData[7] = 0x0;
        if (id < 2)
        {
            frametype = 0x3;
            if (num == 0)
            {
                nodeadd = 0x06;
            }
            else
            {
                nodeadd = 0x07;
            }
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            memcpy(stCanMng[id].TxFm.data, &txCanData[0], sizeof(txCanData));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }
    }
    else if (type == 0xBB) // 单机遥测指令
    {
        txCanData[0] = 0x0A;
        txCanData[1] = 0x1A;
        txCanData[2] = 0x28;
        txCanData[3] = 0x08;
        txCanData[4] = 0x0;
        txCanData[5] = 0x0;
        txCanData[6] = 0x0;
        txCanData[7] = 0x0;
        if (id < 2)
        {
            frametype = 0x3;
            if (num == 0)
            {
                nodeadd = 0x06;
            }
            else
            {
                nodeadd = 0x07;
            }
            if (id == 0)
            {
                ABflag = 0;
            }
            else
            {
                ABflag = 1;
            }
            danfuframeflag = 0x0;
            stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
            memcpy(stCanMng[id].TxFm.data, &txCanData[0], sizeof(txCanData));
            stCanMng[id].TxFm.can_dlc = 8;

            nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
            if (nbytes != sizeof(struct can_frame))
            {
                LOG("can%d write error!\n", stCanMng[id].id);
            }
        }
    }
    else if (type == 0xDD) // 遥测数据
    {
        txMuxData[0] = 0x00;
        txMuxData[1] = 0x85;
        txMuxData[2] = 0xbb;
        txMuxData[3] = 0x00;
        sum += txMuxData[2];
        sum += txMuxData[3];
        for (i_temp = 4; i_temp < 135; i_temp++)
        {
            txMuxData[i_temp] = i_temp;
            sum += txMuxData[i_temp];
        }
        txMuxData[135] = sum;
        LOG("yaokong zhen  sum=0x%02x ...\n\r", sum);
        if (id < 2)
        {
            while (1)
            {
                frametype = 0x3;
                if (num == 0)
                {
                    nodeadd = 0x06;
                }
                else
                {
                    nodeadd = 0x07;
                }
                if (id == 0)
                {
                    ABflag = 0;
                }
                else
                {
                    ABflag = 1;
                }
                if (tx_cnt >= 8)
                {
                    stCanMng[id].TxFm.can_dlc = 8;
                    if (tx_cnt == 136)
                    {
                        danfuframeflag = 0x1;
                    }
                    else if (tx_cnt == 8)
                    {
                        danfuframeflag = 0x3;
                    }
                    else
                    {
                        danfuframeflag = 0x2;
                    }
                }
                else
                {
                    danfuframeflag = 0x3;
                    stCanMng[id].TxFm.can_dlc = tx_cnt % 8;
                }
                stCanMng[id].TxFm.can_id = (danfuframeflag | (ABflag << 2) | (nodeadd << 3) | (frametype << 8)); // ID number
                memcpy(stCanMng[id].TxFm.data, &txMuxData[0 + num_cnt], stCanMng[id].TxFm.can_dlc);
                num_cnt += stCanMng[id].TxFm.can_dlc;
                nbytes = write(stCanMng[id].sockid, &stCanMng[id].TxFm, sizeof(struct can_frame)); // ���� frame[0]
                if (nbytes != sizeof(struct can_frame))
                {
                    LOG("can%d write error!\n", stCanMng[id].id);
                }

                if (tx_cnt >= 8)
                {
                    tx_cnt -= 8;
                    if (tx_cnt == 0)
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
                usleep(2000);
            }
        }
    }
    else
    {
        LOG("type=%d error!\n", type);
    }
}

// FPGA核要求读写访问都按16bit使用，offset和len都是以16bit字为单位的
int mram_read(unsigned int offset, unsigned int len, unsigned short *bufShort)
{
    if (g_mram_addr == NULL)
    {
        LOG("mram_read: g_mram_addr is NULL!\n");
        return -1;
    }

    if (NULL == bufShort)
    {
        LOG("mram_read: bufShort is NULL!\n");
        return -1;
    }

    if (((len + offset) > (MRAM_SIZE / 2)) || (0 == len))
    {
        LOG("mram_read: len must>0 and (len+offset)<=0x100000. len=%d,offset =%d\n", len, offset);
        return -1;
    }

    for (unsigned int i = 0; i < len; i++)
    {
        *(bufShort + i) = *(unsigned short *)(g_mram_addr + (offset + i) * 2);
    }

    return OK;
}

// FPGA核要求读写访问都按16bit使用，offset和len都是以16bit字为单位的
int mram_write(unsigned int offset, unsigned int len, unsigned short *bufShort)
{
    if (g_mram_addr == NULL)
    {
        LOG("mram_write: g_mram_addr is NULL!\n");
        return -1;
    }

    if (NULL == bufShort)
    {
        LOG("mram_write: bufShort is NULL!\n");
        return -1;
    }

    if (((len + offset) > (MRAM_SIZE / 2)) || (0 == len))
    {
        LOG("mram_write: len must>0 and (len+offset)<0x100000. len=%d,offset =%d\n", len, offset);
        return -1;
    }

    for (int i = 0; i < len; i++)
    {
        *(unsigned short *)(g_mram_addr + (offset + i) * 2) = *(bufShort + i);
    }

    return OK;
}

void resetr_test(void)
{
    unsigned int *g_resetr_addr = NULL;
    unsigned int value = 0;
    int resetrHdl = -1;
#if 0
	g_resetr_addr = (RESETR_BASE_ADDR, RESETR_SIZE, &resetrHdl);
	if(g_resetr_addr == NULL)
	{
		LOG("mram_test: mmap fail!\n");
		return;
	}
	usleep(100);
        value = *(g_resetr_addr+VER_DATE_ADDR);
	 LOG("version  DATE:0x%4x...\n",value);
        value = *(g_resetr_addr+VER_TIME_ADDR);
	 LOG("version  TIME:0x%4x...\n",value);
        value = *(g_resetr_addr+VER_WDTCNT_ADDR);
	 LOG("version  WDT cnt:0x%4x...\n",value);
        value = *(g_resetr_addr+VER_SYSCNT_ADDR);
	 LOG("version  SYS cnt:0x%4x...\n",value);	 
	devm_unmap(g_resetr_addr, RESETR_SIZE, &resetrHdl);
#else
    g_resetr_addr = devm_map(RESETR_BASE_ADDR, 0x4, &resetrHdl);
    if (g_resetr_addr == NULL)
    {
        LOG("mram_test: mmap fail!\n");
        return;
    }
    usleep(100);
    value = *(g_resetr_addr);
    LOG("version  DATE:0x%4x...\n", value);
    devm_unmap(g_resetr_addr, 0x4, &resetrHdl);

    g_resetr_addr = devm_map((RESETR_BASE_ADDR + VER_TIME_ADDR), 0x4, &resetrHdl);
    if (g_resetr_addr == NULL)
    {
        LOG("mram_test: mmap fail!\n");
        return;
    }
    usleep(100);
    value = *(g_resetr_addr);
    LOG("version  TIME:0x%4x...\n", value);
    devm_unmap(g_resetr_addr, 0x4, &resetrHdl);

    g_resetr_addr = devm_map((RESETR_BASE_ADDR + VER_WDTCNT_ADDR), 0x4, &resetrHdl);
    if (g_resetr_addr == NULL)
    {
        LOG("mram_test: mmap fail!\n");
        return;
    }
    usleep(100);
    value = *(g_resetr_addr);
    LOG("version  WDTCNT:0x%4x...\n", value);
    devm_unmap(g_resetr_addr, 0x4, &resetrHdl);

    g_resetr_addr = devm_map((RESETR_BASE_ADDR + VER_SYSCNT_ADDR), 0x4, &resetrHdl);
    if (g_resetr_addr == NULL)
    {
        LOG("mram_test: mmap fail!\n");
        return;
    }
    usleep(100);
    value = *(g_resetr_addr);
    LOG("version  SYSCNT:0x%4x...\n", value);
    devm_unmap(g_resetr_addr, 0x4, &resetrHdl);
#endif
}

// mram测试函数，value是以16bit字为单位
void mram_test(unsigned char rdOrWt, unsigned int addr, unsigned int value)
{
    int mramHdl = -1;
    unsigned short *pbuff = NULL;
    unsigned short wbuf[2];

    g_mram_addr = devm_map(MRAM_BASE_ADDR, MRAM_SIZE, &mramHdl);
    if (g_mram_addr == NULL)
    {
        LOG("mram_test: mmap fail!\n");
        return;
    }

    if (rdOrWt == 0) // 读
    {
        pbuff = (unsigned short *)malloc(value * sizeof(unsigned short) + 4); // 4为保护长度
        if (NULL == pbuff)
        {
            LOG("mram_test: malloc len=%u fail!\n",
                   value * sizeof(unsigned short) + 4);
            return;
        }

        mram_read(addr, value, pbuff);
        print_data_16bit(pbuff, value);

        free(pbuff);
    }
    else // 写
    {
        // 将value转化为2个16bit字，大端放置
        wbuf[0] = value >> 16;
        wbuf[1] = value & 0xffff;
        mram_write(addr, 2, wbuf);
    }

    devm_unmap(g_mram_addr, PPS_MEM_LEN, &mramHdl);
}

int mgpioHdl = -1;
void *g_gpio_addr = NULL;
void goioinout_init_test(unsigned char rdOrWt, unsigned int num, unsigned int value)
{
    unsigned int m_value;
    LOG("rd:%d  num:%d  value:%d ...\n\r", rdOrWt, num, value);
    if (g_gpio_addr == NULL)
    {
        g_gpio_addr = devm_map(GPIOINOUT_BASE_ADDR, 0x10000, &mgpioHdl);
        if (g_gpio_addr == NULL)
        {
            LOG("mgpio_test: mgpio fail!\n");
            return;
        }
    }

    if (rdOrWt == 0)
    {
        m_value = *(unsigned int *)(g_gpio_addr + 0x70);
        m_value = (m_value & (1 << num));
        //		LOG("goioinout_init_test %d...\n\r",m_value);
        if (m_value == (1 << num))
        {
            LOG("1\n\r");
        }
        else
        {
            LOG("0\n\r");
        }
    }
    else
    {
        m_value = *(unsigned int *)(g_gpio_addr + 0x0c);
        if ((m_value & (1 << num)) == (value << num))
        {
            LOG("in out gpiovalue same...\n\r");
        }
        else
        {
            if (value == 1)
            {
                m_value |= (1 << num);
                //				LOG("22222,%d,%d...\n\r",num,m_value);
            }
            else
            {
                m_value &= (~(1 << num));
                //				LOG("33333,%d,%d...\n\r",num,m_value);
            }
            *(unsigned int *)(g_gpio_addr + 0x0c) = m_value;
        }
    }
}

void goioinout_free_test(void)
{

    if (g_gpio_addr == NULL)
    {
        LOG("mgpio_test: mgpio free fail!\n");
        return;
    }
    devm_unmap(g_gpio_addr, 0x10000, &mgpioHdl);
}

// flash 刷新测试
void flash_resume(unsigned int value)
{
    int flashHdl = 0, ret;
    char dev[16] = {0};

    snprintf(dev, sizeof(dev), "/dev/mtd0");
    // 打开设备

    flashHdl = open(dev, O_RDWR);
    if (flashHdl < 0)
    {
        LOG("cannot open %s\n", dev);
        return;
    }

    LOG("open %s success!\n", dev);

    if (value == 0)
    {
        ret = ioctl(flashHdl, MEMRESUME, 0);
    }
    else
    {
        ret = ioctl(flashHdl, MEMRESUME_ZHH, 0);
    }

    close(flashHdl);

    LOG("ioctl MEMRESUME return=%d!\n", ret);

    return;
}

// flash读
void flash_read(unsigned int offset, unsigned int len)
{
    char rdbuf[256] = {0};
    int hdl, rdlen;
    char *flashBufFile = "/home/root/flashrd-tmp";
    char cmd[256] = {0};

    if (len > 256)
        len = 256;

    // 1、先读出head到临时文件 /home/root/flashrd-tmp 中
    snprintf(cmd, sizeof(cmd), "mtd_debug read /dev/mtd3 %d %d %s",
             offset, len, flashBufFile);
    LOG("exec cmd: %s...\n", cmd);
    system_cmd(cmd);

    // 2、打开临时文件 /home/root/flashrd-tmp
    hdl = open(flashBufFile, O_RDONLY);
    if (hdl < 0)
    {
        LOG("cannot open %s\n", flashBufFile);
        return;
    }

    // 3、从临时文件中读出数据
    lseek(hdl, 0, SEEK_SET); // 设置指针位置为0
    rdlen = read(hdl, rdbuf, len);
    if (rdlen < len)
    {
        LOG("read len=%d fail!\n", rdlen);
        close(hdl);
        return;
    }

    print_data(rdbuf, len);

    close(hdl);

    return;
}
// flash写一个u32的字

void flash_write(unsigned int offset, unsigned int value)
{
    int hdl, wtlen;
    char *flashBufFile = "/home/root/flashwt-tmp";
    char cmd[256] = {0};

    // 1、先打开临时文件 /home/root/flashwt-tmp
    hdl = open(flashBufFile, O_RDWR | O_CREAT | O_TRUNC, 0644); //| O_SYNC);
    if (hdl < 0)
    {
        LOG("cannot open %s\n", flashBufFile);
        return;
    }
    // 2、写入临时文件

    wtlen = write(hdl, (char *)&value, sizeof(value));
    if (wtlen < sizeof(value))
    {
        LOG("write fail, wtlen=%d fail!\n", wtlen);
        close(hdl);
        return;
    }
    close(hdl);

    usleep(5000);

    // 3、使用mtd_debug将临时文件中的数据写入mtd3
    snprintf(cmd, sizeof(cmd), "mtd_debug erase /dev/mtd3 0 %d",
             FSH_ERASESIZE);
    LOG("exec cmd: %s...\n", cmd);
    system_cmd(cmd);

    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "mtd_debug write /dev/mtd3 %d %d %s",
             offset, sizeof(value), flashBufFile);
    LOG("exec cmd: %s...\n", cmd);
    system_cmd(cmd);

    // LOG("flash(offset=%d) write words=0x%08x OK!\n", offset, value);

    return;
}

// 用于恢复qspi控制器为读状态，规避切换flash并写入数据后可能引起的reboot失败
void recov_qspi_ctrlor_rd()
{
    char *File = "/home/root/test-mtd0-rd.bin";
    char cmd[256] = {0};

    snprintf(cmd, sizeof(cmd), "mtd_debug read /dev/mtd0 %d %d %s",
             0, 256, File);
    system(cmd);

    return;
}

/************************ 重构功能 ***************************/
update_info g_updateInfo = {0};
update_ctrl g_updateCtrl = {0};

// 读出mtd3中的app星务软件长度字段
int get_mtd3_lenwords()
{
    int hdl, rdlen;
    app_head appHead = {0};

    // 打开设备
    hdl = open("/dev/mtdblock3", O_RDONLY);
    if (hdl < 0)
    {
        LOG("cannot open '/dev/mtdblock3'\n");
        return ERROR;
    }

    // 读出
    // lseek(hdl, 0, SEEK_SET);//设置指针位置为0
    rdlen = read(hdl, (char *)&appHead, sizeof(appHead));
    if (rdlen < sizeof(appHead))
    {
        LOG("read len=%d fail!\n", rdlen);
        close(hdl);
        return ERROR;
    }

    close(hdl);

    return appHead.fileLen;
}

// 设置mtd3中的app星务软件长度字段
int set_mtd3_lenwords(uint32_t len)
{
    int hdl, wtlen;
    app_head appHead = {0};

    appHead.fileLen = len;

    // 打开设备
    hdl = open("/dev/mtdblock3", O_RDONLY);
    if (hdl < 0)
    {
        LOG("cannot open '/dev/mtdblock3'\n");
        return ERROR;
    }

    // 读出
    // lseek(hdl, 0, SEEK_SET);//设置指针位置为0
    wtlen = write(hdl, (char *)&appHead, sizeof(appHead));
    if (wtlen < sizeof(appHead))
    {
        LOG("write fail, wrlen=%d fail!\n", wtlen);
        close(hdl);
        return ERROR;
    }

    close(hdl);

    return OK;
}

// 加载mtd3的app，入参loadSel=0为后台加载，其他值为前台加载
void load_app(int loadSel)
{
    int hdl, rdlen;
    app_head appHead = {0};
    char appPathFile[128] = {0};
    char cmd[256] = {0};

    // 1、先读出head到 /home/root/app-head-load 文件中
    // 命令格式：mtd_debug read /dev/mtd3 0 17438060 app
    snprintf(cmd, sizeof(cmd), "mtd_debug read /dev/mtd3 0 %d %s",
             sizeof(appHead), APP_HEAD_LD_FILE);
    system(cmd);

    // 2、打开文件 /home/root/app-head-load
    hdl = open(APP_HEAD_LD_FILE, O_RDONLY);
    if (hdl < 0)
    {
        LOG("cannot open %s\n", APP_HEAD_LD_FILE);
        return;
    }

    lseek(hdl, 0, SEEK_SET); // 设置指针位置为0
    rdlen = read(hdl, (char *)&appHead, sizeof(appHead));
    if (rdlen < sizeof(appHead))
    {
        LOG("read len=%d fail!\n", rdlen);
        close(hdl);
        return;
    }

    close(hdl);

    // 根据长度字段的内容，读取app，存入内存文件 /home/root/app-test
    memset(cmd, 0, sizeof(cmd));
    snprintf(appPathFile, sizeof(appPathFile), "%s%s", APP_EXEC_PATH, VER_NAME_APP);
    // 命令格式：mtd_debug read /dev/mtd3 0 17438060 app
    snprintf(cmd, sizeof(cmd), "mtd_debug read /dev/mtd3 %d %d %s",
             sizeof(appHead), appHead.fileLen, appPathFile);
    system(cmd);

    // 执行app
    /*
    chmod +x /run/media/mmcblk0p1/prj0878-soc-post-v1.00
    start-stop-daemon -S -o --background -x /run/media/mmcblk0p1/prj0878-soc-post-v1.00
    */
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "chmod +x %s", appPathFile);
    system(cmd);

    memset(cmd, 0, sizeof(cmd));
    if (loadSel == 0)
    {
        snprintf(cmd, sizeof(cmd), "start-stop-daemon -S -o --background -x %s",
                 appPathFile);
    }
    else
    {
        snprintf(cmd, sizeof(cmd), "exec %s", appPathFile);
    }
    system(cmd);

    return;
}

// flash片选切换到非启动flash，为后续轮询或重构准备
int flash_startup_sw()
{
    uint8_t curRdFsh;
    // char cmd[128];
    // snprintf(cmd, sizeof(cmd), "cat /proc/mtd");
    // system(cmd);

    // 检查当前flash区是否为非启动区
    curRdFsh = (uint8_t)gpio_read(GPIO_BOOT_STATE);
    if (curRdFsh == g_updateInfo.BootFlash) // 启动区
    {
        // 需要两次下降沿才能正常切换
        gpio_write(GPIO_BOOT_SW, 1);
        usleep(1000);
        gpio_write(GPIO_BOOT_SW, 0);
        usleep(1000);
        gpio_write(GPIO_BOOT_SW, 1);
        usleep(1000);
        gpio_write(GPIO_BOOT_SW, 0);
        usleep(1000);

        usleep(5000);

        // 再次确认是否已经切换
        curRdFsh = (uint8_t)gpio_read(GPIO_BOOT_STATE);
        if (curRdFsh == g_updateInfo.BootFlash)
        {
            LOG("flash_startup_sw: flash switch fail!\n");
            return ERROR;
        }

        // flash_resume(0);
        // recov_qspi_ctrlor_rd();
    }

    return OK;
}

int create_mtd3_head_file(app_head *appHead)
{
    int fid, wtlen;

    // 打开app-head二进制文件
    fid = open(APP_HEAD_UP_FILE, O_RDWR | O_CREAT, 0644); // |O_DIRECT);
    if (fid < 0)
    {
        LOG("%s file open fail!\n", APP_HEAD_UP_FILE);
        return ERROR;
    }

    wtlen = write(fid, appHead, sizeof(app_head));
    if (wtlen < sizeof(app_head))
    {
        LOG("write %s fail, wtlen=%d.\n", APP_HEAD_UP_FILE, wtlen);
        return ERROR;
    }

    close(fid);
    return OK;
}
// wtMtd为要写的mtd，其他mtd需要保护

void flash_lock(unsigned int wtMtd)
{
    int flashHdl = 0, ret;
    char dev[16] = {0};
    struct erase_info_user ersLock[4]; // mtd3更新的时候，对mtd0~2进行擦除锁
    struct mtd_info_user mtdinfo = {0};

    // 只保护mtd0~mtd3
    if (wtMtd > 3)
    {
        LOG("mtdn=%u error!\n", wtMtd);
        return;
    }

    /*
        ersLock[0].start = 0;
        ersLock[0].length = 0x1400000;
        ersLock[1].start = 0;
        ersLock[1].length = 0x80000;
        ersLock[2].start = 0;
        ersLock[2].length = 0x3000000;
        ersLock[3].start = 0;
        ersLock[3].length = 0x1000000;
    */
    // 所有不写的mtd均上锁
    for (int i = 0; i < 4; i++)
    {
        if (i != wtMtd) // 对不写的mtd需要上锁
        {
            snprintf(dev, sizeof(dev), "/dev/mtd%d", i);
            flashHdl = open(dev, O_RDWR);
            if (flashHdl < 0)
            {
                LOG("cannot open %s\n", dev);
                continue;
            }

            // 获取mtd长度信息
            ioctl(flashHdl, MEMGETINFO, &mtdinfo);

            // 配置加锁
            ersLock[i].start = 0;
            ersLock[i].length = mtdinfo.size;
            ret = ioctl(flashHdl, MEMLOCK, &ersLock[i]);
            LOG("%s ioctl MEMLOCK ret=%d\n", dev, ret);

            close(flashHdl);
        }
    }

    LOG("flash locked excetp mtd=%d.\n", wtMtd);
}

// 写完wtMtd后，其他mtd需要解锁
void flash_unlock(unsigned int wtMtd)
{
    int flashHdl = 0, ret;
    char dev[16] = {0};
    struct erase_info_user ersLock[4];
    struct mtd_info_user mtdinfo = {0};

    // 只保护mtd0~mtd3
    if (wtMtd > 3)
    {
        LOG("mtdn=%u error!\n", wtMtd);
        return;
    }

    // 对所有不写的mtd解锁
    for (int i = 0; i < 4; i++)
    {
        if (i != wtMtd)
        {
            snprintf(dev, sizeof(dev), "/dev/mtd%d", i);
            flashHdl = open(dev, O_RDWR);
            if (flashHdl < 0)
            {
                LOG("cannot open %s\n", dev);
                continue;
            }

            // 获取mtd长度信息
            ioctl(flashHdl, MEMGETINFO, &mtdinfo);

            // 配置解锁
            ersLock[i].start = 0;
            ersLock[i].length = mtdinfo.size;
            ret = ioctl(flashHdl, MEMUNLOCK, &ersLock[i]);
            LOG("%s ioctl MEMUNLOCK ret=%d\n", dev, ret);

            close(flashHdl);
        }
    }

    LOG("flash unlocked excetp mtd=%d.\n", wtMtd);
}

// 对某个flash的mtd0~mtd3全部加锁
void flash_lock_all()
{
    int flashHdl = 0, ret;
    char dev[16] = {0};
    struct erase_info_user ersLock[4];
    struct mtd_info_user mtdinfo = {0};

    // 所有mtd均上锁
    for (int i = 0; i < 4; i++)
    {
        snprintf(dev, sizeof(dev), "/dev/mtd%d", i);
        flashHdl = open(dev, O_RDWR);
        if (flashHdl < 0)
        {
            LOG("cannot open %s\n", dev);
            continue;
        }

        // 获取mtd长度信息
        ioctl(flashHdl, MEMGETINFO, &mtdinfo);

        // 配置加锁
        ersLock[i].start = 0;
        ersLock[i].length = mtdinfo.size;
        ret = ioctl(flashHdl, MEMLOCK, &ersLock[i]);
        LOG("%s ioctl MEMLOCK ret=%d\n", dev, ret);

        close(flashHdl);
    }

    LOG("flash locked all mtd.\n");
}

// 对某个flash的mtd0~mtd3全部解锁
void flash_unlock_all()
{
    int flashHdl = 0, ret;
    char dev[16] = {0};
    struct erase_info_user ersLock[4];
    struct mtd_info_user mtdinfo = {0};

    // 对所有mtd0~mtd3解锁
    for (int i = 0; i < 4; i++)
    {
        snprintf(dev, sizeof(dev), "/dev/mtd%d", i);
        flashHdl = open(dev, O_RDWR);
        if (flashHdl < 0)
        {
            LOG("cannot open %s\n", dev);
            continue;
        }

        // 获取mtd长度信息
        ioctl(flashHdl, MEMGETINFO, &mtdinfo);

        // 配置解锁
        ersLock[i].start = 0;
        ersLock[i].length = mtdinfo.size;
        ret = ioctl(flashHdl, MEMUNLOCK, &ersLock[i]);
        LOG("%s ioctl MEMUNLOCK ret=%d\n", dev, ret);

        close(flashHdl);
    }

    LOG("flash unlocked all mtd.\n");
}

void qspi_ctrl(uint8_t type, uint32_t offset, uint32_t value)
{
    unsigned int *qspiAddr;
    int rtcEnHdl = -1;

    offset = offset & 0xfc; // 4字节对齐

    if (offset >= 0x100)
    {
        LOG("qspi_ctrl reg offset=0x%02x error!\n", offset);
        return;
    }
    // RTC使能接口使用的交互区映射
    qspiAddr = (unsigned int *)devm_map(QSPI_Ctrler_ADDR, 0x1000, &rtcEnHdl);
    if (qspiAddr == NULL)
    {
        LOG("qspi_ctrl mmap fail!\n");
        return;
    }

    if (type == 0) // RTC使能接口使用的交互区映射
    {
        for (int i = 0; i < QSPI_Ctrler_MLEN; i = i + 4)
        {
            LOG("read reg[0x%02x]=0x%08x\n", i, *(qspiAddr + i));
        }
    }
    else if (type == 1) // 读某个地址一个32bit字
    {
        LOG("read reg[0x%02x]=0x%08x\n", offset, *(qspiAddr + offset));
    }
    else if (type == 2) // 写某个地址一个32bit字
    {
        *(qspiAddr + offset) = value;
        LOG("write reg[0x%02x]=0x%08x\n", offset, value);
    }

    devm_unmap((void *)qspiAddr, 0x1000, &rtcEnHdl);
}

void flash_lock_ctrl(unsigned char lock, unsigned int mtdn)
{
    if (lock == 0)
    {
        flash_unlock(mtdn);
    }
    else if (lock == 1)
    {
        flash_lock(mtdn);
    }
    else if (lock == 0xff)
    {
        if (mtdn != 0)
        {
            flash_lock_all();
        }
        else
        {
            flash_unlock_all();
        }
    }
}

/*
BIT15: I2C_EN4 输出.默认0，使能为1
BIT14: I2C_EN3 输出.默认0，使能为1
BIT13: I2C_EN2 输出.默认0，使能为1
BIT12: I2C_EN1 输出.默认0，使能为1
BIT11: RS422_EN4 输出.默认1，使能为0
BIT10: RS422_EN3 输出.默认1，使能为0
BIT9: RS422_EN2 输出.默认1，使能为0
BIT8: RS422_EN1 输出.默认1，使能为0
BIT6: LVDS_EN 输出.默认1，使能为0
BIT5: GPIO_EN 输出.默认1，使能为0时，RS232才能使用
BIT4: SPI_EN 输出.默认1，使能为0时，RS232才能使用
BIT3: FPGA内部RTC_RST信号.默认0.
BIT2: FPGA内部软件看门狗RUN信号.默认0.
BIT1: FPGA内部软件看门狗EN信号.默认0不使能.
BIT0: FPGA内部LED灯快闪慢闪控制信号.默认0慢闪.
*/

void enable(unsigned char type, unsigned char data)
{
#if 0
	unsigned int *enbAddr, value, maskbit;
	int rtcEnHdl = -1;
	
	// RTC使能接口使用的交互区映射
	enbAddr = (unsigned int *)devm_map(PPS_RTC_EN_BASE, PPS_MEM_LEN, &rtcEnHdl);
	if(enbAddr == NULL)
	{
		LOG("devm_map return NULL!\n");
		return;
	}

	if(type>31)
	{
		LOG("bitN(=%u) must <=31!\n", type);
		
		//只读寄存器值
		LOG("READ enable reg=0x%08x\n", *enbAddr);
	}
	else
	{
	//写寄存器值
		value = *enbAddr;  // 先读出寄存器值
		maskbit = 0x1<<type;

		if(data)
		{
			*enbAddr = value | maskbit;
		}
		else
		{
			*enbAddr = value & (~maskbit);
		}

		usleep(10);
		LOG("WRITE enable reg=0x%08x\n", *enbAddr);
	}

	devm_unmap((void *)enbAddr, PPS_MEM_LEN, &rtcEnHdl);
#else
    char path[64];
    memset(path, 0, sizeof(path));
    if (type > 63)
    {
        LOG("bitN(=%u) must <=64!\n", type);
    }
    else
    {

        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", PS_GPO_BASE + type);
        if (-1 == access(path, F_OK))
        {
            gpio_export(PS_GPO_BASE + type);
            gpio_direction(PS_GPO_BASE + type, GPIO_DIR_OUT);
        }

        gpio_write(PS_GPO_BASE + type, data);
    }
#endif
}

uint16_t IO_READ16(uint32_t addr)
{
    uint32_t data;
    data = *(uint32_t *)addr;
    return (uint16_t)(data & 0xffff);

    // return *(uint16_t*)addr;
}

void IO_WRITE16(uint32_t addr, uint16_t value)
{
    uint32_t data = (uint32_t)value;
    data = data & 0xffff;

    *(uint32_t *)addr = data;

    //*(uint16_t *)addr = value;
}

// 字符串解析函数
int str_parse(char *param_strs, st_opt_t *opt)
{
    char *str;
    int len1, len2, len3, len4, len5, len6, len7, len8;

    if ((NULL == param_strs) || (NULL == opt))
    {
        LOG("param_strs is null\n");
        return ERR;
    }
    str = param_strs;

    while (*str != '\0')
    {
        if (*str != ' ')
        {
            str++;
        }
        else
        {
            break;
        }
    }
    len1 = str - param_strs;
    strncpy(opt->select, param_strs, len1);
    if (*str == '\0')
    {
        return OK;
    }
    /*找第二次的头*/
    while (*str != '\0')
    {
        if (*str == ' ')
        {
            str++;
        }
        else
        {
            break;
        }
    }
    len2 = str - param_strs;
    /*计算第二次的长度*/
    while (*str != '\0')
    {
        if (*str != ' ')
        {
            str++;
        }
        else
        {
            break;
        }
    }
    len3 = str - param_strs;

    strncpy(opt->fval, (param_strs + len2), len3);
    if (*str == '\0')
    {
        return OK;
    }
    /*找第三次头*/
    while (*str != '\0')
    {
        if (*str == ' ')
        {
            str++;
        }
        else
        {
            break;
        }
    }
    len4 = str - param_strs;

    /*计算第三次的长度*/
    while (*str != '\0')
    {
        if (*str != ' ')
        {
            str++;
        }
        else
        {
            break;
        }
    }
    len5 = str - param_strs;
    strncpy(opt->sval, (param_strs + len4), len5);
    if (*str == '\0')
    {
        return OK;
    }

    /*找第四次的头*/
    while (*str != '\0')
    {
        if (*str == ' ')
        {
            str++;
        }
        else
        {
            break;
        }
    }
    len6 = str - param_strs;

    /*计算第四次的长度*/
    while (*str != '\0')
    {
        if (*str != ' ')
        {
            str++;
        }
        else
        {
            break;
        }
    }
    len7 = str - param_strs;
    strncpy(opt->tval, (param_strs + len6), len7);
    if (*str == '\0')
    {
        return OK;
    }

    /*找第五次的头*/
    while (*str != '\0')
    {
        if (*str == ' ')
        {
            str++;
        }
        else
        {
            break;
        }
    }
    len8 = str - param_strs;
    strcpy(opt->endrval, (param_strs + len8)); // 第五次拷到字符串结束，不用指定长度
    return OK;
}

void help_proc()
{
    LOG("plddr3:      execute PL DDR3 test.\n");
    LOG("pulse:       execute second pulse test.\n");
    LOG("i2c ID:      execute i2c test. ID=i2c id(0~3, 4=all).\n");
    LOG("spi ID:      execute spi test. ID=spi id(0~1, 2=all).\n");
    LOG("rs422 ID:    execute rs422 test. ID=rs422 id(0~7, 8=all).\n");
    LOG("rs232 ID:    execute rs232 test. ID=rs232 id(0~1, 2=all).\n");
    LOG("gpi ID:      execute gpi test. ID=gpi id(0~7, 8=all).\n");
    LOG("gpo ID data: execute gpo test. ID=gpo id(0~7, 8=all).\n");
    LOG("                               data=(0~1).\n");
    LOG("                               If ID=8 and data=0 then output 0x55.\n");
    LOG("                               If ID=8 and data=1 then output 0xaa.\n");
    LOG("can ID:      execute can bus test. ID=can id(0~1, 2=all).\n");
    LOG("eth:         execute ethernet test.\n");
    LOG("emmc:        execute emmc test.\n");
    LOG("mram:        execute mram test.\n");
    LOG("dog:         execute watch dog and dual-boot test.\n");
    LOG("all:         execute all interfaces and devices test.\n");
    LOG("help:        show all commands and usage.\n");
    LOG("quit:        exit program.\n\n");
}

int keybd_hit()
{
    char inmsg[STR_MAX_LEN];
    unsigned char type, id, data, addr, reg, sw, num;
    unsigned int addr32, value, chId32, u32data;
    int pktCnt;
    st_opt_t opt;
    struct timeval tv_timeout;
    tv_timeout.tv_sec = 0;
    tv_timeout.tv_usec = 10000; // 等待10ms

    /*
    fd_set是一个数组的宏定义，实际上是一long类型的数组，每一个数组元素都能
    与一打开的文件句柄(socket、文件、管道、设备等)建立联系，当调用select()时，
    由内核根据IO状态修改fd_set的内容，由此来通知执行了select()的进程哪个句柄可读。

    */
    fd_set fs_stio;
    FD_ZERO(&fs_stio);
    FD_SET(STDIN_FILENO, &fs_stio); // 将STDIN_FILENO设备加入fs_stio集合，建立联系

    select(STDIN_FILENO + 1, &fs_stio, NULL, NULL, &tv_timeout);
    if (FD_ISSET(STDIN_FILENO, &fs_stio))
    {
        memset(inmsg, 0, sizeof(inmsg));
        memset(&opt, 0, sizeof(st_opt_t));

        fgets(inmsg, sizeof(inmsg), stdin);
        if (strlen(inmsg) <= 0)
        {
            LOG("Input CMD len error!\n");
            return OK;
        }

        if (ERR == str_parse(inmsg, &opt))
        {
            LOG("Input CMD parameter error!\n");
            return OK;
        }

        // 对输入命令进行判别
        if (!strncmp(opt.select, "help", 4))
        {
            help_proc();
        }
        else if (!strncmp(opt.select, "i2cbase", 7)) // i2c基本功能测试
        {
            type = strtoll(opt.fval, NULL, 0);   // read or write: 0 read, 1 write
            id = strtoll(opt.sval, NULL, 0);     // i2c id
            addr = strtoll(opt.tval, NULL, 0);   // i2c addr
            reg = strtoll(opt.endrval, NULL, 0); // reg addr
            i2c_test(type, id, addr, reg);
        }
        else if (!strncmp(opt.select, "i2cread", 7)) // i2c基本功能测试
        {
            id = strtoll(opt.fval, NULL, 0);   // i2c id
            addr = strtoll(opt.sval, NULL, 0); // i2c addr
            reg = strtoll(opt.tval, NULL, 0);  // reg addr
            data = strtoll(opt.endrval, NULL, 0);
            i2c_test_t(0, id, addr, reg, data);
        }
        else if (!strncmp(opt.select, "i2cwrite", 8)) // i2c基本功能测试
        {
            id = strtoll(opt.fval, NULL, 0);   // i2c id
            addr = strtoll(opt.sval, NULL, 0); // i2c addr
            reg = strtoll(opt.tval, NULL, 0);  // reg addr
            data = strtoll(opt.endrval, NULL, 0);
            i2c_test_t(1, id, addr, reg, data);
        }
        else if (!strncmp(opt.select, "ipmbtest", 8)) // i2c基本功能测试
        {
            // BoardIPMBTestFunc(0x0, 0x1);
            unsigned char *p_ackInfo = NULL;
            int sendBufLen = 0;
#if 0
            for (int i = 0; i <= 4; i++)
                if (ipmb_func(i, &p_ackInfo, &sendBufLen) < 0)
                {
                    LOG("ipmb_func error:i2c -%d", i);
                    continue;
                }
#else
            if (ipmb_func(0, &p_ackInfo, &sendBufLen) < 0)
            {
                LOG("ipmb_func error:i2c -%d", 0);
            }
#endif
        }
        else if (!strncmp(opt.select, "spi", 3))
        {
            // id = strtoll(opt.fval, NULL, 0);
            spi_test();
        }
        else if (!strncmp(opt.select, "uartsendall", 11))
        {
            for (int i = 0; i < 32; i++)
                rs422_send(i);
        }
        else if (!strncmp(opt.select, "sendzigbee", 10))
        {
            send_zigbee_to_light();
        }
        else if (!strncmp(opt.select, "openjoin", 8))
        {
            open_join_network();
        }
        else if (!strncmp(opt.select, "uart", 4))
        {
            type = strtoll(opt.fval, NULL, 0); // 测试类型
            id = strtoll(opt.sval, NULL, 0);   // uart id
            pktCnt = strtoll(opt.tval, NULL, 0);
            value = strtoll(opt.endrval, NULL, 0); // usleep时间，仅用于rs422_savefile_rx_delay

            if (type == 0) // SOC做还回
            {
                rs422_loopback(id, value);
            }
            else if (type == 1) // soc单发
            {
                rs422_send(id);
            }
            else if (type == 2) // 自增数满带宽发送
            {
                rs422_selfinc(id, pktCnt);
            }
            else if (type == 3) // 接收存文件
            {
                rs422_savefile(id, pktCnt);
            }
            else if (type == 0x10) // 用于测试FPGA接收buffer溢出问题
            {
                rs422_savefile_rx_delay(id, pktCnt, value);
            }
            else if (type == 0x11)
            {
                rs422_selfinc_tx_delay(id, pktCnt, value);
            }
            else if (type == 0x12)
            {
                rs422_to_cktest_selfinc(id);
            }
            else
            {
                LOG("param err! type=0~3, 0x10 or 0x11; id=0~10");
                return OK;
            }
        }
        else if (!strncmp(opt.select, "udp_test", 8))
        {
            udp_test();
        }
        else if (!strncmp(opt.select, "boottest", 8))
        {
            boot_test();
        }
        else if (!strncmp(opt.select, "bootshow", 8))
        {
            boot_show();
        }
        else if (!strncmp(opt.select, "bootsw", 6))
        {
            boot_sw();
        }
        else if (!strncmp(opt.select, "dogctrl", 7))
        {
            id = strtoll(opt.fval, NULL, 0);
            dogfeed_ctrl(id);
        }
        else if (!strncmp(opt.select, "canlpbk", 7)) // can口环回测试
        {
            id = strtoll(opt.fval, NULL, 0);
            pktCnt = strtoll(opt.sval, NULL, 0);
            if (id >= 2)
            {
                LOG("param err! can id=0~1\n");
                return OK;
            }
            can_test(id, pktCnt);
        }
        else if (!strncmp(opt.select, "canctrl", 7)) // can协议测试
        {
            id = strtoll(opt.fval, NULL, 0);     // can id
            type = strtoll(opt.sval, NULL, 0);   // 控制类型，0:主备切换使能，1:OC开关指令
            chId32 = strtoll(opt.tval, NULL, 0); // OC通道id
            sw = strtoll(opt.endrval, NULL, 0);  // OC通道打开或关闭，0:关，1:开
            if (id >= 2)
            {
                LOG("param err! can id=0~1\n");
                return OK;
            }

            LOG("canid=%d, type=%d, chId32=%d, sw=%d\n", id, type, chId32, sw);
            can_ctrl(id, type, chId32, sw);
        }
        else if (!strncmp(opt.select, "cancktest", 9)) // can协议测试
        {
            id = strtoll(opt.fval, NULL, 0); // can id
            type = strtoll(opt.sval, NULL, 0);
            num = strtoll(opt.tval, NULL, 0); // 单机遥控指令
            sw = strtoll(opt.endrval, NULL, 0);
            if (id >= 2)
            {
                LOG("param err! can id=0~1\n");
                return OK;
            }

            LOG("canid=%d, type=0x%02x...num=%d\n", id, type, num);
            canck_test(id, type, num, sw);
        }
        else if (!strncmp(opt.select, "eepromw", 7)) // can协议测试
        {
            id = strtoll(opt.fval, NULL, 0);
            type = strtoll(opt.sval, NULL, 0);
            num = strtoll(opt.tval, NULL, 0);
            sw = strtoll(opt.endrval, NULL, 0);
            eeprom_write_func(id, type, num, sw);
        }
        else if (!strncmp(opt.select, "eepromr", 7)) // can协议测试
        {
            id = strtoll(opt.fval, NULL, 0);
            type = strtoll(opt.sval, NULL, 0);
            num = strtoll(opt.tval, NULL, 0);
            sw = strtoll(opt.endrval, NULL, 0);
            eeprom_read_func(id, type, num, sw);
        }
        else if (!strncmp(opt.select, "ppsopen", 7))
        {
            type = strtoll(opt.fval, NULL, 0);
            pps_open(type);
        }
        else if (!strncmp(opt.select, "ppsdump", 7))
        {
            pps_dump();
        }
        else if (!strncmp(opt.select, "ppsclose", 8))
        {
            pps_close();
        }
        else if (!strncmp(opt.select, "pps", 3))
        {
            type = strtoll(opt.fval, NULL, 0);
            id = strtoll(opt.sval, NULL, 0);
            u32data = strtoll(opt.tval, NULL, 0); // 表示PPS校时使能位
            pps_test(type, id, u32data);
        }
        else if (!strncmp(opt.select, "18b20set", 8))
        {
            // ds_test();
            ds_test_t();
        }
        else if (!strncmp(opt.select, "18b20get", 8))
        {
            int chan = strtoll(opt.fval, NULL, 0); // 8路中的哪一路。 取值0 -7
            ds_test_read(chan);
        }
        else if (!strncmp(opt.select, "resetr", 6))
        {
            type = strtoll(opt.fval, NULL, 0);   // 0: read, 1:write
            addr32 = strtoll(opt.sval, NULL, 0); // mram addr
            value = strtoll(opt.tval, NULL, 0);
            resetr_test();
        }
        else if (!strncmp(opt.select, "lvds0", 5))
        {
            // 打开使能
            int resetrHdl0 = -1;
            void *snCtl = devm_map(0x43cc0000, 0xff, &resetrHdl0);
            if (snCtl == NULL)
            {
                LOG("snCtl: mmap fail!\n");
                return -1;
            }
            int dsCtrlData = COMMO_ReadReg(0, 0x08, snCtl);
            dsCtrlData |= 1 << 10;
            COMMO_WriteReg(0, 0x08, snCtl, dsCtrlData);

            u32 *p = NULL;
            // 设置gpio值为1
            int plDDRhdl = -1;
            void *psDDR = devm_map(0x1f000000, 0xffff, &plDDRhdl); // 映射61440
            if (psDDR == NULL)
            {
                LOG("devm_map fail\n");
                return FALSE;
            }

            p = psDDR;
            for (u32 i = 0; i < 0x3c00; i++)
            {
                *p = i;
                p++;
            }

            int fd = -1;
            void *gpioAddr = devm_map(0x41210000, 0xff, &fd);
            if (gpioAddr == NULL)
            {
                LOG("gpio_common_map: mmap fail!\n");
                return NULL;
            }
            if (*(u32 *)gpioAddr == 1)
            {
                dma_op_init(0);
                DMA_MM2S_m2m_start(0, 61440, 0x1f000000); // ddr地址随便写个值
            }
        }
        else if (!strncmp(opt.select, "lvds1", 5))
        {
            // 打开使能
            int resetrHdl0 = -1;
            void *snCtl = devm_map(0x43cc0000, 0xff, &resetrHdl0);
            if (snCtl == NULL)
            {
                LOG("snCtl: mmap fail!\n");
                return -1;
            }
            int dsCtrlData = COMMO_ReadReg(0, 0x08, snCtl);
            dsCtrlData |= 1 << 10;
            COMMO_WriteReg(0, 0x08, snCtl, dsCtrlData);

            u32 *p = NULL;
            // 设置gpio值为1
            int plDDRhdl = -1;
            void *psDDR = devm_map(0x1f000000, 0xffff, &plDDRhdl); // 映射61440
            if (psDDR == NULL)
            {
                LOG("devm_map fail\n");
                return FALSE;
            }

            p = psDDR;
            for (u32 i = 0; i < 0x3c00; i++)
            {
                *p = i;
                p++;
            }

            // 设置gpio值为1
            int fd = -1;
            void *gpioAddr = devm_map(0x41220000, 0xff, &fd);
            if (gpioAddr == NULL)
            {
                LOG("gpio_common_map: mmap fail!\n");
                return NULL;
            }
            if (*(u32 *)gpioAddr == 1)
            {
                dma_op_init(1);
                DMA_MM2S_m2m_start(1, 61440, 0x1f000000); // ddr地址随便写个值
            }
        }
        else if (!strncmp(opt.select, "mram", 4))
        {
            type = strtoll(opt.fval, NULL, 0);   // 0: read, 1:write
            addr32 = strtoll(opt.sval, NULL, 0); // mram addr
            value = strtoll(opt.tval, NULL, 0);  // 写入时表示写入的值，按4字节一次写入；读出时表示长度
            mram_test(type, addr32, value);
        }
        else if (!strncmp(opt.select, "enable", 6))
        {
            type = strtoll(opt.fval, NULL, 0); // 代表bitN，取值范围0~7
            data = strtoll(opt.sval, NULL, 0); // 表示使能或去使能，取值0或1
            enable(type, data);
        }
        else if (!strncmp(opt.select, "gpio", 4))
        {
            type = strtoll(opt.fval, NULL, 0);
            int number = strtoll(opt.sval, NULL, 0);
            if (type == 0)
            {
                goioinout_init_test(type, number, 0);
            }
            else
            {
                value = strtoll(opt.tval, NULL, 0);
                goioinout_init_test(type, number, value);
            }
        }

        else if (!strncmp(opt.select, "ttl", 3))
        {
            data = strtoll(opt.fval, NULL, 0); // value为测试次数
            ttl_out(data);
        }
        else if (!strncmp(opt.select, "flashres", 8))
        {
            // flash resume测试
            value = strtoll(opt.fval, NULL, 0);
            flash_resume(value);
        }
        else if (!strncmp(opt.select, "flashrd", 7))
        {
            // addr32为偏移地址，value为读出的长度
            addr32 = strtoll(opt.fval, NULL, 0);
            value = strtoll(opt.sval, NULL, 0);
            flash_read(addr32, value);
        }
        else if (!strncmp(opt.select, "flashwt", 7))
        {
            // addr32为偏移地址，value为写入的u32字
            addr32 = strtoll(opt.fval, NULL, 0);
            value = strtoll(opt.sval, NULL, 0);
            flash_write(addr32, value);
        }
        else if (!strncmp(opt.select, "loadapp", 7))
        {
            // 星务app加载, 0:后台加载, 1:前台加载
            // 从mtd3分区中读取app，生成可执行文件/home/root/app-test，并加载
            type = strtoll(opt.fval, NULL, 0);
            load_app(type);
        }
        else if (!strncmp(opt.select, "qspictrl", 8))
        {
            // qspi控制器寄存器读写指令，0：读， 1: 写
            type = strtoll(opt.fval, NULL, 0);
            addr32 = strtoll(opt.sval, NULL, 0);
            value = strtoll(opt.tval, NULL, 0);
            qspi_ctrl(type, addr32, value);
        }
        else if (!strncmp(opt.select, "dogdown", 7))
        {
            type = strtoll(opt.fval, NULL, 0);
            system("killall -9 wtd_dog");
        }
        else if (!strncmp(opt.select, "dogno", 5))
        {
            type = strtoll(opt.fval, NULL, 0);
            system("killall -9 app-test");
        }
        else if (!strncmp(opt.select, "flashlock", 9))
        {
            // type为加锁或解锁，1:加锁，0:解锁
            // valueΪmtdn
            type = strtoll(opt.fval, NULL, 0);
            value = strtoll(opt.sval, NULL, 0);
            flash_lock_ctrl(type, value);
        }
        else
        {
            LOG("Unknown cmd!\n");
        }
    }

    return OK;
}

void Get_CtrlC_handler(int sig)
{
    signal(sig, SIG_IGN);
    LOG("please close fd...\n\r");
    LOG("delay 2s close progress\n\r");
    sleep(2);
    exit(0);
}

int main(int argc, char **argv)
{
    int ret;
    int i_cnt = 0;
    char path[64];
    LOG("prj app start..............................................\n\r");

#if 0
    g_updateInfo.BootFlash = (uint8_t)boot_show();
    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", GPIO_BOOT_SW);
    if (-1 == access(path, F_OK))
    {
        gpio_export(GPIO_BOOT_SW);
        gpio_direction(GPIO_BOOT_SW, GPIO_DIR_OUT);
        usleep(5000);
    }
    while (g_updateInfo.BootFlash != boot_show())
    {
        boot_sw();
        usleep(50000);
        i_cnt++;
    }
    LOG("first bootsw count : %d...\n\r", i_cnt);
    LOG("g_updateInfo.BootFlash ...%d...\n\r", g_updateInfo.BootFlash);

    signal(SIGINT, Get_CtrlC_handler);

    Trans_server_init();
#endif
    while (1)
    {
        ret = keybd_hit();

        if (ret == ERR)
            break;
    }
}
