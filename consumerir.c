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
#define TRAILING_SPACE_US 10
#define MAX_SINGLE_DURATION_US 50000  // 单个数据最大50ms，超过则砍成40ms
#define CLAMP_DURATION_US 40000       // 砍成40ms
#define MAX_TOTAL_DURATION_US 500000  // 总时长最大500ms

static const consumerir_freq_range_t consumerir_freqs[] = {
    {.min = 30000, .max = 60000},
};

static int consumerir_transmit(struct consumerir_device *dev __unused,
   int carrier_freq, const int pattern[], int pattern_len)
{
    int fd = -1;
    int ret = 0;
    int i;
    int final_len;
    unsigned int *tx_buf = NULL;
    unsigned int total_duration = 0;
    bool need_clamp = false;
    bool added_trailing = false;

    ALOGI("consumerir_transmit: called for %d Hz, %d slices", carrier_freq, pattern_len);

    if (pattern_len <= 0 || !pattern) {
        ALOGE("Invalid pattern or pattern_len");
        return -1;
    }

    // 计算总时长
    for (i = 0; i < pattern_len; i++) {
        total_duration += (unsigned int)pattern[i];
    }
    ALOGI("Total duration: %u us (%d ms)", total_duration, total_duration / 1000);

    // 判断是否需要clamp
    need_clamp = (total_duration > MAX_TOTAL_DURATION_US);

    // 计算最终长度（偶数需要加trailing space变奇数）
    final_len = pattern_len;
    if (final_len % 2 == 0) {
        final_len++;
        added_trailing = true;
    }

    // 分配内存
    tx_buf = malloc(final_len * sizeof(unsigned int));
    if (!tx_buf) {
        ALOGE("Failed to allocate tx buffer, size=%d", final_len);
        return -1;
    }

    // 复制数据，超过50ms的砍成40ms
    for (i = 0; i < pattern_len; i++) {
        unsigned int val = (unsigned int)pattern[i];
        if (need_clamp && val > MAX_SINGLE_DURATION_US) {
            ALOGI("Clamping pattern[%d] from %u to %d us", i, val, CLAMP_DURATION_US);
            val = CLAMP_DURATION_US;
        }
        tx_buf[i] = val;
    }

    // 偶数数据添加trailing space
    if (added_trailing) {
        tx_buf[pattern_len] = TRAILING_SPACE_US;
        ALOGI("Pattern is even (%d), added trailing space %d us, new length: %d",
              pattern_len, TRAILING_SPACE_US, final_len);
    }

    // 打开设备
    fd = open(LIRC_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        ALOGE("Cannot open LIRC device: %s, error: %s", LIRC_DEVICE_PATH, strerror(errno));
        free(tx_buf);
        return -1;
    }
    ALOGI("Opened LIRC device fd=%d", fd);

    // 设置发送模式
    unsigned int mode = LIRC_MODE_PULSE;
    if (ioctl(fd, LIRC_SET_SEND_MODE, &mode) < 0) {
        ALOGE("LIRC_SET_SEND_MODE failed: %s", strerror(errno));
    }

    // 设置载波频率
    if (ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier_freq) < 0) {
        ALOGE("LIRC_SET_SEND_CARRIER failed: %s", strerror(errno));
    } else {
        ALOGI("LIRC_SET_SEND_CARRIER succeeded: %d Hz", carrier_freq);
    }

    // 发送数据
    ssize_t bytes_to_write = final_len * sizeof(unsigned int);
    ssize_t bytes_written = write(fd, tx_buf, bytes_to_write);

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
        .name               = "Demo IR HAL",
        .author             = "The Android Open Source Project",
        .methods            = &consumerir_module_methods,
    },
};
