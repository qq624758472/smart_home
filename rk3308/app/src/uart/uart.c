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
#include "uart.h"

// uart测试函数
// 设置串口函数

int set_opt(int fd, int nSpeed, int nBits, char nEvent, int nStop)
{
    struct termios newtio, oldtio;
    /* 获取fd串口对应的termios结构体，这步主要是查询串口是否启动正常 */
    if (tcgetattr(fd, &oldtio) != 0)
    {
        LOG("SetupSerial 1");
        return -1;
    }
    // 清空
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag |= CLOCAL | CREAD; // 配置成本地模式(本地连接、不改变端口所有者)、可读
    newtio.c_cflag &= ~CSIZE;         // 清空数据位设置
    /* 选择数据位 */
    switch (nBits)
    {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }
    /* 选择校验位 */
    switch (nEvent)
    {
    case 'O':
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        // newtio.c_iflag |= (INPCK | ISTRIP);	//启用输入奇偶检测、去掉第八位
        newtio.c_iflag |= INPCK;
        break;
    case 'E':
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        // newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_iflag |= INPCK;
        break;
    case 'N':
        newtio.c_cflag &= ~PARENB;
        break;
    }
    /* 选择波特率 */
    switch (nSpeed)
    {
    case 2400:
        cfsetispeed(&newtio, B2400);
        cfsetospeed(&newtio, B2400);
        break;
    case 4800:
        cfsetispeed(&newtio, B4800);
        cfsetospeed(&newtio, B4800);
        break;
    case 9600:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    case 115200:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    case 230400:
        cfsetispeed(&newtio, B230400);
        cfsetospeed(&newtio, B230400);
        break;
    case 460800:
        cfsetispeed(&newtio, B460800);
        cfsetospeed(&newtio, B460800);
        break;
    case 921600:
        cfsetispeed(&newtio, B921600);
        cfsetospeed(&newtio, B921600);
        break;
    default:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    }
    /* 选择停止位，貌似linux下不能设置(1.5 0.5)停止位 */
    if (nStop == 1)
        newtio.c_cflag &= ~CSTOPB;
    else if (nStop == 2)
        newtio.c_cflag |= CSTOPB;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd, TCIFLUSH);
    /* 设置新配置 */
    if ((tcsetattr(fd, TCSANOW, &newtio)) != 0)
    {
        perror("com set error");
        return -1;
    }
    //	LOG("set done!\n\r");
    return 0;
}
// uart测试函数

void rs422_send(unsigned char id)
{
    int wr_static;
    int fd;
    char buffer[10] = {0xCD, 0xAB, 0xCD, 0xAB, 0xB2, 00, 00, 00, 00, 0xFF};
    char *uart1 = "/dev/ttyS";
    char uartx[32];

    memset(uartx, 0, sizeof(uartx));
    snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);

    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s is success.\n", uartx);
        // set_opt(fd, 115200, 8, 'O', 1); // 设置串口
        set_opt(fd, 9600, 8, 'N', 1); // 设置串口
        wr_static = write(fd, buffer, 10);
        if (wr_static > 0)
        {
            LOG("uart write len is %d.\n", wr_static);
        }
        else
        {
            LOG("uart write failed.\n");
        }

        usleep(100000);
        close(fd);
    }
}

void open_join_network()
{
    int id = 1;
    int wr_static;
    int fd;
    char buffer[64] = {0};
    char *uart1 = "/dev/ttyS";
    char uartx[32];
    memset(buffer, 0, 64);

    rk_cmd *p = buffer;
    p->cmdHead = UART_HEAD;
    p->cmd = UART_CMD_RK_B0;

    memset(uartx, 0, sizeof(uartx));
    snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);

    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s is success.\n", uartx);
        set_opt(fd, 9600, 8, 'N', 1); // 设置串口

        wr_static = write(fd, buffer, 10);
        if (wr_static > 0)
        {
            LOG("uart write len is %d.\n", wr_static);
            while (1)
            {
                memset(buffer, 0, 64);
                wr_static = read(fd, buffer, 256);
                if (wr_static > 0)
                {
                    // 打印观测
                    LOG("uart%d rx len=%d:", id, wr_static);
                    print_data(buffer, wr_static);
                }
                else
                {
                    usleep(10000);
                }
            }
        }
        else
        {
            LOG("uart write failed.\n");
        }

        usleep(100000);
        close(fd);
    }
}

