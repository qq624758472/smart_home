/*
 * @Description: UDP server in C language
 * @Version: 2.0
 * @Author: ruog__
 * @Date: 2024-05-30 15:32:10
 * @LastEditors: ruog__
 * @LastEditTime: 2024-11-19 11:20:00
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>

#define PORT 20000
#define RESPONSE_PORT 20001
#define MAX_BUFFER_SIZE 1060
#define DATA_SIZE 1024

int udp_test()
{
    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_size;
    char buffer[MAX_BUFFER_SIZE];
    char response[36] = {0};

    // 创建 UDP 套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Error creating socket");
        return 1;
    }

    memset(&serverAddr, '\0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 绑定到指定端口
    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Error binding");
        close(sockfd);
        return 1;
    }

    printf("Server listening on port %d\n", PORT);

    while (1)
    {
        // 接收数据包
        addr_size = sizeof(clientAddr);
        int recv_len = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &addr_size);
        if (recv_len < 0)
        {
            perror("Error receiving data");
            continue;
        }

        // 修改客户端端口号为 20001
        clientAddr.sin_port = htons(RESPONSE_PORT);

        // 拷贝前 36 字节到响应字段
        memcpy(response, buffer, 36);

        // 发送响应
        if (sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
        {
            perror("Error sending response");
            continue;
        }

        // 打开文件并写入数据
        FILE *outFile = fopen("data.txt", "ab"); // 打开文件进行追加
        if (outFile == NULL)
        {
            perror("Error opening file");
            continue;
        }

        // 写入数据到文件（从第 36 字节开始写入）
        if (fwrite(buffer + 36, 1, DATA_SIZE, outFile) != DATA_SIZE)
        {
            perror("Error writing to file");
        }
        else
        {
            printf("Data written to file\n");
        }

        fclose(outFile);
    }

    close(sockfd);
    return 0;
}
