/******************************************************************************
* COPYRIGHT Beijing UCAS Space Technology Co.,Ltd
*******************************************************************************

*******************************************************************************
* 文件名称: i2c_access.c
* 功能描述: i2c接口访问
* 使用说明:
* 文件作者:
* 编写日期: 2022/1/24 
* 修改历史:
* 修改版本  修改日期     修改人        修改内容
* -----------------------------------------------------------------------------
* 01a      2022/1/24    zhanghao     创建基础版本 
*******************************************************************************/

/******************************* 包含文件声明 ********************************/
#include <stdio.h>   
#include <linux/types.h>   
#include <fcntl.h>   
#include <unistd.h>   
#include <stdlib.h>   
#include <sys/types.h>   
#include <sys/ioctl.h>   
#include <errno.h>   
#include <assert.h>   
#include <string.h>   
#include <linux/i2c.h>   
#include <linux/i2c-dev.h>   
#include "drv_common.h"
#include "i2c_access.h"
/******************************* 局部宏定义 **********************************/
int hmc_fd;
/******************************* 局部函数原型声明 ****************************/

/******************************* 全局变量定义/初始化 *************************/ 
/******************************* 函数实现 ************************************/
/**************************实现函数********************************************
*函数原型:	i2c_open(void)
*功　　能:	i2c接口打开
*返回值:	成功 为 0 
*         失败 为-1
*******************************************************************************/ 
int i2c_open(unsigned char id)
{
	if(id == 0)
	{
    	hmc_fd = open(I2C_0_CHIP, O_RDWR);
    }
	else if(id == 1)
	{
    	hmc_fd = open(I2C_1_CHIP, O_RDWR);
    }
	else if(id == 2)
	{
    	hmc_fd = open(I2C_2_CHIP, O_RDWR);
    }
	else if(id ==3)
	{
    	hmc_fd = open(I2C_3_CHIP, O_RDWR);
    }
	else if(id ==4)
	{
    	hmc_fd = open(I2C_4_CHIP, O_RDWR);
    }
	else if(id ==5)
	{
    	hmc_fd = open(I2C_5_CHIP, O_RDWR);
    }
	else if(id ==6)
	{
    	hmc_fd = open(I2C_6_CHIP, O_RDWR);
    }	

	if(hmc_fd < 0)
    {
        printf("open I2C failed !\n");
        return -1;		
    }

	return 0;

}


void i2c_fd_close(int fd)
{
    if(fd > 0)
    {
		close(fd);
		fd = -1;
		printf("close I2C OK!\n");
    }
}


void i2c_close(void)
{
    if(hmc_fd > 0)
    {
		close(hmc_fd);
		hmc_fd = -1;
		printf("close I2C OK!\n");
    }
}

int i2cIPMB_Write(unsigned short slave_addr, unsigned char len, unsigned char *dat)
{
    struct i2c_rdwr_ioctl_data packets;  
    struct i2c_msg messages[1];  

//    switch_channel_enable(3);

    ioctl(hmc_fd,I2C_TIMEOUT,1);/*超时时间*/
    ioctl(hmc_fd,I2C_RETRIES,2);/*重复次数*/
  
    messages[0].addr  = slave_addr;  
    messages[0].flags = I2C_M_WT;  
    messages[0].len   = len;    
    messages[0].buf   = dat;
  
    /* Transfer the i2c packets to the kernel and verify it worked */  
    packets.msgs  = messages;  
    packets.nmsgs = 1;  
    if(ioctl(hmc_fd, I2C_RDWR, &packets) < 0)
    {  
        perror("Unable to send data");  
        return 1;  
    }  
  
    return 0;  
}

int i2cIPMB_Read(unsigned short slave_addr, unsigned char len, unsigned char *outbuf)
{
    struct i2c_rdwr_ioctl_data packets;  
    struct i2c_msg messages[1]; 

//    switch_channel_enable(3);

    ioctl(hmc_fd,I2C_TIMEOUT,1);/*超时时间*/
    ioctl(hmc_fd,I2C_RETRIES,2);/*重复次数*/

    messages[0].addr  = slave_addr;  
    messages[0].flags = I2C_M_RD;  
    messages[0].len   = len; //sizeof(outbuf);
    messages[0].buf   = outbuf;  

    /* Send the request to the kernel and get the result back */  
    packets.msgs      = messages;  
    packets.nmsgs     = 1;  
	if(ioctl(hmc_fd, I2C_RDWR, &packets) < 0)
	{
        perror("Unable to send data");  
        return -1;  
    }  
  
    return 0;    	
}