void send_zigbee_to_light()
{
    int id = 1;
    int wr_static;
    int fd;
    char buffer[64] = {0};
    char *uart1 = "/dev/ttyS";
    char uartx[32];
    memset(buffer, 0, 64);

    rk_cmd *p = buffer;
    p->cmdHead = UART_HEAD;
    p->cmd = 0x1; // 发送zigbee数据

    memset(uartx, 0, sizeof(uartx));
    snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);

    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s is success.\n", uartx);
        set_opt(fd, 115200, 8, 'O', 1); // 设置串口

        wr_static = write(fd, buffer, 10);
        if (wr_static > 0)
        {
            LOG("uart write len is %d.\n", wr_static);
            while (1)
            {
                memset(buffer, 0, 64);
                wr_static = read(fd, buffer, 256);
                if (wr_static > 0)
                {
                    // 打印观测
                    LOG("uart%d rx len=%d:", id, wr_static);
                    print_data(buffer, wr_static);
                }
                else
                {
                    usleep(10000);
                }
            }
        }
        else
        {
            LOG("uart write failed.\n");
        }

        usleep(100000);
        close(fd);
    }
}
// rs422测试函数，环回收到的二进制数
void rs422_loopback(unsigned char id, unsigned int value)
{
    int wr_static, w_static;
    int fd;
    char readbuf[256];
    char *uart1 = "/dev/ttyS";
    char *uart2 = "/dev/ttyPS";
    char uartx[32];

    memset(readbuf, 0, sizeof(readbuf));
    memset(uartx, 0, sizeof(uartx));
    if (id > 15)
    {
        snprintf(uartx, sizeof(uartx), "%s%d", uart2, id);
    }
    else
    {
        snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);
    }

    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s success. fd :%d ...\n", uartx, fd);
        set_opt(fd, value, 8, 'O', 1);

        usleep(10000);

        while (1)
        {
            wr_static = read(fd, readbuf, 256);
            if (wr_static > 0)
            {
                // 打印观测
                LOG("uart%d rx len=%d:", id, wr_static);
                print_data(readbuf, wr_static);

                w_static = write(fd, readbuf, wr_static); // 环回收到的数据流
                // LOG("write len=%d.\n", strlen(buffer));
                if (w_static > 0)
                {
                    LOG("uart%d send len is %d.\n", id, w_static);
                }
                else
                {
                    LOG("uart%d send failed, ret=%d.\n", id, w_static);
                }
            }
            else
            {
                usleep(10000);
            }
        }
        close(fd); // 释放串口设备资源
    }
}

// 发送自增数
void rs422_selfinc(unsigned char id, int pktCnt)
{
    int wr_static;
    int fd;
    int wtTotLen = 0;
    char buffer[256];
    char *uart1 = "/dev/ttyS";
    char uartx[32];

    memset(uartx, 0, sizeof(uartx));
    snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);

    for (int i = 0; i < 256; i++)
    {
        buffer[i] = i;
    }

    LOG("tx pkt count=%d\n", pktCnt);

    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s is success.\n", uartx);
        set_opt(fd, 115200, 8, 'O', 1); // 设置串口

        for (int i = 0; i < pktCnt;)
        {
            while (1)
            {
                wr_static = write(fd, &buffer[wtTotLen], sizeof(buffer) - wtTotLen);
                if (wr_static > 0)
                {
                    wtTotLen = wtTotLen + wr_static;
                    if (wtTotLen == sizeof(buffer))
                    {
                        i += 1;
                        wtTotLen = 0;
                        break;
                    }
                }
                else if (wr_static == 0)
                {
                    LOG("pkt=%d, wtTotLen=%d, write ret=0\n", i, wtTotLen);
                }
                else
                {
                    usleep(10000);
                    // LOG("uart tx pktcnt=%d failed. errno=%d\n", i, errno);
                }
            }

            if (i % 1000 == 0)
            {
                LOG("uart tx pktcnt=%d...\n", i);
            }

            usleep(10000);
        }

        usleep(10000);
        close(fd);

        LOG("uart tx finished.\n");
    }
}

