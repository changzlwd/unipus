@echo off
chcp 65001 >nul
echo =======================================
echo   红外老化测试 - 一键启动
echo =======================================

echo [1/5] 检查adb连接...
adb devices >nul 2>&1
if errorlevel 1 goto adb_error

echo [2/5] 推送脚本到设备...
adb push "%~dp0ir_test.sh" /data/local/tmp/
if errorlevel 1 goto push_error

echo [3/5] 设置权限...
adb shell "chmod 755 /data/local/tmp/ir_test.sh"

echo [4/5] 停止旧进程...
adb shell "killall ir_test.sh" 2>nul

echo [5/5] 启动红外测试...
adb shell "nohup /data/local/tmp/ir_test.sh > /data/local/tmp/ir.log 2>&1 &"
if errorlevel 1 goto start_error

echo.
echo =======================================
echo   启动成功！红外测试正在后台运行
echo =======================================
echo   停止: 双击 stop_ir_test.bat
echo   状态: 双击 status_ir_test.bat
echo =======================================
echo.
echo   断开USB后红外测试继续运行
echo.
pause
exit /b 0

:adb_error
echo.
echo =======================================
echo   错误: adb未找到或连接失败
echo =======================================
echo   请检查:
echo   1. USB线是否连接
echo   2. 设备是否开启USB调试
echo   3. 设备是否已授权此电脑
echo =======================================
pause
exit /b 1

:push_error
echo.
echo =======================================
echo   错误: 推送脚本失败
echo =======================================
echo   可能原因:
echo   1. /data/local/tmp/ 不可写
echo   2. 存储空间不足
echo   3. USB连接不稳定
echo =======================================
echo   建议: 重新插拔USB后再试
echo =======================================
pause
exit /b 1

:start_error
echo.
echo =======================================
echo   错误: 启动失败
echo =======================================
pause
exit /b 1
