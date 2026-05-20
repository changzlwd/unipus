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
#include <media/rc-core.h>

#define DRIVER_NAME	"pwm-ir-tx"
#define DEVICE_NAME	"PWM IR Transmitter"

struct pwm_ir {
	struct pwm_device *pwm;
	unsigned int carrier;
	unsigned int duty_cycle;
};

static const struct of_device_id pwm_ir_of_match[] = {
	{ .compatible = "pwm-ir-tx", },
	{ },
};
MODULE_DEVICE_TABLE(of, pwm_ir_of_match);

static int pwm_ir_set_duty_cycle(struct rc_dev *dev, u32 duty_cycle)
{
	struct pwm_ir *pwm_ir = dev->priv;

	if (duty_cycle == 0 || duty_cycle >= 100)
		return -EINVAL;

	pwm_ir->duty_cycle = duty_cycle;

	return 0;
}

static int pwm_ir_set_carrier(struct rc_dev *dev, u32 carrier)
{
	struct pwm_ir *pwm_ir = dev->priv;

	if (carrier < 30000 || carrier > 60000)
		return -EINVAL;

	pwm_ir->carrier = carrier;

	return 0;
}

static int pwm_ir_tx(struct rc_dev *dev, unsigned int *txbuf,
		     unsigned int count)
{
	struct pwm_ir *pwm_ir = dev->priv;
	struct pwm_device *pwm = pwm_ir->pwm;
	struct pwm_state state;
	int i;
	ktime_t start, now, target;
	unsigned int period;
	long delta;

	// 预计算 PWM 周期，使用更精确的计算
	period = NSEC_PER_SEC / pwm_ir->carrier;
	// 如果不是整数，调整频率使周期为整数
	if (NSEC_PER_SEC % pwm_ir->carrier != 0) {
		period = DIV_ROUND_CLOSEST(NSEC_PER_SEC, pwm_ir->carrier);
		dev_info(&dev->dev, "adjusted carrier: %u Hz (period: %u ns)", 
			(unsigned int)(NSEC_PER_SEC / period), period);
	}

	pwm_init_state(pwm, &state);
	state.period = period;
	pwm_set_relative_duty_cycle(&state, pwm_ir->duty_cycle, 100);
	state.enabled = false;
	
	// 先设置 PWM 关闭状态，预热
	pwm_apply_state(pwm, &state);
	udelay(10);
	
	start = ktime_get();

	for (i = 0; i < count; i++) {
		// 参数检查
		if (txbuf[i] < 100 || txbuf[i] > 50000) {
			dev_err(&dev->dev, "invalid pulse duration: %u us", txbuf[i]);
			goto error;
		}
		
		target = ktime_add_us(start, txbuf[i]);
		
		// 设置 PWM 状态前加微小延迟，减少抖动
		udelay(1);
		
		state.enabled = !(i % 2);
		pwm_apply_state(pwm, &state);
		
		// 精确忙等待
		now = ktime_get();
		while (ktime_compare(now, target) < 0) {
			// 先尝试长一点的延迟，减少 CPU 占用
			delta = ktime_us_delta(target, now);
			if (delta > 10) {
				udelay(delta / 2);
			} else {
				ndelay(100);
			}
			now = ktime_get();
		}
		
		start = target;
	}

error:
	// 确保最后关闭 PWM
	state.enabled = false;
	pwm_apply_state(pwm, &state);
	udelay(10);

	return (i == count) ? count : i;
}

static int pwm_ir_probe(struct platform_device *pdev)
{
	struct pwm_ir *pwm_ir;
	struct rc_dev *rcdev;
	int rc;

	pwm_ir = devm_kzalloc(&pdev->dev, sizeof(*pwm_ir), GFP_KERNEL);
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
