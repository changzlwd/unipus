/*
 * Copyright (C) 2017 Sean Young <sean@mess.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/workqueue.h>
#include <media/rc-core.h>

#define DRIVER_NAME	"pwm-ir-tx"
#define DEVICE_NAME	"PWM IR Transmitter"

struct pwm_ir {
	struct pwm_device *pwm;
	unsigned int carrier;
	unsigned int duty_cycle;
};

struct pwm_ir_tx_data {
	struct pwm_ir *pwm_ir;
	unsigned int *txbuf;
	unsigned int count;
};

static struct pm_qos_request pwm_ir_qos_req;

static const struct of_device_id pwm_ir_of_match[] = {
	{ .compatible = "pwm-ir-tx", },
	{ },
};
MODULE_DEVICE_TABLE(of, pwm_ir_of_match);

static int pwm_ir_set_duty_cycle(struct rc_dev *dev, u32 duty_cycle)
{
	struct pwm_ir *pwm_ir = dev->priv;

	pwm_ir->duty_cycle = duty_cycle;

	return 0;
}

static int pwm_ir_set_carrier(struct rc_dev *dev, u32 carrier)
{
	struct pwm_ir *pwm_ir = dev->priv;

	if (!carrier)
		return -EINVAL;

	pwm_ir->carrier = carrier;

	return 0;
}

static long pwm_ir_tx_work(void *arg)
{
	struct pwm_ir_tx_data *data = arg;
	struct pwm_ir *pwm_ir = data->pwm_ir;
	struct pwm_device *pwm = pwm_ir->pwm;
	struct pwm_state state;
	int i;
	u64 edge;

	pwm_init_state(pwm, &state);

	state.period = DIV_ROUND_CLOSEST(NSEC_PER_SEC, pwm_ir->carrier);
	pwm_set_relative_duty_cycle(&state, pwm_ir->duty_cycle, 100);

	state.enabled = false;
	pwm_apply_state(pwm, &state);

	edge = ktime_get_ns();

	for (i = 0; i < data->count; i++) {
		if (i & 0x01)
			pwm_disable(pwm);
		else
			pwm_enable(pwm);

		edge += data->txbuf[i] * NSEC_PER_USEC;
		while (ktime_get_ns() < edge)
			cpu_relax();
	}

	pwm_disable(pwm);

	return data->count;
}

static int pwm_ir_tx(struct rc_dev *dev, unsigned int *txbuf,
		     unsigned int count)
{
	struct pwm_ir *pwm_ir = dev->priv;
	struct pwm_ir_tx_data data = {
		.pwm_ir = pwm_ir,
		.txbuf = txbuf,
		.count = count,
	};
	long ret;

	pr_err("[wangyanchen] TX HIT\n");

	pm_qos_update_request(&pwm_ir_qos_req, 1);
	ret = work_on_cpu(0, pwm_ir_tx_work, &data);
	pm_qos_update_request(&pwm_ir_qos_req, PM_QOS_DEFAULT_VALUE);

	return ret;
}

static int pwm_ir_probe(struct platform_device *pdev)
{
	struct pwm_ir *pwm_ir;
	struct rc_dev *rcdev;
	int rc;
	pwm_ir = devm_kmalloc(&pdev->dev, sizeof(*pwm_ir), GFP_KERNEL);
	if (!pwm_ir)
		return -ENOMEM;

	pwm_ir->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(pwm_ir->pwm))
		return PTR_ERR(pwm_ir->pwm);

	pwm_ir->carrier = 38000;
	pwm_ir->duty_cycle = 50;

	rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_IR_RAW_TX);
	if (!rcdev)
		return -ENOMEM;

	rcdev->priv = pwm_ir;
	rcdev->driver_name = DRIVER_NAME;
	rcdev->device_name = DEVICE_NAME;
	rcdev->tx_ir = pwm_ir_tx;
	rcdev->s_tx_duty_cycle = pwm_ir_set_duty_cycle;
	rcdev->s_tx_carrier = pwm_ir_set_carrier;

	rc = devm_rc_register_device(&pdev->dev, rcdev);
	if (rc < 0)
		dev_err(&pdev->dev, "failed to register rc device\n");

	pr_err("rcdev->tx_ir = %p\n", rcdev->tx_ir);

	pm_qos_add_request(&pwm_ir_qos_req,
			   PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

	return rc;
}

static int pwm_ir_remove(struct platform_device *pdev)
{
	pm_qos_remove_request(&pwm_ir_qos_req);
	return 0;
}

static struct platform_driver pwm_ir_driver = {
	.probe = pwm_ir_probe,
	.remove = pwm_ir_remove,
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(pwm_ir_of_match),
	},
};
module_platform_driver(pwm_ir_driver);

MODULE_DESCRIPTION("PWM IR Transmitter");
MODULE_AUTHOR("Sean Young <sean@mess.org>");
MODULE_LICENSE("GPL");
