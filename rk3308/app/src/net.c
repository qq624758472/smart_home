#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include "i2c_access.h"
#include "net.h"
#include "./ipmb/ipmb.h"
#include "drv_common.h"

int savfiled;
char savfName[32];
struct timeval tv_1, tv_now;
IpmbGCdef IpmbGCtest;

typedef struct sateTransHead
{
    unsigned short frameHead; // 帧头
    unsigned char msgType;    // 消息类型
    unsigned char fileType;   // 文件类型
    unsigned short msgLenth;  // 消息长度
} sateTransHead_t;            // 消息头格式

typedef struct sateTransRequestMsg
{
    sateTransHead_t head; // 消息的头
    char data[1024];
} sateTransRequestMsg_t; // 请求的消息格式
typedef struct sateTransAckMsg
{
    sateTransHead_t head; // 消息的头
    unsigned short data;
} sateTransAckMsg_t; // 响应的消息格式

typedef struct udpClientInfo
{
    struct sockaddr_in src_addr[UDP_INFO_MAX_CNT];
    int UDPsend_socket;
    int UDPrecv_socket;
} udpClientInfo_t;

typedef struct epollInfo
{
    bool initialized;     /*是否已经初始化*/
    pthread_t hThread;    /*线程描述符*/
    unsigned int thrdRun; /*线程运行控制*/
    unsigned int sockInitd;
    int efd;                 /*epoll文件描述符*/
    udpClientInfo_t udpInfo; /*UDP信息*/
} epollInfo_t;

/*统计结构体*/
typedef struct sateStatis
{
    unsigned int ZH1_send_Cnt;
    unsigned int ZH1_recv_Cnt;
    unsigned int ZH1_recvERR_Cnt;
    unsigned int ZH2_send_Cnt;
    unsigned int ZH2_recv_Cnt;
    unsigned int ZH2_recvERR_Cnt;
} sateStatis_t;

epollInfo_t g_epollInfo;
sateStatis_t g_sateStatis;

/******************************* 函数实现 ************************************/
/*******************************************************************************
* 函数名称: epoll_info_init
* 功    能: epoll结构体初始化
* 参    数:  无
* 函数返回:  成功  0
             失败  -1
* 说    明: 无
*******************************************************************************/
int epoll_info_init(void)
{
    memset(&g_epollInfo, 0, sizeof(epollInfo_t));

    g_epollInfo.efd = epoll_create(FD_SETSIZE);
    if (g_epollInfo.efd < 0)
    {
        printf("epoll_create error\n");
        return -1;
    }

    g_epollInfo.udpInfo.UDPsend_socket = -1;
    g_epollInfo.udpInfo.UDPrecv_socket = -1;

    return 0;
}

/*******************************************************************************
 * 函数名称: epoll_info_exit
 * 功    能: epoll结构体注销
 * 参    数:  无
 * 函数返回:  无
 * 说    明:  无
 *******************************************************************************/
void epoll_info_exit(void)
{
    if (g_epollInfo.efd > 0)
    {
        close(g_epollInfo.efd);
    }
    memset(&g_epollInfo, 0, sizeof(epollInfo_t));

    g_epollInfo.initialized = false;
    g_epollInfo.efd = -1;
}

/*******************************************************************************
* 函数名称: socket_server_init
* 功    能: 建立socket，绑定端口和监听
* 参    数: 参数名     数据类型     出/入参      含义
                        mode				int				 	 入参			用于区分是TCP还是UDP
            ip          char         入参     服务器的IP地址
            port				int					 入参			端口号

* 函数返回:  成功  socket描述符
             失败  -1
* 说    明: 无
*******************************************************************************/
int socket_server_init(int mode, char *ip, int port)
{
    int listen_fd = -1;
    int flag = 1;
    struct sockaddr_in server_addr;

    if (mode == TCP)
    {
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    }
    else if (mode == UDP)
    {
        listen_fd = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (listen_fd == -1)
    {
        printf("socket %s ip:%s port:%d error\n", ((mode == TCP) ? "TCP" : "UDP"), ip, port);
        return -1;
    }

    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1)
    {
        printf("setsockopt %s ip:%s port:%d error\n", ((mode == TCP) ? "TCP" : "UDP"), ip, port);
        close(listen_fd);
        return -1;
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    // server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        printf("bind %s ip:%s port:%d error\n", ((mode == TCP) ? "TCP" : "UDP"), ip, port);
        close(listen_fd);
        return -1;
    }

    if (mode == TCP)
    {
        if (listen(listen_fd, LISTEN_MAX_NUM) < 0)
        {
            printf("listen %s ip:%s port:%d error\n", ((mode == TCP) ? "TCP" : "UDP"), ip, port);
            close(listen_fd);
            return -1;
        }
    }

    return listen_fd;
}

