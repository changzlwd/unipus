/* ir_aging.c - IR aging test */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/lirc.h>

int main(void) {
    int fd;
    unsigned int txbuf[3] = {560, 1690, 560};

    fd = open("/dev/lirc0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    ioctl(fd, LIRC_SET_SEND_MODE, &(unsigned int){LIRC_MODE_PULSE});
    ioctl(fd, LIRC_SET_SEND_CARRIER, &(unsigned int){38000});

    printf("IR aging test: 560, 1690, 560 every 1s\n");

    while (1) {
        write(fd, txbuf, sizeof(txbuf));
        sleep(1);
    }

    return 0;
}