int i2c_eeprom_Write(unsigned short slave_addr, unsigned char addr, unsigned char value)
{
    unsigned char outbuf[3]={0};
    struct i2c_rdwr_ioctl_data packets;  
    struct i2c_msg messages[1];  

//    switch_channel_enable(3);

    ioctl(hmc_fd,I2C_TIMEOUT,1);/*超时时间*/
    ioctl(hmc_fd,I2C_RETRIES,2);/*重复次数*/
  
    messages[0].addr  = slave_addr;  
    messages[0].flags = I2C_M_WT;  
    messages[0].len   = 3;    
    messages[0].buf   = outbuf;
      
  
    /* The first byte indicates which register we'll write */  
    outbuf[0] = 0;  
	outbuf[1] = addr;  

    /* The second byte indicates the value we'll write */  
    outbuf[2] = value;
  
    /* Transfer the i2c packets to the kernel and verify it worked */  
    packets.msgs  = messages;  
    packets.nmsgs = 1;  
    if(ioctl(hmc_fd, I2C_RDWR, &packets) < 0)
    {  
        perror("Unable to send data");  
        return 1;  
    }  
  
    return 0;  
}
int i2c_eeprom_Read(unsigned short slave_addr, unsigned char reg, int buf_len, unsigned char *outbuf)
{
	unsigned char inbuf[2]={0};
    struct i2c_rdwr_ioctl_data packets;  
    struct i2c_msg messages[2]; 

//    switch_channel_enable(3);

    ioctl(hmc_fd,I2C_TIMEOUT,1);/*超时时间*/
    ioctl(hmc_fd,I2C_RETRIES,2);/*重复次数*/

	inbuf[0] = 0x0;
	inbuf[1] = reg;

    messages[0].addr  = slave_addr;  
    messages[0].flags = I2C_M_WT;  
    messages[0].len   = 2; //sizeof(outbuf);
    messages[0].buf   = inbuf;  

    /* The data will get returned in this structure */  
    messages[1].addr  = slave_addr;  
    messages[1].flags = I2C_M_RD/* | I2C_M_NOSTART*/;  
    messages[1].len   = buf_len;  
    messages[1].buf   = outbuf;  

    /* Send the request to the kernel and get the result back */  
    packets.msgs      = messages;  
    packets.nmsgs     = 2;  
	if(ioctl(hmc_fd, I2C_RDWR, &packets) < 0)
	{
        perror("Unable to send data");  
        return -1;  
    }  
  
    return 0;    	
}



int i2c_Write(unsigned short slave_addr, unsigned char reg, unsigned char value)
{
    unsigned char outbuf[2]={0};
    struct i2c_rdwr_ioctl_data packets;  
    struct i2c_msg messages[1];  

//    switch_channel_enable(3);

    ioctl(hmc_fd,I2C_TIMEOUT,1);/*超时时间*/
    ioctl(hmc_fd,I2C_RETRIES,2);/*重复次数*/
  
    messages[0].addr  = slave_addr;  
    messages[0].flags = I2C_M_WT;  
    messages[0].len   = 2;    
    messages[0].buf   = outbuf;
      
  
    /* The first byte indicates which register we'll write */  
    outbuf[0] = reg;  

    /* The second byte indicates the value we'll write */  
    outbuf[1] = value;
  
    /* Transfer the i2c packets to the kernel and verify it worked */  
    packets.msgs  = messages;  
    packets.nmsgs = 1;  
    if(ioctl(hmc_fd, I2C_RDWR, &packets) < 0)
    {  
        perror("Unable to send data");  
        return 1;  
    }  
  
    return 0;  
}

