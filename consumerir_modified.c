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
#define LOG_NDEBUG 0

#define LOG_TAG "ConsumerIrHal"
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <log/log.h>
#include <hardware/hardware.h>
#include <hardware/consumerir.h>
#include <sys/ioctl.h>
#include <linux/lirc.h>





#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define LIRC_DEV_PATH "/dev/lirc0"
#define PATT_LENGTH 1024
#define MAX_SINGLE_DURATION_US 50000  // 单个数据最大50ms
#define CLAMP_DURATION_US 40000       // 砍成40ms
#define MAX_TOTAL_DURATION_US 500000  // 总时长最大500ms

static const consumerir_freq_range_t consumerir_freqs[] = {
    {.min = 30000, .max = 30000},
    {.min = 33000, .max = 33000},
    {.min = 36000, .max = 36000},
    {.min = 38000, .max = 38000},
    {.min = 40000, .max = 40000},
    {.min = 56000, .max = 56000},
};

static int fd = -1;
static int consumerir_transmit(struct consumerir_device *dev,
   int carrier_freq, int pattern[], int pattern_len)
{
    int i;
    int ret;
    int print_len;
    unsigned int txbuf[PATT_LENGTH];
    int final_len = pattern_len;

    ALOGD("consumerir_transmit: pattern_len=%d, carrier_freq=%d", pattern_len, carrier_freq);

    if (pattern_len > PATT_LENGTH) {
        ALOGE("pattern_len %d exceeds max %d", pattern_len, PATT_LENGTH);
        return -EINVAL;
    }

    // 计算总时长
    unsigned int total_duration = 0;
    for (i = 0; i < pattern_len; i++) {
        total_duration += (unsigned int)pattern[i];
    }
    ALOGD("consumerir_transmit: total_duration=%u us (%d ms)", total_duration, total_duration / 1000);

    // 处理数据：超过50ms的砍成40ms
    for (i = 0; i < pattern_len; i++) {
        unsigned int val = (unsigned int)pattern[i];
        if (val > MAX_SINGLE_DURATION_US) {
            ALOGD("consumerir_transmit: clamping pattern[%d] from %u to %d us", i, val, CLAMP_DURATION_US);
            val = CLAMP_DURATION_US;
        }
        txbuf[i] = val;
    }

    fd = TEMP_FAILURE_RETRY(open(LIRC_DEV_PATH, O_RDWR));
    if (fd < 0) {
        ALOGE("open %s failed: %s", LIRC_DEV_PATH, strerror(errno));
        return -errno;
    }

    ret = ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier_freq);
    if (ret < 0) {
        ALOGE("LIRC_SET_SEND_CARRIER failed: %s", strerror(errno));
        close(fd);
        fd = -1;
        return -errno;
    }

    /*wangyanchen 打印 pattern 数组前几个元素用于调试 20260330*/
    print_len = pattern_len > 5 ? 5 : pattern_len;
    ALOGD("[wangyanchen] txbuf values (first %d): [0x%x, 0x%x, 0x%x, 0x%x, 0x%x]",
        print_len,
        print_len > 0 ? txbuf[0] : 0,
        print_len > 1 ? txbuf[1] : 0,
        print_len > 2 ? txbuf[2] : 0,
        print_len > 3 ? txbuf[3] : 0,
        print_len > 4 ? txbuf[4] : 0);

    int status = TEMP_FAILURE_RETRY(write(fd, txbuf, pattern_len * sizeof(unsigned int)));
    if (status == -1) {
        ALOGE("write failed: %s", strerror(errno));
        ret = -errno;
    } else if (status != (int)(pattern_len * sizeof(unsigned int))) {
        ALOGE("incomplete write: %d bytes", status);
        ret = -EAGAIN;
    } else {
        ALOGD("write success: %d bytes", status);
        ret = 0;
    }

    close(fd);
    fd = -1;
    return ret;
}

static int consumerir_get_num_carrier_freqs(struct consumerir_device *dev)
{
    ALOGD("consumerir_get_num_carrier_freqs");
    return ARRAY_SIZE(consumerir_freqs);
}

static int consumerir_get_carrier_freqs(struct consumerir_device *dev,
    size_t len, consumerir_freq_range_t *ranges)
{
    size_t to_copy = ARRAY_SIZE(consumerir_freqs);
    ALOGD("consumerir_get_carrier_freqs");
    to_copy = len < to_copy ? len : to_copy;
    memcpy(ranges, consumerir_freqs, to_copy * sizeof(consumerir_freq_range_t));
    return to_copy;
}

static int consumerir_close(hw_device_t *dev)
{
    free(dev);
   // close(fd);
    return 0;
}

/*
 * Generic device handling
 */
static int consumerir_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    if (strcmp(name, CONSUMERIR_TRANSMITTER) != 0) {
        return -EINVAL;
    }
    if (device == NULL) {
        ALOGE("NULL device on open");
        return -EINVAL;
    }

    consumerir_device_t *dev = malloc(sizeof(consumerir_device_t));
    memset(dev, 0, sizeof(consumerir_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = consumerir_close;

    dev->transmit = consumerir_transmit;
    dev->get_num_carrier_freqs = consumerir_get_num_carrier_freqs;
    dev->get_carrier_freqs = consumerir_get_carrier_freqs;

    *device = (hw_device_t*) dev;

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
        .name               = "Consumer IR Module",
        .author             = "The ETEK Project",
        .methods            = &consumerir_module_methods,
    },
};