// 发送自增数
void rs422_selfinc_tx_delay(unsigned char id, int pktCnt, unsigned int delay)
{
    int wr_static;
    int fd;
    int wtTotLen = 0;
    char buffer[256];
    char *uart1 = "/dev/ttyS";
    char uartx[32];

    memset(uartx, 0, sizeof(uartx));
    snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);

    for (int i = 0; i < 256; i++)
    {
        buffer[i] = i;
    }

    LOG("tx pkt count=%d\n", pktCnt);

    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s is success.\n", uartx);
        set_opt(fd, 115200, 8, 'O', 1);

        LOG("Tx delay time=%d(us).\n", delay);

        for (int i = 0; i < pktCnt;)
        {
            while (1)
            {
                wr_static = write(fd, &buffer[wtTotLen], sizeof(buffer) - wtTotLen);
                if (wr_static > 0)
                {
                    wtTotLen = wtTotLen + wr_static;
                    if (wtTotLen == sizeof(buffer))
                    {
                        i += 1;
                        wtTotLen = 0;
                        break;
                    }
                }
                else
                {
                    LOG("pkt=%d, wtTotLen=%d, wr_static=%d, errno=%s, delay 1ms.\n",
                        i, wtTotLen, wr_static, strerror(errno));
                    usleep(1000);
                }
            }

            if (i % 1000 == 0)
            {
                LOG("uart tx pktcnt=%d...\n", i);
            }

            // 每包延时时间
            if (delay != 0)
                usleep(delay);
        }

        usleep(10000);
        close(fd); // �ͷŴ����豸��Դ

        LOG("uart tx finished.\n");
    }
}

void rs422_to_cktest_selfinc(unsigned char id)
{
    int wr_static, w_static;
    int fd;
    char readbuf[256];
    char writebuf[] = {0xeb, 0x90, 0x00, 0x04, 0x00, 0x0a, 0x00, 0x01, 0x0a, 0x1a, 0x28, 0x08, 0x00, 0x5f, 0x09, 0xd7};
    char *uart1 = "/dev/ttyS";
    char uartx[32];

    memset(readbuf, 0, sizeof(readbuf));
    memset(uartx, 0, sizeof(uartx));
    snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);

    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s success.\n", uartx);
        set_opt(fd, 115200, 8, 'O', 1);

        usleep(10000);

        while (1)
        {
            w_static = write(fd, writebuf, sizeof(writebuf));
            // LOG("write len=%d.\n", strlen(buffer));
            if (w_static > 0)
            {
                LOG("uart id=%d send len is %d.\n", id, w_static);
            }
            else
            {
                LOG("uart id=%d send failed, ret=%d.\n", id, w_static);
            }
            wr_static = 0;
            usleep(50000);
            wr_static = read(fd, readbuf, 256);
            if (wr_static > 0)
            {
                LOG("uart id=%d recv len=%d, msg=", id, wr_static);
                for (int i = 0; i < wr_static; i++)
                {
                    LOG("%02x ", readbuf[i]);
                    usleep(10);
                }
                LOG("\n");
            }
            else
            {
                //	sleep(1);
            }
            sleep(1);
        }
        close(fd);
    }
}

