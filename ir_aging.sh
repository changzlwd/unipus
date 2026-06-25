#!/system/bin/sh

while [ 1 ]
do
    echo -ne '\x30\x02\x00\x00\x9A\x06\x00\x00\x30\x02\x00\x00' > /dev/lirc0
    sleep 1
done
