@echo off
chcp 65001 >nul
echo =======================================
echo   红外老化测试 - 运行状态
echo =======================================
echo.
echo [进程信息]
adb shell "ps -A | grep ir_test"
echo.
echo [最近日志]
adb shell "tail -20 /data/local/tmp/ir.log" 2>nul
echo.
echo =======================================
echo   快捷操作:
echo   1. 双击 run_ir_test.bat   - 启动测试
echo   2. 双击 stop_ir_test.bat  - 停止测试
echo   3. 重新双击本脚本         - 刷新状态
echo =======================================
pause
