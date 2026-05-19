/*
 * PWM IR Transmitter Driver
 *
 * Copyright (C) 2024 Custom IR HAL Implementation
 *
 * This driver provides a sysfs interface for IR transmission using PWM.
 * It creates:
 *   /sys/class/consumerir/ir/frequency - set carrier frequency (Hz)
 *   /sys/class/consumerir/ir/send - send IR pattern (comma-separated microseconds)
 *
 * Based on Rockchip/Allwinner BSP implementations
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/hrtimer.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>

#define DRIVER_NAME "pwm-ir-tx"
#define DEFAULT_FREQUENCY 38000
#define DEFAULT_DUTY_CYCLE 50

struct pwm_ir_tx_data {
    struct pwm_device *pwm;
    struct device *dev;
    int frequency;
    int duty_cycle;
    struct hrtimer timer;
    int *pattern;
    int pattern_len;
    int pattern_idx;
    bool transmitting;
};

static struct class *consumerir_class;
static struct pwm_ir_tx_data *ir_data;

static enum hrtimer_restart ir_timer_callback(struct hrtimer *timer)
{
    struct pwm_ir_tx_data *data = container_of(timer, struct pwm_ir_tx_data, timer);

    if (data->pattern_idx >= data->pattern_len) {
        pwm_disable(data->pwm);
        data->transmitting = false;
        kfree(data->pattern);
        data->pattern = NULL;
        dev_info(data->dev, "IR transmission completed\n");
        return HRTIMER_NORESTART;
    }

    if (data->pattern_idx % 2 == 0) {
        pwm_enable(data->pwm);
    } else {
        pwm_disable(data->pwm);
    }

    hrtimer_forward_now(timer, ns_to_ktime(data->pattern[data->pattern_idx] * 1000));
    data->pattern_idx++;

    return HRTIMER_RESTART;
}

static ssize_t frequency_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", ir_data->frequency);
}

static ssize_t frequency_store(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count)
{
    unsigned long freq;
    int ret;

    ret = kstrtoul(buf, 10, &freq);
    if (ret)
        return ret;

    ir_data->frequency = freq;

    ret = pwm_config(ir_data->pwm, (1000000000UL / freq) * ir_data->duty_cycle / 100,
                     1000000000UL / freq);
    if (ret < 0)
        dev_err(dev, "Failed to configure PWM\n");
    else
        dev_info(dev, "Frequency set to %lu Hz\n", freq);

    return count;
}

static ssize_t send_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    int *pattern = NULL;
    int len = 0, i = 0;
    const char *ptr = buf;
    char *endptr;
    unsigned long val;

    if (ir_data->transmitting) {
        dev_err(dev, "IR transmitter busy\n");
        return -EBUSY;
    }

    while (*ptr) {
        while (*ptr == ' ' || *ptr == ',' || *ptr == '\n')
            ptr++;
        if (!*ptr)
            break;
        len++;
        while (*ptr && *ptr != ' ' && *ptr != ',' && *ptr != '\n')
            ptr++;
    }

    if (len == 0)
        return count;

    dev_info(dev, "Receiving IR pattern with %d elements\n", len);

    pattern = kzalloc(len * sizeof(int), GFP_KERNEL);
    if (!pattern)
        return -ENOMEM;

    ptr = buf;
    while (*ptr && i < len) {
        while (*ptr == ' ' || *ptr == ',' || *ptr == '\n')
            ptr++;
        if (!*ptr)
            break;

        val = simple_strtoul(ptr, &endptr, 10);
        pattern[i++] = val;
        ptr = endptr;
    }

    dev_info(dev, "IR pattern received, starting transmission\n");

    ir_data->pattern = pattern;
    ir_data->pattern_len = len;
    ir_data->pattern_idx = 0;
    ir_data->transmitting = true;

    hrtimer_start(&ir_data->timer, ns_to_ktime(0), HRTIMER_MODE_REL);

    return count;
}

static DEVICE_ATTR_RW(frequency);
static DEVICE_ATTR_WO(send);

static struct attribute *ir_attrs[] = {
    &dev_attr_frequency.attr,
    &dev_attr_send.attr,
    NULL,
};

static const struct attribute_group ir_attr_group = {
    .attrs = ir_attrs,
};

static int pwm_ir_tx_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct pwm_device *pwm;
    int ret;

    pwm = devm_pwm_get(dev, NULL);
    if (IS_ERR(pwm)) {
        dev_err(dev, "Failed to get PWM\n");
        return PTR_ERR(pwm);
    }

    ir_data = devm_kzalloc(dev, sizeof(*ir_data), GFP_KERNEL);
    if (!ir_data)
        return -ENOMEM;

    ir_data->pwm = pwm;
    ir_data->frequency = DEFAULT_FREQUENCY;
    ir_data->duty_cycle = DEFAULT_DUTY_CYCLE;
    ir_data->transmitting = false;

    hrtimer_init(&ir_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    ir_data->timer.function = ir_timer_callback;

    consumerir_class = class_create(THIS_MODULE, "consumerir");
    if (IS_ERR(consumerir_class)) {
        dev_err(dev, "Failed to create consumerir class\n");
        return PTR_ERR(consumerir_class);
    }

    ir_data->dev = device_create(consumerir_class, NULL, MKDEV(0, 0), NULL, "ir");
    if (IS_ERR(ir_data->dev)) {
        dev_err(dev, "Failed to create ir device\n");
        class_destroy(consumerir_class);
        return PTR_ERR(ir_data->dev);
    }

    ret = sysfs_create_group(&ir_data->dev->kobj, &ir_attr_group);
    if (ret) {
        dev_err(dev, "Failed to create sysfs group\n");
        device_destroy(consumerir_class, MKDEV(0, 0));
        class_destroy(consumerir_class);
        return ret;
    }

    ret = pwm_config(pwm, (1000000000UL / DEFAULT_FREQUENCY) * DEFAULT_DUTY_CYCLE / 100,
                     1000000000UL / DEFAULT_FREQUENCY);
    if (ret < 0) {
        dev_err(dev, "Failed to configure PWM\n");
        sysfs_remove_group(&ir_data->dev->kobj, &ir_attr_group);
        device_destroy(consumerir_class, MKDEV(0, 0));
        class_destroy(consumerir_class);
        return ret;
    }

    dev_info(dev, "PWM IR transmitter initialized, frequency: %d Hz\n", DEFAULT_FREQUENCY);

    return 0;
}

static int pwm_ir_tx_remove(struct platform_device *pdev)
{
    hrtimer_cancel(&ir_data->timer);
    pwm_disable(ir_data->pwm);
    sysfs_remove_group(&ir_data->dev->kobj, &ir_attr_group);
    device_destroy(consumerir_class, MKDEV(0, 0));
    class_destroy(consumerir_class);

    return 0;
}

static const struct of_device_id pwm_ir_tx_of_match[] = {
    { .compatible = "custom,pwm-ir-tx" },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, pwm_ir_tx_of_match);

static struct platform_driver pwm_ir_tx_driver = {
    .probe = pwm_ir_tx_probe,
    .remove = pwm_ir_tx_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = pwm_ir_tx_of_match,
    },
};

module_platform_driver(pwm_ir_tx_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PWM IR Transmitter Driver");
MODULE_AUTHOR("Custom IR HAL Implementation");
MODULE_ALIAS("platform:pwm-ir-tx");