/*******************************************************************************
 * 函数名称: Trans_socket_close
 * 功    能: socket描述符关闭
 * 参    数:  无
 * 函数返回:  无
 * 说    明:  无
 *******************************************************************************/
void Trans_socket_close(void)
{
    if (g_epollInfo.udpInfo.UDPsend_socket > 0)
    {
        close(g_epollInfo.udpInfo.UDPsend_socket);
        g_epollInfo.udpInfo.UDPsend_socket = -1;
    }

    if (g_epollInfo.udpInfo.UDPrecv_socket > 0)
    {
        close(g_epollInfo.udpInfo.UDPrecv_socket);
        g_epollInfo.udpInfo.UDPrecv_socket = -1;
    }
}

/*******************************************************************************
* 函数名称: Trans_epoll_change
* 功    能: epoll的监控状态改变
* 参    数: 参数名   数据类型    出/入参      	含义
            op	       int			   入参		     EPOLL_CTL_ADD\EPOLL_CTL_DEL\EPOLL_CTL_MOD
            events	 uint32_t      入参        EPOLLIN\EPOLLOUT
            socket	   int			   入参		     socket描述符
* 函数返回: 无
* 说    明: 无
*******************************************************************************/
int Trans_epoll_change(int op, uint32_t events, int socket)
{
    int ret = 0;
    struct epoll_event event;

    event.events = events;
    event.data.fd = socket;
    ret = epoll_ctl(g_epollInfo.efd, op, socket, &event);
    return ret;
}

/*******************************************************************************
 * 函数名称: Trans_rcv_cnt
 * 功    能:  接收统计
 * 参    数:  无
 * 函数返回:  无
 * 说    明:  无
 *******************************************************************************/
void Trans_rcv_cnt(int ret, unsigned char cltType)
{
    if (ret > 0)
    {
        if (cltType == ZH1_MSG)
        {
            g_sateStatis.ZH1_recv_Cnt++;
        }
        else if (cltType == ZH2_MSG)
        {
            g_sateStatis.ZH2_recv_Cnt++;
        }
    }
    else
    {
        if (cltType == ZH1_MSG)
        {
            g_sateStatis.ZH1_recvERR_Cnt++;
        }
        else if (cltType == ZH2_MSG)
        {
            g_sateStatis.ZH2_recvERR_Cnt++;
        }
    }
}

/*******************************************************************************
* 函数名称: Trans_udp_addr_is_equal
* 功    能: 判断sockaddr_in里的内容是否相等
* 参    数: 参数名   数据类型    						出/入参     含义
            p		   struct sockaddr_in			  入参		socket的地址
            q		   struct sockaddr_in			  入参		socket的地址
* 函数返回:  成功  1
             失败  0
* 说    明: 无
*******************************************************************************/
int Trans_udp_addr_is_equal(struct sockaddr_in *p, struct sockaddr_in *q)
{
    return p->sin_family == q->sin_family &&
           p->sin_port == q->sin_port &&
           p->sin_addr.s_addr == q->sin_addr.s_addr;
}