int i2c_Read(unsigned short slave_addr, unsigned char reg, int buf_len, unsigned char *outbuf)
{
    struct i2c_rdwr_ioctl_data packets;  
    struct i2c_msg messages[2]; 

//    switch_channel_enable(3);

    ioctl(hmc_fd,I2C_TIMEOUT,1);/*超时时间*/
    ioctl(hmc_fd,I2C_RETRIES,2);/*重复次数*/

    messages[0].addr  = slave_addr;  
    messages[0].flags = I2C_M_WT;  
    messages[0].len   = 1; //sizeof(outbuf);
    messages[0].buf   = &reg;  

    /* The data will get returned in this structure */  
    messages[1].addr  = slave_addr;  
    messages[1].flags = I2C_M_RD/* | I2C_M_NOSTART*/;  
    messages[1].len   = buf_len;  
    messages[1].buf   = outbuf;  

    /* Send the request to the kernel and get the result back */  
    packets.msgs      = messages;  
    packets.nmsgs     = 2;  
	if(ioctl(hmc_fd, I2C_RDWR, &packets) < 0)
	{
        perror("Unable to send data");  
        return -1;  
    }  
  
    return 0;    	
}

// 向PCA9548控制寄存器写入数值
int i2c_wt_pca9548(unsigned short slave_addr, unsigned char value)
{
    struct i2c_rdwr_ioctl_data packets;  
    struct i2c_msg messages[1];  

    ioctl(hmc_fd,I2C_TIMEOUT,5);/*超时时间*/
    ioctl(hmc_fd,I2C_RETRIES,2);/*重复次数*/
  
    messages[0].addr  = slave_addr;  
    messages[0].flags = I2C_M_WT;  
    messages[0].len   = 1;    
    messages[0].buf   = &value;
      
    /* Transfer the i2c packets to the kernel and verify it worked */  
    packets.msgs  = messages;  
    packets.nmsgs = 1;  
    if(ioctl(hmc_fd, I2C_RDWR, &packets) < 0)
    {  
        perror("Unable to send data");  
        return 1;  
    }  
  
    return 0;  
}

// 从PCA9548控制寄存器读出数值
int i2c_rd_pca9548(unsigned short slave_addr, unsigned char *outbuf)
{
    struct i2c_rdwr_ioctl_data packets;  
    struct i2c_msg messages[1]; 

    ioctl(hmc_fd,I2C_TIMEOUT,1);/*超时时间*/
    ioctl(hmc_fd,I2C_RETRIES,2);/*重复次数*/

    messages[0].addr  = slave_addr;  
    messages[0].flags = I2C_M_RD;  
    messages[0].len   = 1; //sizeof(outbuf);
    messages[0].buf   = outbuf;  

    /* Send the request to the kernel and get the result back */  
    packets.msgs      = messages;  
    packets.nmsgs     = 1;  
	if(ioctl(hmc_fd, I2C_RDWR, &packets) < 0)
	{
        perror("Unable to send data");  
        return -1;  
    }  
  
    return 0;    	
}



/**************************实现函数********************************************
*函数原型:		u8 IICwriteBit(u8 dev, u8 reg, u8 bitNum, u8 data)
*功　　能:	    读 修改 写 指定设备 指定寄存器一个字节 中的1个位
输入	dev  目标设备地址
reg	   寄存器地址
bitNum  要修改目标字节的bitNum位
data  为0 时，目标位将被清0 否则将被置位
返回   成功 为1 
失败为0
*******************************************************************************/ 
void IICwriteBit(unsigned char dev, unsigned char reg, unsigned char bitNum, unsigned char data)
{
	unsigned char b;
	i2c_Read(dev, reg, 1, &b);
	b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
	i2c_Write(dev, reg, b);
}

/**************************实现函数********************************************
*函数原型:		u8 IICwriteBits(u8 dev,u8 reg,u8 bitStart,u8 length,u8 data)
*功　　能:	    读 修改 写 指定设备 指定寄存器一个字节 中的多个位
输入	dev  目标设备地址
reg	   寄存器地址
bitStart  目标字节的起始位
length   位长度
data    存放改变目标字节位的值
返回   成功 为1 
失败为0
*******************************************************************************/ 
void IICwriteBits(unsigned char dev,unsigned char reg,unsigned char bitStart,unsigned char length,unsigned char data)
{
	
	unsigned char b;
	unsigned char mask;
	i2c_Read(dev, reg, 1, &b);
	mask = (0xFF << (bitStart + 1)) | 0xFF >> ((8 - bitStart) + length - 1);
	data <<= (8 - length);
	data >>= (7 - bitStart);
	b &= mask;
	b |= data;
	i2c_Write(dev, reg, b);
}


/********************************************** 源文件结束 **********************************/
