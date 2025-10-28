@echo off
echo 正在编译 Windows 版本的 Web 服务器...
echo.

set CXX=g++
set CXXFLAGS=-std=c++11 -Wall -O2
set TARGET=webserver.exe

if exist %TARGET% (
    echo 删除旧的可执行文件...
    del %TARGET%
)

%CXX% %CXXFLAGS% -o %TARGET% webserver_windows.cpp
if %ERRORLEVEL% NEQ 0 (
    echo 编译失败！
    pause
    exit /b 1
)

echo.
echo 编译成功！
echo 可执行文件: %TARGET%
echo.
echo 运行服务器: %TARGET%
echo 或直接执行: %TARGET%
echo.
pause

