/*
 * Copyright (C) 2013 by Xiang Xiao <xiaoxiang@xiaomi.com>
 * Copyright (C) 2017 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/rc-core.h>

#define DRIVER_NAME "pwm-ir-tx"
#define DEVICE_NAME "PWM IR Transmitter"

struct pwm_ir_dev {
	struct mutex            lock;
	struct platform_device *pdev;
	struct rc_dev          *rdev;
	struct pwm_device      *pwm;
	u32                     carrier;
	u32                     duty_cycle;
};

struct pwm_ir_packet {
	struct completion  done;
	struct pwm_device *pwm;
	unsigned int      *buffer;
	unsigned int       length;
	unsigned int       next;
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

static long pwm_ir_tx_work(void *arg)
{
	struct pwm_ir_packet *pkt = arg;
	unsigned long flags;
	int i;

	init_completion(&pkt->done);

	local_irq_save(flags);

	for (i = 0; i < pkt->length; i++) {
		if (signal_pending(current))
			break;

		if (i & 0x01)
			pwm_disable(pkt->pwm);
		else
			pwm_enable(pkt->pwm);

		ndelay(pkt->buffer[i] % 1000);
		udelay(pkt->buffer[i] / 1000);
	}

	pwm_disable(pkt->pwm);
	local_irq_restore(flags);

	complete(&pkt->done);

	return i ? i : -ERESTARTSYS;
}

static int pwm_ir_tx_transmit(struct rc_dev *rdev, unsigned int *txbuf, unsigned int n)
{
	struct pwm_ir_dev *dev = rdev->priv;
	struct pwm_ir_packet pkt = {};
	int cpu, rc = -ENODEV;

	mutex_lock(&dev->lock);

	pkt.pwm    = dev->pwm;
	pkt.buffer = txbuf;
	pkt.length = n;

	for_each_online_cpu(cpu) {
		if (cpu != 0) {
			rc = work_on_cpu(cpu, pwm_ir_tx_work, &pkt);
			break;
		}
	}

	if (rc == -ENODEV) {
		pr_warn("pwm-ir: can't run on auxilliary cpu, trying CPU 0\n");
		rc = work_on_cpu(0, pwm_ir_tx_work, &pkt);
	}

	mutex_unlock(&dev->lock);

	return rc;
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

	dev->pwm = devm_of_pwm_get(&pdev->dev, pdev->dev.of_node, NULL);
	if (IS_ERR(dev->pwm)) {
		dev_err(&pdev->dev, "failed to get PWM device\n");
		return PTR_ERR(dev->pwm);
	}

	dev->carrier = 38000;
	dev->duty_cycle = 50;

	rc = pwm_ir_tx_config(dev, dev->carrier, dev->duty_cycle);
	if (rc != 0) {
		dev_err(&pdev->dev, "failed to config PWM\n");
		return rc;
	}

	rcdev = rc_allocate_device();
	if (!rcdev)
		return -ENOMEM;

	rcdev->parent       = &pdev->dev;
	rcdev->driver_name  = DRIVER_NAME;
	rcdev->device_name  = DEVICE_NAME;
	rcdev->map_name     = RC_MAP_LIRC;
	rcdev->driver_type  = RC_DRIVER_IR_RAW;
	rcdev->priv        = dev;
	rcdev->tx_ir        = pwm_ir_tx_transmit;
	rcdev->s_tx_carrier = pwm_ir_tx_carrier;
	rcdev->s_tx_duty_cycle = pwm_ir_tx_duty_cycle;

	rc = rc_register_device(rcdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to register rc device\n");
		return rc;
	}

	dev->rdev = rcdev;

	return 0;
}

static int pwm_ir_remove(struct platform_device *pdev)
{
	struct pwm_ir_dev *dev = platform_get_drvdata(pdev);

	rc_unregister_device(dev->rdev);

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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiang Xiao <xiaoxiang@xiaomi.com>");
MODULE_DESCRIPTION("PWM IR driver");
