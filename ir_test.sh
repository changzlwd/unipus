#!/system/bin/sh
# 红外老化测试脚本
# 每1秒发送 560, 1690, 560 三个数据

while :
do
  echo -ne '\x30\x02\x00\x00\x9A\x06\x00\x00\x30\x02\x00\x00' > /dev/lirc0
  sleep 1
done
