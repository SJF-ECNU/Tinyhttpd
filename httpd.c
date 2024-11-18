/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdarg.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

// 在文件开头添加新的响应头定义
#define CONTENT_LENGTH "Content-Length: %d\r\n"
#define CONTENT_TYPE "Content-Type: %s\r\n"
#define CONNECTION "Connection: %s\r\n"
#define DATE "Date: %s\r\n"

/*
 * 这是一个简单的HTTP服务器实现
 * 主要功能：
 * 1. 处理基本的HTTP GET和POST请求
 * 2. 支持静态文件服务
 * 3. 支持CGI脚本执行
 * 4. 多线程处理客户端请求
 */

void accept_request(void *); // 处理HTTP请求
void bad_request(int);      // 发送400错误响应
void cat(int, FILE *);      // 发送文件内容
void cannot_execute(int);   // 发送500错误响应
void error_die(const char *); // 错误处理和退出
void execute_cgi(int, const char *, const char *, const char *); // 执行CGI脚本
int get_line(int, char *, int); // 读取一行HTTP请求
void headers(int, const char *); // 发送HTTP响应头
void not_found(int);        // 发送404错误响应
void serve_file(int, const char *); // 处理静态文件请求
int startup(u_short *);     // 启动服务器
void unimplemented(int);    // 发送501错误响应
void log_access(const char *format, ...); // 记录访问日志
const char* get_content_type(const char *filename); // 添加这一行声明

// 添加配置结构
typedef struct {
    int port;
    char document_root[512];
    int max_clients;
    int timeout;
} server_config;