/*******************************************************************************
* 函数名称: Trans_udp_info_searce_by_addr
* 功    能: 通过socket地址查找udpClientInfo_t结构体的信息
* 参    数: 参数名   数据类型    						出/入参     含义
            addr		 struct sockaddr_in			  入参		socket的地址
* 函数返回:  成功  index
             失败  -1
* 说    明: 无
*******************************************************************************/
int Trans_udp_info_searce_by_addr(struct sockaddr_in *addr)
{
    int i = 0;

    for (i = 0; i < 2; i++)
    {
        if (Trans_udp_addr_is_equal(&g_epollInfo.udpInfo.src_addr[i], addr))
        {
            return i;
        }
    }

    return -1;
}

/*******************************************************************************
* 函数名称: Trans_snd_udp_usrid_check_result
* 功    能: 用户身份校验
* 参    数: 参数名   数据类型    出/入参      含义
            buf		 		char			  入参			待校验信息
            len		 		int			    入参			待校验信息的长度
* 函数返回: 无
* 说    明: 无
*******************************************************************************/
void Trans_snd_udp_usrid_check_result(unsigned short len, char msgType, int udpsockt, struct sockaddr *addr)
{
    int i = 0;
    int ret = -1;
    static unsigned short FrameSqueCnt = 0;
    unsigned char ackInfo[1024] = {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);
    memset(ackInfo, 0x0, sizeof(ackInfo));
    ackInfo[0] = 0xfc;
    ackInfo[1] = 0x1d;
    ackInfo[2] = msgType;
    ackInfo[3] = 0xc1;
    FrameSqueCnt++;
    ackInfo[4] = (unsigned char)((FrameSqueCnt >> 8) & 0x00ff);
    ackInfo[5] = (unsigned char)((FrameSqueCnt)&0x00ff);
    ackInfo[10] = (unsigned char)((len >> 8) & 0x00ff);
    ackInfo[11] = (unsigned char)((len)&0x00ff);
    memcpy(&ackInfo[12], IpmbGCtest.DigitalVar, 7);
    memcpy(&ackInfo[12 + 7], IpmbGCtest.GC_Var, len - 7);
    for (int cnt = 12; cnt < (len + 12); cnt++)
    {
        ackInfo[12 + len] += ackInfo[cnt];
    }

    unsigned char *p_ackInfo = NULL;
    int sendBufLen = 0;
    if (ipmb_func(6,&p_ackInfo, &sendBufLen) < 0)
    {
        LOG("ipmb_func error");
        return;
    }
    printf("***printf   Trans_snd_udp_usrid_check_result*\n\r");
    ret = sendto(udpsockt, (void *)p_ackInfo, sendBufLen, 0, (struct sockaddr *)addr, addrlen);
    printf("*********sendto ret:%d*************\n\r", ret);
    if (p_ackInfo)
        free(p_ackInfo);
}

