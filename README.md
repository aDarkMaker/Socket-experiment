# Socket 编程实验 - Web 服务器

基于 POSIX Socket API 实现的 Web 服务器。

## 已完成功能

- 可配置监听地址、端口、主目录
- HTTP 请求解析与响应
- 多文件类型支持（HTML/CSS/JS/图片/JSON等）
- 请求日志输出
- 错误处理（400/403/404/500/501）
- 异常情况处理

## 文件结构

```
Expr-Socket/
├── webserver.cpp          # Web 服务器主程序 (Linux/Mac)
├── webserver_windows.cpp # Web 服务器主程序 (Windows)
├── server_config.txt      # 服务器配置文件
├── Makefile              # 编译文件 (Linux/Mac)
├── build_windows.bat     # Windows 编译脚本
├── README.md             # 本文件
├── README_WINDOWS.md     # Windows 详细文档
└── www/                  # Web 根目录
    ├── index.html        # 首页
    └── test.html         # 测试页面
```

## 快速开始

### Linux/Mac

```bash
# 编译
make

# 运行
./webserver
```

### Windows

```bash
# 双击运行或命令行执行
build_windows.bat

# 运行
webserver.exe
```

访问：http://localhost:8080

## 配置

编辑 `server_config.txt` 修改监听地址、端口、根目录。

```ini
listen_address=0.0.0.0
listen_port=8080
web_root=www
```

## 技术栈

- C++11 + POSIX Socket
- HTTP/1.1
- 单线程模型

## 日志输出

服务器控制台输出：
- 客户端 IP:端口
- 请求方法与路径
- 响应状态码