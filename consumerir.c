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
    {.min = 30000, .max = 30000},
    {.min = 33000, .max = 33000},
    {.min = 36000, .max = 36000},
    {.min = 38000, .max = 38000},
    {.min = 40000, .max = 40000},
    {.min = 56000, .max = 56000},
};

static int consumerir_transmit(struct consumerir_device *dev __unused,
   int carrier_freq, const int pattern[], int pattern_len)
{
    int fd = -1;
    int ret = 0;
    int i;
    int total_time = 0;

    ALOGD("transmit for %d Hz, %d slices", carrier_freq, pattern_len);

    fd = open(LIRC_DEVICE_PATH, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        ALOGE("Cannot open LIRC device: %s, error: %s", LIRC_DEVICE_PATH, strerror(errno));
        return -1;
    }

    unsigned long features = 0;
    if (ioctl(fd, LIRC_GET_FEATURES, &features) < 0) {
        ALOGE("LIRC_GET_FEATURES failed: %s", strerror(errno));
        ret = -1;
        goto exit;
    }

    if (features & LIRC_CAN_SET_SEND_CARRIER) {
        if (ioctl(fd, LIRC_SET_SEND_CARRIER, carrier_freq) < 0) {
            ALOGE("LIRC_SET_SEND_CARRIER failed: %s", strerror(errno));
            ret = -1;
            goto exit;
        }
    }

    if (features & LIRC_CAN_SET_SEND_DUTY_CYCLE) {
        int duty = 50;
        if (ioctl(fd, LIRC_SET_SEND_DUTY_CYCLE, duty) < 0) {
            ALOGW("LIRC_SET_SEND_DUTY_CYCLE failed: %s", strerror(errno));
        }
    }

    if (features & LIRC_CAN_SEND_RAW) {
        __u32 buffer[pattern_len + 1];
        buffer[0] = pattern_len * sizeof(__u32);

        for (i = 0; i < pattern_len; i++) {
            buffer[i + 1] = pattern[i];
            total_time += pattern[i];
        }

        ret = ioctl(fd, LIRC_SEND, buffer);
        if (ret < 0) {
            ALOGE("LIRC_SEND failed: %s", strerror(errno));
            ret = -1;
            goto exit;
        }
    } else {
        ALOGE("LIRC does not support RAW send");
        ret = -1;
        goto exit;
    }

    usleep(total_time);

    ALOGD("transmit completed, total time: %d us", total_time);
    ret = 0;

exit:
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
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = CONSUMERIR_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = CONSUMERIR_HARDWARE_MODULE_ID,
        .name               = "LIRC IR HAL",
        .author             = "Custom IR HAL Implementation",
        .methods            = &consumerir_module_methods,
    },
};
