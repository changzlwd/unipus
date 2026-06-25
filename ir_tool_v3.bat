@echo off
chcp 65001 >nul
title 红外老化测试工具 v3.0

:MENU
cls
echo ========================================
echo    红外老化测试工具 v3.0
echo ========================================
echo.
echo   [1] 启动红外测试
echo   [2] 停止红外测试
echo   [3] 查看运行状态
echo   [4] 退出
echo.
echo ========================================
set /p choice=请选择操作 (1-4):

if "%choice%"=="1" goto START
if "%choice%"=="2" goto STOP
if "%choice%"=="3" goto STATUS
if "%choice%"=="4" goto END
goto MENU

:START
echo.
echo 正在启动红外测试...

REM 停止旧的循环
adb shell killall sh >nul 2>&1

REM 启动循环 - 使用文件方式避免转义问题
adb shell "echo while : ^> /data/local/tmp/ir_loop.sh"
adb shell "echo do ^>^> /data/local/tmp/ir_loop.sh"
adb shell "echo echo -ne '\x30\x02\x00\x00\x9A\x06\x00\x00\x30\x02\x00\x00' ^> /dev/lirc0 ^>^> /data/local/tmp/ir_loop.sh"
adb shell "echo sleep 1 ^>^> /data/local/tmp/ir_loop.sh"
adb shell "echo done ^>^> /data/local/tmp/ir_loop.sh"

adb shell chmod 755 /data/local/tmp/ir_loop.sh
adb shell nohup /data/local/tmp/ir_loop.sh > /data/local/tmp/ir.log 2>&1 &

echo.
echo ========================================
echo   启动成功！
echo ========================================
timeout /t 3 >nul
goto MENU

:STOP
echo.
echo 正在停止红外测试...
adb shell killall sh >nul 2>&1
adb shell killall ir_loop.sh >nul 2>&1
echo   已停止！
echo ========================================
timeout /t 3 >nul
goto MENU

:STATUS
echo.
echo [进程状态]
adb shell "ps -A | grep -E sh" 2>nul
echo.
echo [设备连接]
adb devices
echo.
echo ========================================
pause
goto MENU

:END
exit
