/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "ConsumerIrHal"

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <linux/lirc.h>

#include <log/log.h>

#include <hardware/consumerir.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
// liuqizhi 20260521 implement infrared remote control
#define LIRC_DEVICE_PATH "/dev/lirc0"

static const consumerir_freq_range_t consumerir_freqs[] = {
    {.min = 30000, .max = 60000},
};

static int consumerir_transmit(struct consumerir_device *dev __unused,
   int carrier_freq, const int pattern[], int pattern_len)
{
    int fd = -1;
    int ret = 0;
    int i;

    ALOGI("consumerir_transmit: called for %d Hz, %d slices", carrier_freq, pattern_len);

    if (pattern_len <= 0 || !pattern) {
        ALOGE("Invalid pattern or pattern_len");
        return -1;
    }

    ALOGI("Pattern data (first 50):");
    for (i = 0; i < pattern_len && i < 50; i++) {
        ALOGI("pattern[%d] = %d", i, pattern[i]);
    }
    if (pattern_len > 50) {
        ALOGI("... and %d more samples", pattern_len - 50);
    }

    int final_len = pattern_len;
    const int *final_pattern = pattern;
    unsigned int *tx_buf = NULL;

    tx_buf = malloc(final_len * sizeof(unsigned int));
    if (!tx_buf) {
        ALOGE("Failed to allocate tx buffer");
        return -1;
    }
    for (i = 0; i < final_len; i++) {
        tx_buf[i] = (unsigned int)final_pattern[i];
    }

    fd = open(LIRC_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        ALOGE("Cannot open LIRC device: %s, error: %s", LIRC_DEVICE_PATH, strerror(errno));
        free(tx_buf);
        return -1;
    }
    ALOGI("Opened LIRC device fd=%d", fd);

    unsigned int mode = LIRC_MODE_PULSE;
    if (ioctl(fd, LIRC_SET_SEND_MODE, &mode) < 0) {
        ALOGE("LIRC_SET_SEND_MODE failed: %s", strerror(errno));
    }

    if (ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier_freq) < 0) {
        ALOGE("LIRC_SET_SEND_CARRIER failed: %s", strerror(errno));
    } else {
        ALOGI("LIRC_SET_SEND_CARRIER succeeded: %d Hz", carrier_freq);
    }

    ssize_t bytes_to_write = final_len * sizeof(unsigned int);

    ALOGE("consumerir_transmit: About to write %zd bytes (%d samples) to fd=%d",
          bytes_to_write, final_len, fd);

    ssize_t bytes_written = write(fd, tx_buf, bytes_to_write);

    ALOGE("consumerir_transmit: write() returned %zd, errno=%d (%s)",
          bytes_written, errno, strerror(errno));

    if (bytes_written != bytes_to_write) {
        ALOGE("Write to LIRC device failed: %s, written %zd bytes, expected %zd",
              strerror(errno), bytes_written, bytes_to_write);
        ret = -1;
    } else {
        ALOGI("Successfully wrote %zd bytes (%d samples) to LIRC",
              bytes_written, final_len);
    }

    close(fd);
    free(tx_buf);
    return ret;
}

static int consumerir_get_num_carrier_freqs(struct consumerir_device *dev __unused)
{
    return ARRAY_SIZE(consumerir_freqs);
}

static int consumerir_get_carrier_freqs(struct consumerir_device *dev __unused,
    size_t len, consumerir_freq_range_t *ranges)
{
    size_t to_copy = ARRAY_SIZE(consumerir_freqs);

    to_copy = len < to_copy ? len : to_copy;
    memcpy(ranges, consumerir_freqs, to_copy * sizeof(consumerir_freq_range_t));
    return to_copy;
}

static int consumerir_close(hw_device_t *dev)
{
    free(dev);
    return 0;
}

static int consumerir_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    int init_fd;
    unsigned int mode;

    if (strcmp(name, CONSUMERIR_TRANSMITTER) != 0) {
        ALOGE("Invalid name for IR device: %s", name);
        return -EINVAL;
    }
    if (device == NULL) {
        ALOGE("NULL device on open");
        return -EINVAL;
    }

    consumerir_device_t *dev = malloc(sizeof(consumerir_device_t));
    if (dev == NULL) {
        ALOGE("Failed to allocate memory for IR device");
        return -ENOMEM;
    }
    memset(dev, 0, sizeof(consumerir_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = consumerir_close;

    dev->transmit = consumerir_transmit;
    dev->get_num_carrier_freqs = consumerir_get_num_carrier_freqs;
    dev->get_carrier_freqs = consumerir_get_carrier_freqs;

    ALOGE("consumerir_open: trying to pre-initialize LIRC device");
    init_fd = open(LIRC_DEVICE_PATH, O_RDWR);
    if (init_fd >= 0) {
        mode = LIRC_MODE_PULSE;
        ioctl(init_fd, LIRC_SET_SEND_MODE, &mode);
        close(init_fd);
        ALOGE("consumerir_open: LIRC device pre-initialized successfully, fd=%d", init_fd);
    } else {
        ALOGE("consumerir_open: FAILED to pre-initialize LIRC device: %s", strerror(errno));
    }

    *device = (hw_device_t*) dev;
    ALOGI("Consumer IR device opened successfully with LIRC");
    return 0;
}

static struct hw_module_methods_t consumerir_module_methods = {
    .open = consumerir_open,
};

consumerir_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = CONSUMERIR_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = CONSUMERIR_HARDWARE_MODULE_ID,
        .name               = "Demo IR HAL",
        .author             = "The Android Open Source Project",
        .methods            = &consumerir_module_methods,
    },
};
