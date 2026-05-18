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

    ALOGE("consumerir_transmit: called for %d Hz, %d slices", carrier_freq, pattern_len);

    fd = open(LIRC_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        ALOGE("Cannot open LIRC device: %s, error: %s", LIRC_DEVICE_PATH, strerror(errno));
        return -1;
    }
    ALOGE("Opened LIRC device fd=%d", fd);

    unsigned int mode = LIRC_MODE_PULSE;
    if (ioctl(fd, LIRC_SET_SEND_MODE, &mode) < 0) {
        ALOGE("LIRC_SET_SEND_MODE failed: %s", strerror(errno));
    }

    if (ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier_freq) < 0) {
        ALOGE("LIRC_SET_SEND_CARRIER failed: %s", strerror(errno));
    }

    unsigned char *byte_buf = malloc(pattern_len * sizeof(int));
    if (!byte_buf) {
        ALOGE("Failed to allocate byte buffer");
        close(fd);
        return -ENOMEM;
    }

    for (i = 0; i < pattern_len; i++) {
        uint32_t val = pattern[i];
        byte_buf[i*4] = val & 0xFF;
        byte_buf[i*4+1] = (val >> 8) & 0xFF;
        byte_buf[i*4+2] = (val >> 16) & 0xFF;
        byte_buf[i*4+3] = (val >> 24) & 0xFF;
    }

    ssize_t bytes_written = write(fd, byte_buf, pattern_len * sizeof(int));
    if (bytes_written != pattern_len * sizeof(int)) {
        ALOGE("Write to LIRC device failed: %s, written %zd bytes, expected %zd", 
              strerror(errno), bytes_written, pattern_len * sizeof(int));
        ret = -1;
    } else {
        ALOGE("Successfully wrote %zd bytes to LIRC", bytes_written);
    }

    free(byte_buf);

    close(fd);
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

    *device = (hw_device_t*) dev;
    ALOGI("Consumer IR device opened successfully with LIRC");
    return 0;
}

static struct hw_module_methods_t consumerir_module_methods = {
    .open = consumerir_open,
};

consumerir_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_DEVICE_TAG,
        .module_api_version = CONSUMERIR_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = CONSUMERIR_HARDWARE_MODULE_ID,
        .name               = "LIRC IR HAL",
        .author             = "Custom IR HAL Implementation",
        .methods            = &consumerir_module_methods,
    },
};