// 添加配置读取函数
server_config read_config(const char *filename) 
{
    server_config config = {
        .port = 4000,
        .document_root = "htdocs",
        .max_clients = 1000,
        .timeout = 60
    };
    
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return config;
    
    char line[1024];
    char key[64], value[960];
    
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        if (sscanf(line, "%63[^=]=%959s", key, value) == 2) {
            if (strcmp(key, "port") == 0)
                config.port = atoi(value);
            else if (strcmp(key, "document_root") == 0)
                strncpy(config.document_root, value, sizeof(config.document_root)-1);
            else if (strcmp(key, "max_clients") == 0)
                config.max_clients = atoi(value);
            else if (strcmp(key, "timeout") == 0)
                config.timeout = atoi(value);
        }
    }
    
    fclose(fp);
    return config;
}

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(void *arg)
{
    /* 处理HTTP请求的主要步骤：
     * 1. 解析HTTP请求行，获取方法、URL和HTTP版本
     * 2. 如果是GET请求，解析URL中的查询字符串
     * 3. 如果是POST请求，获取Content-Length
     * 4. 确定是否需要CGI处理（有查询字符串或POST请求）
     * 5. 构建本地文件路径
     * 6. 根据请求类型调用相应的处理函数
     */
    int client = (intptr_t)arg; // 将传入的参数转换为客户端套接字描述符
    char buf[1024]; // 用于存储从客户端读取的数据
    size_t numchars; // 读取的字符数
    char method[255]; // 存储请求方法（GET 或 POST）
    char url[255]; // 存储请求的 URL
    char path[512]; // 存储请求的文件路径
    size_t i, j;
    struct stat st; // 用于获取文件状态信息
    int cgi = 0; // 标记是否为 CGI 请求
    char *query_string = NULL; // 存储查询字符串

    /* 处理HTTP请求的主要步骤：
     * 1. 解析HTTP请求方法（GET/POST）
     * 2. 解析URL和查询字符串
     * 3. 确定是否需要CGI处理
     * 4. 处理静态文件或执行CGI脚本
     */

    // 获取HTTP请求的第一行
    numchars = get_line(client, buf, sizeof(buf));
    
    // 解析HTTP方法（GET/POST）
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j = i;
    method[i] = '\0';

    // 检查是否支持该HTTP方法
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return;
    }

    // POST请求一定需要CGI处理
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    // 跳过空白字符
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    // 解析 URL
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    /* 处理GET请求的查询字符串
     * 如果URL中包含?，则需要CGI处理
     * 例如：/path?param=value
     */
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    /* 构建本地文件路径
     * 所有文件都存放在htdocs目录下
     * 如果请求的是目录，默认返回index.html
     */
    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

    // 检查文件是否存在和访问权限
    if (stat(path, &st) == -1) {
        // 文件不存在，返回404错误
        while ((numchars > 0) && strcmp("\n", buf))
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else
    {
        // 如果是目录，添加默认的index.html
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        
        // 如果文件有执行权限，认为是CGI脚本
        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH))
            cgi = 1;
        
        // 检查路径中是否包含 ..
        if (strstr(url, "..") != NULL) {
            bad_request(client);
            return;
        }
        
        // 检查请求的文件路径是否超出htdocs目录
        char real_path[512];
        char *htdocs_path = realpath("htdocs", NULL);
        if (realpath(path, real_path) == NULL || 
            strncmp(real_path, htdocs_path, strlen(htdocs_path)) != 0) {
            not_found(client);
            free(htdocs_path);
            return;
        }
        free(htdocs_path);
        
        // 根据是否是CGI请求选择处理方式
        if (!cgi)
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string);
    }

    // 记录访问日志
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(client, (struct sockaddr*)&addr, &addr_len);
    
    log_access("%s - \"%s %s\" %d", 
               inet_ntoa(addr.sin_addr),
               method,
               url,
               200); // 这里应该根据实际响应码修改
               
    close(client);
}
/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
        const char *method, const char *query_string)
{
    /* CGI脚本执行流程：
     * 1. 创建两个管道用于父子进程通信
     * 2. fork出子进程
     * 3. 在子进程中：
     *    - 重定向标准输入输出到管道
     *    - 设置环境变量
     *    - 执行CGI程序
     * 4. 在父进程中：
     *    - 如果是POST请求，将POST数据写入子进程
     *    - 读取CGI程序的输出并发送给客户端
     */
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else if (strcasecmp(method, "POST") == 0) /*POST*/
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }
    else/*HEAD or other*/
    {
    }


    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    if ( (pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], STDOUT);
        dup2(cgi_input[0], STDIN);
        close(cgi_output[0]);
        close(cgi_input[1]);
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, NULL);
        exit(0);
    } else {    /* parent */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    char date_str[100];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    const char *content_type;
    struct stat st;
    
    // 获取文件大小
    if (stat(filename, &st) == -1)
        return;
        
    // 根据文件扩展名确定Content-Type
    content_type = get_content_type(filename);
    
    // 格式化HTTP日期
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    
    // 发送基本响应头
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, DATE, date_str);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, CONTENT_TYPE, content_type);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, CONTENT_LENGTH, (int)st.st_size);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, CONNECTION, "close");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    // 对静态资源添加缓存控制
    if (strstr(filename, ".html") || strstr(filename, ".htm")) {
        // HTML文件不缓存
        sprintf(buf, "Cache-Control: no-cache\r\n");
    } else {
        // 其他静态资源缓存1小时
        sprintf(buf, "Cache-Control: public, max-age=3600\r\n");
    }
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    /* 服务器启动流程：
     * 1. 创建服务器套接字
     * 2. 设置套接字选项（地址重用）
     * 3. 绑定到指定端口（如果端口为0则动态分配）
     * 4. 开始监听连接
     * 返回：服务器套接字描述符
     */
    int httpd = 0;
    int on = 1;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)  
    {  
        error_die("setsockopt failed");
    }
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    /* 主程序流程：
     * 1. 初始化服务器，监听指定端口（默认4000）
     * 2. 进入主循环：
     *    - 接受新的客户端连接
     *    - 为每个连接创建新线程处理请求
     *    - 线程独立运行，主线程继续接受新连接
     * 3. 服务器永久运行，除非发生错误或被手动终止
     */
    int server_sock = -1;
    u_short port = 4000;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t  client_name_len = sizeof(client_name);
    pthread_t newthread;

    /* 主函数：启动服务器并处理客户端连接 */

    // 初始化服务器，监听指定端口
    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    /* 主循环：接受客户端连接并创建新线程处理请求
     * 每个客户端连接都在独立的线程中处理
     * 支持多个客户端同时连接
     */
    while (1)
    {
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        
        // 创建新线程处理请求
        if (pthread_create(&newthread, NULL, (void *)accept_request, 
                (void *)(intptr_t)client_sock) != 0)
            perror("pthread_create");
    }

    close(server_sock);

    return(0);
}

/**********************************************************************/
/* 添加获取Content-Type的辅助函数 */
/**********************************************************************/
const char* get_content_type(const char *filename) 
{
    const char *dot = strrchr(filename, '.');
    if (!dot) return "text/plain";
    
    if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) 
        return "text/html";
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcasecmp(dot, ".png") == 0)
        return "image/png";
    if (strcasecmp(dot, ".css") == 0)
        return "text/css";
    if (strcasecmp(dot, ".js") == 0)
        return "application/javascript";
    if (strcasecmp(dot, ".pdf") == 0)
        return "application/pdf";
        
    return "text/plain";
}

// 添加日志函数
void log_access(const char *format, ...) 
{
    FILE *fp;
    va_list arg_list;
    char timestamp[100];
    time_t now = time(NULL);
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", 
             localtime(&now));
    
    fp = fopen("access.log", "a");
    if (fp == NULL) return;
    
    fprintf(fp, "[%s] ", timestamp);
    va_start(arg_list, format);
    vfprintf(fp, format, arg_list);
    va_end(arg_list);
    fprintf(fp, "\n");
    
    fclose(fp);
}
