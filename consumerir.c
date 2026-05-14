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

#include <log/log.h>

#include <hardware/consumerir.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define CONSUMERIR_SYSFS_PATH "/sys/class/consumerir/ir"
#define CONSUMERIR_SEND_PATH CONSUMERIR_SYSFS_PATH "/send"
#define CONSUMERIR_FREQ_PATH CONSUMERIR_SYSFS_PATH "/frequency"

static const consumerir_freq_range_t consumerir_freqs[] = {
    {.min = 30000, .max = 30000},
    {.min = 33000, .max = 33000},
    {.min = 36000, .max = 36000},
    {.min = 38000, .max = 38000},
    {.min = 40000, .max = 40000},
    {.min = 56000, .max = 56000},
};

static int consumerir_write_int(const char *path, int value) {
    int fd;
    int result = -1;

    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        char buffer[32];
        int bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
        if (bytes > 0) {
            result = write(fd, buffer, bytes);
        }
        close(fd);
    }

    return result;
}

static int consumerir_write_string(const char *path, const char *value) {
    int fd;
    int result = -1;

    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        int bytes = strlen(value);
        if (write(fd, value, bytes) == bytes) {
            result = 0;
        }
        close(fd);
    }

    return result;
}

static int consumerir_write_pattern(const char *path, const int pattern[], int pattern_len) {
    int fd;
    int result = -1;
    int i;

    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        char buffer[8192];
        int pos = 0;
        
        for (i = 0; i < pattern_len; i++) {
            if (i > 0) {
                pos += snprintf(buffer + pos, sizeof(buffer) - pos, ",");
            }
            pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%d", pattern[i]);
            if (pos >= sizeof(buffer) - 10) {
                ALOGE("Pattern buffer overflow");
                result = -1;
                goto exit;
            }
        }
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "\n");
        
        if (write(fd, buffer, pos) == pos) {
            result = 0;
        }
        
exit:
        close(fd);
    }

    return result;
}

static int consumerir_transmit(struct consumerir_device *dev __unused,
   int carrier_freq, const int pattern[], int pattern_len)
{
    int i;
    int total_time = 0;

    ALOGD("transmit for %d Hz, %d slices", carrier_freq, pattern_len);

    for (i = 0; i < pattern_len; i++) {
        total_time += pattern[i];
    }

    if (consumerir_write_int(CONSUMERIR_FREQ_PATH, carrier_freq) < 0) {
        ALOGE("Failed to set frequency: %s", CONSUMERIR_FREQ_PATH);
        return -1;
    }

    if (consumerir_write_pattern(CONSUMERIR_SEND_PATH, pattern, pattern_len) < 0) {
        ALOGE("Failed to write pattern: %s", CONSUMERIR_SEND_PATH);
        return -1;
    }

    usleep(total_time);

    ALOGD("transmit completed, total time: %d us", total_time);

    return 0;
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
    ALOGI("Consumer IR device opened successfully with sysfs");
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
        .name               = "Sysfs IR HAL",
        .author             = "Custom IR HAL Implementation",
        .methods            = &consumerir_module_methods,
    },
};
