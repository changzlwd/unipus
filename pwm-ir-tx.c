// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 Sean Young <sean@mess.org>
 * Optimized for Android IR transmission
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <media/rc-core.h>

#define DRIVER_NAME	"pwm-ir-tx"
#define DEVICE_NAME	"PWM IR Transmitter"

struct pwm_ir {
	struct pwm_device *pwm;
	unsigned int period;
	u32 carrier;
	u32 duty_cycle;
};

struct pwm_ir_tx_data {
	struct pwm_ir *pwm_ir;
	unsigned int *txbuf;
	unsigned int count;
};

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
	pwm_ir->period = DIV_ROUND_CLOSEST(NSEC_PER_SEC, carrier);
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

		edge += (u64)data->txbuf[i] * NSEC_PER_USEC;
		while (ktime_get_ns() < edge)
			cpu_relax();
	}

	pwm_disable(pwm);

	return data->count;
}

static int pwm_ir_tx(struct rc_dev *dev, unsigned int *txbuf, unsigned int count)
{
	struct pwm_ir *pwm_ir = dev->priv;
	struct pwm_ir_tx_data data = {
		.pwm_ir = pwm_ir,
		.txbuf = txbuf,
		.count = count,
	};
	long ret;
	cpu_set_t cpuset;
	struct sched_param param;

	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	param.sched_priority = 99;
	sched_setscheduler(0, SCHED_FIFO, &param);

	ret = work_on_cpu(0, pwm_ir_tx_work, &data);

	param.sched_priority = 0;
	sched_setscheduler(0, SCHED_NORMAL, &param);

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
	pwm_ir->period = DIV_ROUND_CLOSEST(NSEC_PER_SEC, pwm_ir->carrier);

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

	return rc;
}

static struct platform_driver pwm_ir_driver = {
	.probe = pwm_ir_probe,
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(pwm_ir_of_match),
	},
};
module_platform_driver(pwm_ir_driver);

MODULE_DESCRIPTION("PWM IR Transmitter");
MODULE_AUTHOR("Sean Young <sean@mess.org>");
MODULE_LICENSE("GPL");
