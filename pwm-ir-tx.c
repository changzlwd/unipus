// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 Sean Young <sean@mess.org>
 */

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/rc-core.h>

#define DRIVER_NAME "pwm-ir-tx"
#define DEVICE_NAME "PWM IR Transmitter"
#define USE_HRTIMER_MODE false

struct pwm_ir_dev {
	struct mutex            lock;
	struct platform_device *pdev;
	struct rc_dev          *rdev;
	struct pwm_device      *pwm;
	u32                     carrier;
	u32                     duty_cycle;
};

static int pwm_ir_tx_config(struct pwm_ir_dev *dev, u32 carrier, u32 duty_cycle)
{
	int period_ns, duty_ns, rc;

	period_ns = NSEC_PER_SEC / carrier;
	duty_ns = period_ns * duty_cycle / 100;

	rc = pwm_config(dev->pwm, duty_ns, period_ns);
	if (rc == 0) {
		dev->carrier = carrier;
		dev->duty_cycle = duty_cycle;
	}

	return rc;
}

static int pwm_ir_tx_carrier(struct rc_dev *rdev, u32 carrier)
{
	struct pwm_ir_dev *dev = rdev->priv;
	int rc;

	mutex_lock(&dev->lock);
	rc = pwm_ir_tx_config(dev, carrier, dev->duty_cycle);
	mutex_unlock(&dev->lock);

	return rc;
}

static int pwm_ir_tx_duty_cycle(struct rc_dev *rdev, u32 duty_cycle)
{
	struct pwm_ir_dev *dev = rdev->priv;
	int rc;

	mutex_lock(&dev->lock);
	rc = pwm_ir_tx_config(dev, dev->carrier, duty_cycle);
	mutex_unlock(&dev->lock);

	return rc;
}

static int pwm_ir_tx_transmit(struct rc_dev *rdev, unsigned int *txbuf, unsigned int n)
{
	struct pwm_ir_dev *dev = rdev->priv;
	int i;

	mutex_lock(&dev->lock);

	for (i = 0; i < n; i++) {
		if (signal_pending(current))
			break;

		if (i & 0x01)
			pwm_disable(dev->pwm);
		else
			pwm_enable(dev->pwm);

		udelay(txbuf[i]);
	}

	pwm_disable(dev->pwm);
	mutex_unlock(&dev->lock);

	return i ? i : -ERESTARTSYS;
}

static int pwm_ir_probe(struct platform_device *pdev)
{
	struct pwm_ir_dev *dev;
	struct rc_dev *rcdev;
	int rc;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mutex_init(&dev->lock);
	dev->pdev = pdev;
	platform_set_drvdata(pdev, dev);

	dev->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(dev->pwm)) {
		pr_err("failed to get PWM device\n");
		return PTR_ERR(dev->pwm);
	}

	dev->carrier = 38000;
	dev->duty_cycle = 50;

	rc = pwm_ir_tx_config(dev, dev->carrier, dev->duty_cycle);
	if (rc != 0) {
		pr_err("failed to config PWM\n");
		return rc;
	}

	rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_IR_RAW_TX);
	if (!rcdev)
		return -ENOMEM;

	rcdev->dev.parent   = &pdev->dev;
	rcdev->driver_name  = DRIVER_NAME;
	rcdev->device_name  = DEVICE_NAME;
	rcdev->map_name     = RC_MAP_EMPTY;
	rcdev->priv        = dev;
	rcdev->tx_ir        = pwm_ir_tx_transmit;
	rcdev->s_tx_carrier = pwm_ir_tx_carrier;
	rcdev->s_tx_duty_cycle = pwm_ir_tx_duty_cycle;

	rc = devm_rc_register_device(&pdev->dev, rcdev);
	if (rc < 0) {
		pr_err("failed to register rc device\n");
		return rc;
	}

	dev->rdev = rcdev;

	pr_err("pwm-ir: probed successfully, using busy-wait mode\n");

	return 0;
}

static int pwm_ir_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id pwm_ir_of_match[] = {
	{ .compatible = "pwm-ir-tx", },
	{ },
};
MODULE_DEVICE_TABLE(of, pwm_ir_of_match);

static struct platform_driver pwm_ir_driver = {
	.probe  = pwm_ir_probe,
	.remove = pwm_ir_remove,
	.driver = {
		.name   = DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = pwm_ir_of_match,
	},
};
module_platform_driver(pwm_ir_driver);

MODULE_DESCRIPTION("PWM IR Transmitter");
MODULE_AUTHOR("Sean Young <sean@mess.org>");
MODULE_LICENSE("GPL");