/************************************************************************************************************************************************/
/*******************************************************************************
* 函数名称: Trans_ck_rcv_handle
* 功    能: 接收数据处理函数(UDP)
* 参    数: 参数名   数据类型    				出/入参      含义
            socket	udpClientInfo_t			  出参		udpClientInfo_t结构体
* 函数返回:  成功  0
             失败  -1
* 说    明: 无
*******************************************************************************/
int Trans_ck_rcv_handle(udpClientInfo_t *info)
{
    int ret = 0;
    int n = 0;
    static unsigned int cnt = 0;
    int index = 0;
    int udpindex = 0;
    long tv_nowval;
    static unsigned int totalByte = 0;
    unsigned int saveByte = 0;
    // static int udp_add_flag = 0;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    sateTransRequestMsg_t msg;
    // struct epoll_event event;
    unsigned char g_IpmbCmdVar[9];
    unsigned char g_sum = 0;

    n = recvfrom(info->UDPrecv_socket, (void *)g_IpmbCmdVar, sizeof(g_IpmbCmdVar), 0, (struct sockaddr *)&addr, &addrlen);
    if (n > 0)
    {
        print_data((char *)g_IpmbCmdVar, sizeof(g_IpmbCmdVar));
        if ((g_IpmbCmdVar[0] != 0x1A) || (g_IpmbCmdVar[1] != 0xCF))
        {
            printf("ck receive frame head 0x%x error\n", msg.head.frameHead);
            return -1;
        }
#if 0
		  for(int i_cnt = 0;i_cnt<(sizeof(g_IpmbCmdVar)-1);i_cnt++)
		  {
				g_sum + = g_IpmbCmdVar[i_cnt];
		  }
		  if(g_IpmbCmdVar[sizeof(g_IpmbCmdVar)-1] != g_sum)
		  {
				return -1;
		  }
#endif
        index = Trans_udp_info_searce_by_addr(&addr);
        if (index < 0) // 如果没找到，说明是新的socket请求
        {
            memcpy(&g_epollInfo.udpInfo.src_addr[0], &addr, sizeof(struct sockaddr_in));
        }
        else // 如果找到则更新状态
        {
            printf("g_IpmbCmdVar serch OK\n");
            cnt++;
        }

        char IPdotdec[20];
        memset(IPdotdec, '\0', sizeof(IPdotdec));
        inet_ntop(AF_INET, (void *)&g_epollInfo.udpInfo.src_addr[0].sin_addr, IPdotdec, 16);
        unsigned short portNUM = ntohs(g_epollInfo.udpInfo.src_addr[0].sin_port);
        printf("src_addr[0] ip:%s    port:%d ...\n", IPdotdec, portNUM);
        g_epollInfo.udpInfo.src_addr[0].sin_port = htons(QT_RECV_PORT);
        portNUM = ntohs(g_epollInfo.udpInfo.src_addr[0].sin_port);
        printf("src_addr[0] ip:%s    port:%d ...\n", IPdotdec, portNUM);

        if (g_IpmbCmdVar[2] == 0x01)
        {
#if 0
            //可输入，即可写
            ret = Trans_epoll_change(EPOLL_CTL_MOD, (EPOLLOUT|EPOLLIN), info->udpsock);
            if(ret < 0)
            {
                printf("REQUEST_YC_START epoll_ctl udp error %s\n", strerror(errno));
                satellite_trans_snd_udp_usrid_check_result(-1, msg.head.msgType, info->udpsock, (struct sockaddr *) &addr);
                return -1;
            }
#endif
            unsigned char len = ((g_IpmbCmdVar[6] << 8) & 0xff00);
            len = (len | (g_IpmbCmdVar[7] & 0x00ff));
            Trans_snd_udp_usrid_check_result(len, g_IpmbCmdVar[2], info->UDPsend_socket, (struct sockaddr *)&g_epollInfo.udpInfo.src_addr[0]);
        }
        else if (g_IpmbCmdVar[2] == 0x02) // 加电
        {
            if(g_IpmbCmdVar[6] == 9)//表示要给其他7个模块都发送一遍指令。
            {
                for (int i = 2; i < 9;i++) //所有模块不包含0x01：主控板
                {
                    g_IpmbCmdVar[6] = i;
                    ipmbUnitCtrlFunc(g_IpmbCmdVar);
                    usleep(10000);
                }
            }
            else
            {
                ipmbUnitCtrlFunc(g_IpmbCmdVar);
            }
        }
    }
    else
    {
        printf("ck recive len error\n");
        return -1;
    }

    return 0;
}

