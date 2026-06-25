@echo off
chcp 65001 >nul
title 红外老化测试工具

:MENU
cls
echo ========================================
echo    红外老化测试工具 v1.0
echo ========================================
echo.
echo   [1] 启动红外测试（后台运行，断开USB也继续）
echo   [2] 停止红外测试
echo   [3] 查看运行状态和日志
echo   [4] 推送脚本到设备（首次使用）
echo   [5] 退出
echo.
echo ========================================
set /p choice=请选择操作 (1-5):

if "%choice%"=="1" goto START
if "%choice%"=="2" goto STOP
if "%choice%"=="3" goto STATUS
if "%choice%"=="4" goto PUSH
if "%choice%"=="5" goto END
goto MENU

:START
echo.
echo 正在启动红外测试...
adb shell "killall ir_test.sh" 2>nul
adb shell "nohup /data/local/tmp/ir_test.sh > /data/local/tmp/ir.log 2>&1 &"
echo.
echo ========================================
echo   启动成功！红外测试正在后台运行
echo   断开USB后继续运行
echo ========================================
timeout /t 3 >nul
goto MENU

:STOP
echo.
echo 正在停止红外测试...
adb shell "killall ir_test.sh" 2>nul
adb shell "pkill -9 -f ir_test.sh" 2>nul
echo   已停止！
echo ========================================
timeout /t 3 >nul
goto MENU

:STATUS
echo.
echo [进程状态]
adb shell "ps -A | grep ir_test"
echo.
echo [最近日志]
adb shell "tail -10 /data/local/tmp/ir.log" 2>nul
echo.
echo ========================================
pause
goto MENU

:PUSH
echo.
echo 正在推送脚本到设备...
if not exist "%~dp0ir_test.sh" (
    echo   错误: ir_test.sh 不存在！
    echo   请将 ir_test.sh 放在本目录
    pause
    goto MENU
)
adb push "%~dp0ir_test.sh" /data/local/tmp/
adb shell "chmod 755 /data/local/tmp/ir_test.sh"
echo   推送完成！
echo ========================================
timeout /t 3 >nul
goto MENU

:END
exit
