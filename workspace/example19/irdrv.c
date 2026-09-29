// SPDX-License-Identifier: GPL-2.0
/*
 * Loongson 2K500 teaching platform NEC infrared receiver.
 *
 * The demodulator output is active low.  Measuring the interval between
 * consecutive falling edges gives approximately 13.5 ms for a leader,
 * 11.25 ms for a repeat frame, 1.125 ms for bit 0 and 2.25 ms for bit 1.
 */

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/types.h>

#include "ir_keymap.h"

#define DRV_NAME             "loongson-ir-nec"
#define INPUT_NAME           "loongson-ir-nec"
#define IR_GPIO              121
#define GPIO_MUX_PHYS        0x1fe104ccUL
#define GPIO_MUX_MASK        0x77777707U

#define NEC_LEADER_MIN_US    13000
#define NEC_LEADER_MAX_US    14000
#define NEC_REPEAT_MIN_US    10500
#define NEC_REPEAT_MAX_US    12500
#define NEC_BIT0_MIN_US      850
#define NEC_BIT0_MAX_US      1450
#define NEC_BIT1_MIN_US      1850
#define NEC_BIT1_MAX_US      2650
#define NEC_GAP_US           20000

struct ir_key_entry {
	u8 command;
	unsigned int keycode;
	const char *name;
};

#define MAKE_KEY_ENTRY(command, keycode, name) { command, keycode, name },
static const struct ir_key_entry ir_keys[] = {
	IR_KEYMAP(MAKE_KEY_ENTRY)
};
#undef MAKE_KEY_ENTRY

struct loongson_ir {
	int irq;
	struct input_dev *input;
	void __iomem *mux_reg;
	u64 last_edge_ns;
	u32 frame;
	u8 bit_count;
	bool receiving;
	bool last_key_valid;
	unsigned int last_keycode;
};

static struct loongson_ir irdev;
static bool configure_mux = true;
static bool debug;

module_param(configure_mux, bool, 0444);
MODULE_PARM_DESC(configure_mux,
	"configure the 2K500 GPIO121 mux register (default: true)");
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "print decoded and unknown NEC frames");

static bool in_range(u32 value, u32 min, u32 max)
{
	return value >= min && value <= max;
}

static const struct ir_key_entry *find_command(u8 command)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(ir_keys); i++)
		if (ir_keys[i].command == command)
			return &ir_keys[i];

	return NULL;
}

static void report_key(struct loongson_ir *dev, unsigned int keycode)
{
	input_report_key(dev->input, keycode, 1);
	input_sync(dev->input);
	input_report_key(dev->input, keycode, 0);
	input_sync(dev->input);
}

static void finish_frame(struct loongson_ir *dev)
{
	u8 address = dev->frame;
	u8 address_inv = dev->frame >> 8;
	u8 command = dev->frame >> 16;
	u8 command_inv = dev->frame >> 24;
	const struct ir_key_entry *key;

	dev->receiving = false;
	dev->bit_count = 0;

	if ((u8)(address ^ address_inv) != 0xff ||
	    (u8)(command ^ command_inv) != 0xff) {
		if (debug)
			pr_info(DRV_NAME ": checksum failed: %08x\n", dev->frame);
		return;
	}

	key = find_command(command);
	if (!key) {
		if (debug)
			pr_info(DRV_NAME ": unknown command 0x%02x, frame=%08x\n",
				command, dev->frame);
		return;
	}

	dev->last_keycode = key->keycode;
	dev->last_key_valid = true;
	if (debug)
		pr_info(DRV_NAME ": command=0x%02x key=%s\n",
			command, key->name);
	report_key(dev, key->keycode);
}