// rs422测试函数，接收存文件
void rs422_savefile(unsigned char id, int maxPkt)
{
    int wr_static;
    int fd, savfiled;
    int pktcnt = 0;
    int saveByte, totalByte = 0;
    char readbuf[256];
    char *uart1 = "/dev/ttyS";
    char savfName[32];
    char uartx[32];

    memset(readbuf, 0, sizeof(readbuf));

    // 打开文件
    memset(savfName, 0, sizeof(savfName));
    snprintf(savfName, sizeof(savfName), "%s%d_recv.bin", "./", id);
    savfiled = open(savfName, O_RDWR | O_CREAT, 0644); // |O_DIRECT);
    if (savfiled < 0)
    {
        LOG("open file=%s is failed.\n", savfName);
        // return;
    }

    // 打开串口设备
    memset(uartx, 0, sizeof(uartx));
    snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);
    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s success.\n", uartx);
        // set_opt(fd, 115200, 8, 'O', 1);
        set_opt(fd, 115200, 8, 'N', 1);

        usleep(10000);

        while (1)
        {
            wr_static = read(fd, readbuf, sizeof(readbuf));
            if (wr_static > 0)
            {
                pktcnt += 1;
                LOG("uart%d rx:\n", id);
                print_data(readbuf, wr_static);
            }
            else
            {
                usleep(1000);
            }
        }

        usleep(10000);
        close(fd);
        close(savfiled);
        LOG("Recv pktcnt=%d, file=%s save finished\n.", pktcnt, savfName);
    }
}

// rs422测试函数，接收存文件，用于测试FPGA接收buffer溢出问题
void rs422_savefile_rx_delay(unsigned char id, int maxPkt, unsigned int delay)
{
    int wr_static;
    int fd, savfiled;
    int pktcnt = 0;
    int saveByte, totalByte = 0;
    char readbuf[256];
    char *uart1 = "/dev/ttyS";
    char savfName[64] = {0};
    char uartx[32];

    memset(readbuf, 0, sizeof(readbuf));

    // 打开文件
    memset(savfName, 0, sizeof(savfName));
    snprintf(savfName, sizeof(savfName), "%s%d_recv_rx_delay.bin", "/home/root/uart", id);
    savfiled = open(savfName, O_RDWR | O_CREAT, 0644); // |O_DIRECT);
    if (savfiled < 0)
    {
        LOG("open file=%s is failed.\n", savfName);
        return;
    }

    // 打开串口设备
    memset(uartx, 0, sizeof(uartx));
    snprintf(uartx, sizeof(uartx), "%s%d", uart1, id);
    if ((fd = open(uartx, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        LOG("open %s is failed.\n", uartx);
    }
    else
    {
        LOG("open %s success.\n", uartx);
        set_opt(fd, 115200, 8, 'O', 1);

        usleep(10000);

        if (delay == 0)
        {
            delay = 10000; // 默认10ms
        }
        LOG("Rx delay time=%d(us).\n", delay);

        while (1)
        {
            wr_static = read(fd, readbuf, sizeof(readbuf));
            if (wr_static > 0)
            {
                pktcnt += 1;

                LOG("uart%d rx:\n", id);
                print_data(readbuf, wr_static);

                saveByte = write(savfiled, readbuf, wr_static);
                if (saveByte <= 0)
                {
                    LOG("write file=%s error.\n", savfName);
                }
                else
                {
                    totalByte = totalByte + saveByte;
                }

                if (totalByte >= maxPkt * 10)
                {
                    break;
                }

                if (pktcnt % 1000 == 0)
                {
                    LOG("file=%s save pktcnt=%d...\n", savfName, pktcnt);
                }

                usleep(delay);
            }
            else
            {
                usleep(1000);
            }
        }

        // fflush(savfiled);
        // system("sync");
        /*
        ret = syncfs(savfiled);
        if(ret<0)
        {
            LOG("call syncfs error!\n");
        }
        */

        usleep(10000);
        close(fd);
        close(savfiled);
        LOG("Recv pktcnt=%d, file=%s save finished\n.", pktcnt, savfName);
    }
}