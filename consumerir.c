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
#include <sys/stat.h>
#include <stdio.h>

#include <log/log.h>

#include <hardware/consumerir.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define SYSFS_SEND_PATH "/sys/class/consumerir/ir/send"
#define SYSFS_FREQ_PATH "/sys/class/consumerir/ir/frequency"

static const consumerir_freq_range_t consumerir_freqs[] = {
    {.min = 30000, .max = 60000},
};

static int consumerir_transmit(struct consumerir_device *dev __unused,
   int carrier_freq, const int pattern[], int pattern_len)
{
    int fd = -1;
    int ret = 0;
    int i;
    FILE *fp = NULL;
    char *buffer = NULL;
    size_t buffer_size = 0;

    ALOGE("consumerir_transmit: called for %d Hz, %d slices", carrier_freq, pattern_len);

    ALOGE("IR TX: carrier=%d Hz, count=%d", carrier_freq, pattern_len);
    ALOGE("IR TX: data dump:");
    for (i = 0; i < pattern_len; i++) {
        if (i % 8 == 0) {
            if (i > 0)
                ALOGE("");
            ALOGE("  [%04d-%04d]:", i, (i + 7 < pattern_len) ? i + 7 : pattern_len - 1);
        }
        ALOGE(" %d", pattern[i]);
    }
    ALOGE("");

    // Check if sysfs path exists
    struct stat st;
    if (stat(SYSFS_FREQ_PATH, &st) != 0) {
        ALOGE("Sysfs path does not exist: %s", SYSFS_FREQ_PATH);
        return -1;
    }

    // Set frequency
    fp = fopen(SYSFS_FREQ_PATH, "w");
    if (fp == NULL) {
        ALOGE("Cannot open frequency sysfs: %s", SYSFS_FREQ_PATH);
        return -1;
    }
    fprintf(fp, "%d", carrier_freq);
    fclose(fp);
    ALOGE("Frequency set to %d Hz", carrier_freq);

    // Build pattern string (comma-separated)
    buffer_size = pattern_len * 12;  // max 10 digits + comma + safety margin
    buffer = malloc(buffer_size);
    if (!buffer) {
        ALOGE("Failed to allocate buffer");
        return -ENOMEM;
    }

    memset(buffer, 0, buffer_size);
    for (i = 0; i < pattern_len; i++) {
        if (i > 0) {
            strcat(buffer, ",");
        }
        char num[16];
        snprintf(num, sizeof(num), "%d", pattern[i]);
        strcat(buffer, num);
    }

    ALOGE("Sending pattern via sysfs, pattern length: %zu bytes", strlen(buffer));

    // Send pattern
    fp = fopen(SYSFS_SEND_PATH, "w");
    if (fp == NULL) {
        ALOGE("Cannot open send sysfs: %s", SYSFS_SEND_PATH);
        free(buffer);
        return -1;
    }

    size_t written = fwrite(buffer, 1, strlen(buffer), fp);
    fclose(fp);
    free(buffer);

    if (written != strlen(buffer)) {
        ALOGE("Failed to write pattern to sysfs");
        return -1;
    }

    ALOGE("Pattern sent successfully via sysfs");
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
    ALOGI("Consumer IR device opened successfully via sysfs");
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
        .name               = "Sysfs IR HAL",
        .author             = "Custom IR HAL Implementation",
        .methods            = &consumerir_module_methods,
    },
};