static void decode_interval(struct loongson_ir *dev, u32 delta_us)
{
	if (in_range(delta_us, NEC_LEADER_MIN_US, NEC_LEADER_MAX_US)) {
		dev->frame = 0;
		dev->bit_count = 0;
		dev->receiving = true;
		return;
	}

	if (in_range(delta_us, NEC_REPEAT_MIN_US, NEC_REPEAT_MAX_US)) {
		dev->receiving = false;
		dev->bit_count = 0;
		if (dev->last_key_valid)
			report_key(dev, dev->last_keycode);
		return;
	}

	if (!dev->receiving)
		return;

	if (in_range(delta_us, NEC_BIT1_MIN_US, NEC_BIT1_MAX_US)) {
		dev->frame |= BIT(dev->bit_count);
	} else if (!in_range(delta_us, NEC_BIT0_MIN_US, NEC_BIT0_MAX_US)) {
		dev->receiving = false;
		dev->bit_count = 0;
		return;
	}

	dev->bit_count++;
	if (dev->bit_count == 32)
		finish_frame(dev);
}

static irqreturn_t ir_irq_handler(int irq, void *data)
{
	struct loongson_ir *dev = data;
	u64 now_ns = ktime_get_ns();
	u64 delta_ns;
	u32 delta_us;

	if (!dev->last_edge_ns) {
		dev->last_edge_ns = now_ns;
		return IRQ_HANDLED;
	}

	delta_ns = now_ns - dev->last_edge_ns;
	dev->last_edge_ns = now_ns;
	delta_us = (u32)(delta_ns / NSEC_PER_USEC);

	if (delta_us > NEC_GAP_US) {
		dev->receiving = false;
		dev->bit_count = 0;
		return IRQ_HANDLED;
	}

	decode_interval(dev, delta_us);
	return IRQ_HANDLED;
}

static int configure_gpio_mux(struct loongson_ir *dev)
{
	u32 value;

	if (!configure_mux)
		return 0;

	dev->mux_reg = ioremap(GPIO_MUX_PHYS, sizeof(u32));
	if (!dev->mux_reg)
		return -ENOMEM;

	value = ioread32(dev->mux_reg);
	iowrite32(value & GPIO_MUX_MASK, dev->mux_reg);
	return 0;
}

static int __init loongson_ir_init(void)
{
	struct input_dev *input;
	size_t i;
	int ret;

	ret = configure_gpio_mux(&irdev);
	if (ret)
		return ret;

	ret = gpio_request(IR_GPIO, DRV_NAME);
	if (ret)
		goto err_unmap;

	ret = gpio_direction_input(IR_GPIO);
	if (ret)
		goto err_gpio;

	input = input_allocate_device();
	if (!input) {
		ret = -ENOMEM;
		goto err_gpio;
	}

	irdev.input = input;
	input->name = INPUT_NAME;
	input->phys = "gpio121/input0";
	input->id.bustype = BUS_HOST;
	for (i = 0; i < ARRAY_SIZE(ir_keys); i++)
		input_set_capability(input, EV_KEY, ir_keys[i].keycode);

	ret = input_register_device(input);
	if (ret)
		goto err_free_input;

	irdev.irq = gpio_to_irq(IR_GPIO);
	if (irdev.irq < 0) {
		ret = irdev.irq;
		goto err_unregister_input;
	}

	ret = request_irq(irdev.irq, ir_irq_handler, IRQF_TRIGGER_FALLING,
			  DRV_NAME, &irdev);
	if (ret)
		goto err_unregister_input;

	pr_info(DRV_NAME ": GPIO%d, IRQ%d, input device registered\n",
		IR_GPIO, irdev.irq);
	return 0;

err_unregister_input:
	input_unregister_device(irdev.input);
	irdev.input = NULL;
	goto err_gpio;
err_free_input:
	input_free_device(input);
	irdev.input = NULL;
err_gpio:
	gpio_free(IR_GPIO);
err_unmap:
	if (irdev.mux_reg)
		iounmap(irdev.mux_reg);
	return ret;
}

static void __exit loongson_ir_exit(void)
{
	free_irq(irdev.irq, &irdev);
	if (irdev.input)
		input_unregister_device(irdev.input);
	gpio_free(IR_GPIO);
	if (irdev.mux_reg)
		iounmap(irdev.mux_reg);
	pr_info(DRV_NAME ": unloaded\n");
}

module_init(loongson_ir_init);
module_exit(loongson_ir_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI, adapted from the Loongson 2K500 textbook experiment");
MODULE_DESCRIPTION("NEC IR receiver for the Loongson 2K500 teaching platform");
