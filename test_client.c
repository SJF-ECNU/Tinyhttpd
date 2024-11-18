#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_hex_dump(const char* data, size_t len) {
    printf("数据包内容 (十六进制):\n");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", (unsigned char)data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n\n");
}

int main(int argc, char *argv[]) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    char message[] = "{\"message\":\"Hello Server\"}";
    char buffer[2048];
    
    // 设置服务器信息
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(4000);
    
    // 连接服务器
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("连接失败");
        return 1;
    }
    
    // 构建HTTP请求
    char request[1024];
    sprintf(request, 
        "POST /echo HTTP/1.1\r\n"
        "Host: localhost:4000\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %ld\r\n"
        "\r\n"
        "%s", strlen(message), message);
    
    printf("发送的请求:\n%s\n", request);
    print_hex_dump(request, strlen(request));
    
    // 发送请求
    send(sock, request, strlen(request), 0);
    
    // 接收响应
    int bytes_received = recv(sock, buffer, sizeof(buffer)-1, 0);
    buffer[bytes_received] = '\0';
    
    printf("收到的响应:\n%s\n", buffer);
    print_hex_dump(buffer, bytes_received);
    
    close(sock);
    return 0;
} 