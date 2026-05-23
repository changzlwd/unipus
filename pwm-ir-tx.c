// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 Sean Young <sean@mess.org>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <media/rc-core.h>
// liuqizhi 20260521 optimize infrared transmission
#define DRIVER_NAME	"pwm-ir-tx"
#define DEVICE_NAME	"PWM IR Transmitter"

struct pwm_ir {
	struct pwm_device *pwm;
	struct hrtimer timer;
	struct completion tx_done;
	unsigned int period;
	u32 carrier;
	u32 duty_cycle;
	const unsigned int *txbuf;
	unsigned int txbuf_len;
	unsigned int txbuf_index;
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
	pr_err("[pwm-ir-tx] set duty cycle: %u%%\n", duty_cycle);

	return 0;
}

static int pwm_ir_set_carrier(struct rc_dev *dev, u32 carrier)
{
	struct pwm_ir *pwm_ir = dev->priv;

	if (!carrier)
		return -EINVAL;

	pwm_ir->carrier = carrier;
	pwm_ir->period = DIV_ROUND_CLOSEST(NSEC_PER_SEC, carrier);
	pr_err("[pwm-ir-tx] set carrier: %u Hz, period: %u ns\n", carrier, pwm_ir->period);

	return 0;
}

static enum hrtimer_restart pwm_ir_timer(struct hrtimer *timer)
{
	struct pwm_ir *pwm_ir = container_of(timer, struct pwm_ir, timer);
	struct pwm_device *pwm = pwm_ir->pwm;
	u64 ns;

	if (pwm_ir->txbuf_index % 2 == 0)
		pwm_enable(pwm);
	else
		pwm_disable(pwm);

	if (pwm_ir->txbuf_index >= pwm_ir->txbuf_len) {
		pwm_disable(pwm);
		complete(&pwm_ir->tx_done);
		pr_err("[pwm-ir-tx] tx completed, sent %u samples\n", pwm_ir->txbuf_len);
		return HRTIMER_NORESTART;
	}

	ns = (u64)pwm_ir->txbuf[pwm_ir->txbuf_index] * NSEC_PER_USEC;
	hrtimer_add_expires_ns(timer, ns);

	pwm_ir->txbuf_index++;

	return HRTIMER_RESTART;
}

static long pwm_ir_tx_work(void *arg)
{
	struct pwm_ir *pwm_ir = arg;
	struct pwm_device *pwm = pwm_ir->pwm;
	int ret;

	pwm_config(pwm, pwm_ir->period * pwm_ir->duty_cycle / 100, pwm_ir->period);

	hrtimer_start(&pwm_ir->timer, 0, HRTIMER_MODE_REL);

	ret = wait_for_completion_timeout(&pwm_ir->tx_done,
					 msecs_to_jiffies(1000));
	if (!ret)
		ret = -ETIMEDOUT;
	else
		ret = pwm_ir->txbuf_len;

	return ret;
}

static int pwm_ir_tx(struct rc_dev *dev, unsigned int *txbuf,
		     unsigned int count)
{
	struct pwm_ir *pwm_ir = dev->priv;
	long ret;
	cpu_set_t cpuset;
	struct sched_param param;

	pwm_ir->txbuf = txbuf;
	pwm_ir->txbuf_len = count;
	pwm_ir->txbuf_index = 0;

	pr_err("[pwm-ir-tx] TX HIT, count=%u\n", count);

	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	param.sched_priority = 99;
	sched_setscheduler(0, SCHED_FIFO, &param);

	ret = pwm_ir_tx_work(pwm_ir);

	param.sched_priority = 0;
	sched_setscheduler(0, SCHED_NORMAL, &param);

	return ret;
}

static int pwm_ir_probe(struct platform_device *pdev)
{
	struct pwm_ir *pwm_ir;
	struct rc_dev *rcdev;
	int rc;

	pr_err("[pwm-ir-tx] probing...\n");

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

	init_completion(&pwm_ir->tx_done);
	hrtimer_init(&pwm_ir->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pwm_ir->timer.function = pwm_ir_timer;

	rcdev->tx_ir = pwm_ir_tx;
	rcdev->priv = pwm_ir;
	rcdev->driver_name = DRIVER_NAME;
	rcdev->device_name = DEVICE_NAME;
	rcdev->s_tx_duty_cycle = pwm_ir_set_duty_cycle;
	rcdev->s_tx_carrier = pwm_ir_set_carrier;

	rc = devm_rc_register_device(&pdev->dev, rcdev);
	if (rc < 0)
		dev_err(&pdev->dev, "failed to register rc device\n");
	else
		pr_err("[pwm-ir-tx] rc device registered, tx_ir=%p\n", rcdev->tx_ir);

	pr_err("[pwm-ir-tx] probe completed\n");

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
