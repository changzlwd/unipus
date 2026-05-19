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
#include <sys/time.h>

#include <linux/lirc.h>

#include <log/log.h>

#include <hardware/consumerir.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define LIRC_DEVICE_PATH "/dev/lirc0"
#define TRAILING_SPACE_US 5600

static const consumerir_freq_range_t consumerir_freqs[] = {
    {.min = 30000, .max = 60000},
};

static long long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

static int consumerir_transmit(struct consumerir_device *dev __unused,
   int carrier_freq, const int pattern[], int pattern_len)
{
    int fd = -1;
    int ret = 0;
    int i;

    long long start_time = get_time_us();
    ALOGE("consumerir_transmit: called for %d Hz, %d slices", carrier_freq, pattern_len);

    if (pattern_len <= 0 || !pattern) {
        ALOGE("Invalid pattern or pattern_len");
        return -1;
    }

    int final_len = pattern_len;
    int *final_pattern = NULL;
    bool need_free = false;

    if (final_len % 2 == 0) {
        final_len++;
        final_pattern = malloc(final_len * sizeof(int));
        if (!final_pattern) {
            ALOGE("Failed to allocate memory for pattern");
            return -1;
        }
        memcpy(final_pattern, pattern, pattern_len * sizeof(int));
        final_pattern[final_len - 1] = TRAILING_SPACE_US;
        ALOGE("Pattern is even (%d), adding trailing space %d us, new length: %d",
              pattern_len, TRAILING_SPACE_US, final_len);
        need_free = true;
    } else {
        final_pattern = (int *)pattern;
    }

    unsigned int *tx_buf = malloc(final_len * sizeof(unsigned int));
    if (!tx_buf) {
        ALOGE("Failed to allocate tx buffer");
        if (need_free)
            free(final_pattern);
        return -1;
    }

    for (i = 0; i < final_len; i++) {
        tx_buf[i] = (unsigned int)final_pattern[i];
    }

    fd = open(LIRC_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        ALOGE("Cannot open LIRC device: %s, error: %s", LIRC_DEVICE_PATH, strerror(errno));
        free(tx_buf);
        if (need_free)
            free(final_pattern);
        return -1;
    }
    ALOGE("Opened LIRC device fd=%d", fd);

    unsigned int mode = LIRC_MODE_PULSE;
    if (ioctl(fd, LIRC_SET_SEND_MODE, &mode) < 0) {
        ALOGE("LIRC_SET_SEND_MODE failed: %s", strerror(errno));
    }

    if (ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier_freq) < 0) {
        ALOGE("LIRC_SET_SEND_CARRIER failed: %s", strerror(errno));
    } else {
        ALOGE("LIRC_SET_SEND_CARRIER succeeded: %d Hz", carrier_freq);
    }

    long long write_start = get_time_us();
    ssize_t bytes_to_write = final_len * sizeof(unsigned int);
    ssize_t bytes_written = write(fd, tx_buf, bytes_to_write);
    long long write_end = get_time_us();

    if (bytes_written != bytes_to_write) {
        ALOGE("Write to LIRC device failed: %s, written %zd bytes, expected %zd",
              strerror(errno), bytes_written, bytes_to_write);
        ret = -1;
    } else {
        ALOGE("Successfully wrote %zd bytes (%d samples) to LIRC, took %lld us",
              bytes_written, final_len, write_end - write_start);

        long long end_time = get_time_us();
        ALOGE("IR transmission completed, total time: %lld us", end_time - start_time);
    }

    close(fd);
    free(tx_buf);
    if (need_free)
        free(final_pattern);
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
