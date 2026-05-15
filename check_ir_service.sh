# 执行以下命令并把结果发给我

# 1. 尝试手动启动服务
/vendor/bin/hw/android.hardware.ir-service.volcano &

# 2. 等待几秒后检查服务是否在运行
sleep 3
ps -A | grep ir-service

# 3. 查看 SELinux 权限拒绝日志
dmesg | grep -i "avc\|denied" | tail -20

# 4. 查看服务相关日志
logcat | grep -i "ir\|ConsumerIr" | tail -30