/*******************************************************************************
* 函数名称: Trans_server_handle
* 功    能:
* 参    数: 参数名   数据类型    出/入参     含义
            ptr	 			void			   入参		  线程参数
* 函数返回: 无
* 说    明: 无
*******************************************************************************/
void *Trans_server_handle(void *ptr)
{
    int ret = 0;
    int i, nfds;
    int index = 0;
    int runclose = 0;
    struct epoll_event events[FD_SETSIZE];
    uint32_t eventsFd;
#if 0
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, 0);
#endif
    pthread_detach(pthread_self());

    snprintf(savfName, sizeof(savfName), "%s%s_recv.bin", "/run/media/mmcblk0p4/", "socket");
    savfiled = open(savfName, O_RDWR | O_CREAT, 0644); // |O_DIRECT);
    if (savfiled < 0)
    {
        printf("open file=%s is failed.\n", savfName);
        return NULL;
    }
    ret = gettimeofday(&tv_1, NULL);

    printf("satellite trans server handle start...\n");
    while (g_epollInfo.thrdRun)
    {
        if ((nfds = epoll_wait(g_epollInfo.efd, events, FD_SETSIZE, -1)) < 0)
        {
            printf("poll wait error\n");
            continue;
        }

        //		printf("out epoll wait nfds:%d...\n", nfds);
        for (i = 0; i < nfds; i++)
        {
            if ((events[i].data.fd == g_epollInfo.udpInfo.UDPrecv_socket) && (events[i].events & EPOLLIN))
            {
                Trans_ck_rcv_handle(&g_epollInfo.udpInfo);
            }
#if 0
			if((events[i].data.fd == g_epollInfo.udpInfo.udpsock) && (events[i].events & EPOLLOUT))
			{
				Trans_yc_snd(&g_epollInfo.udpInfo);	
			}
#endif
        }
    }

    printf("satellite_trans_server_handle exit!\n");
    return NULL;
}

int Trans_server_init(void)
{

    int ret = 0;

    if (g_epollInfo.initialized != true)
    {
        ret = epoll_info_init();
        if (ret < 0)
        {
            printf("epoll_info_init error\n");
            return ret;
        }
        // printf("satellite_trans_epoll_info_init\n");
    }

    if (g_epollInfo.udpInfo.UDPsend_socket < 0) // UDP
    {
        g_epollInfo.udpInfo.UDPsend_socket = socket_server_init(UDP, JISUANJI1_IP, SEND_PORT); // 创建天基UDP
        if (g_epollInfo.udpInfo.UDPsend_socket < 0)
        {
            printf("socket udp server init error, ip:%s port:%d\n", JISUANJI1_IP, SEND_PORT);
            goto error;
        }
        /*将UDP加入到epoll中*/
        ret = Trans_epoll_change(EPOLL_CTL_ADD, EPOLLIN, g_epollInfo.udpInfo.UDPsend_socket);
        if (ret < 0)
        {
            printf("epoll_ctl udp error\n");
            close(g_epollInfo.udpInfo.UDPsend_socket);
            g_epollInfo.udpInfo.UDPsend_socket = -1;
            goto error;
        }
    }

    if (g_epollInfo.udpInfo.UDPrecv_socket < 0) // UDP
    {
        g_epollInfo.udpInfo.UDPrecv_socket = socket_server_init(UDP, JISUANJI1_IP, RECV_PORT); // 创建天基UDP
        if (g_epollInfo.udpInfo.UDPrecv_socket < 0)
        {
            printf("socket udp server init error, ip:%s port:%d\n", JISUANJI1_IP, RECV_PORT);
            goto error;
        }
        /*将UDP加入到epoll中*/
        ret = Trans_epoll_change(EPOLL_CTL_ADD, EPOLLIN, g_epollInfo.udpInfo.UDPrecv_socket);
        if (ret < 0)
        {
            printf("epoll_ctl udp error\n");
            close(g_epollInfo.udpInfo.UDPrecv_socket);
            g_epollInfo.udpInfo.UDPrecv_socket = -1;
            goto error;
        }
    }

    if (g_epollInfo.initialized != true)
    {
        g_epollInfo.thrdRun = 1;
        ret = pthread_create(&g_epollInfo.hThread, NULL, Trans_server_handle, NULL);
        if (ret)
        {
            fprintf(stderr, "pthread_create Trans_server_handle Error: %s\n", strerror(errno));
            goto CLOSE_SOCK;
        }

        g_epollInfo.initialized = true;
        memset(&g_sateStatis, 0, sizeof(sateStatis_t));
        printf("pthread ok\n");
    }

    return ret;

CLOSE_SOCK:
    Trans_socket_close();
error:
    if (g_epollInfo.initialized != true)
    {
        epoll_info_exit();
    }
    return -1;
}

#if 0
int main(int argc,char **argv)
{
	int ret =0;
	ret = Trans_server_init();
	while(1)
	{
		sleep(1);
		}
}
#endif
