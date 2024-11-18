# TinyHTTPd

一个简单的HTTP服务器实现，用于学习和演示HTTP协议的基本工作原理。这个项目基于J. David Blackstone的原始代码，进行了现代化改进和功能增强。

## 功能特点

- 支持基本的HTTP GET和POST请求
- 提供静态文件服务
- 支持CGI脚本执行
- 多线程处理客户端请求
- 现代化的Web界面演示
- 请求/响应信息可视化

## 构建和运行

### 准备工作

确保系统已安装：
- GCC编译器
- Make工具
- Perl（用于CGI脚本）

### 编译

```bash
make clean  # 清理之前的编译文件
make       # 编译服务器
```

### 运行

```bash
./httpd    # 默认监听4000端口
```

## 使用演示

1. 启动服务器后，访问：
   ```
   http://localhost:4000/
   ```

2. Web界面提供两个主要功能：
   - 基本消息发送：演示HTTP POST请求
   - 静态文件请求：演示HTTP GET请求

3. 每个请求都会显示：
   - 完整的HTTP请求信息
   - 服务器的响应内容

## 项目结构

```
.
├── httpd.c          # 服务器主程序
├── Makefile         # 编译配置
├── htdocs/          # Web根目录
│   ├── index.html   # 主页面
│   ├── test.html    # 测试页面
│   └── message.cgi  # 消息处理脚本
└── README.md        # 项目说明
```

## 主要函数说明

- `accept_request`: 处理HTTP请求
- `execute_cgi`: 执行CGI脚本
- `get_line`: 读取HTTP请求行
- `serve_file`: 提供静态文件服务
- `startup`: 初始化服务器

## 注意事项

1. 这是一个教学用的简单HTTP服务器，不建议用于生产环境
2. 默认端口为4000，可以通过修改源码更改
3. 确保CGI脚本有执行权限：
   ```bash
   chmod +x htdocs/*.cgi
   ```

## 许可证

本项目遵循 GNU General Public License v3.0 开源许可证。

## 致谢

- 原作者：J. David Blackstone
- 本项目在原始代码基础上进行了改进和现代化改造

