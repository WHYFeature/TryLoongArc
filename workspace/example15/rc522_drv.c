// SPDX-License-Identifier: GPL-2.0

#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "rc522_core.h"

#define DEVICE_NAME       "rfid_dev"
#define GPIO_MUX1_PHYS    0x1fe104ccUL
#define GPIO_MUX2_PHYS    0x1fe104d0UL
#define GPIO_MUX1_MASK    0x00077777U
#define GPIO_MUX2_MASK    0x77777700U

struct rc522_file_ctx {
	u8 block;
	u8 key[RC522_KEY_SIZE];
};

static DEFINE_MUTEX(reader_lock);
static void __iomem *mux1;
static void __iomem *mux2;
static u32 mux1_saved;
static u32 mux2_saved;
static bool configure_mux = true;
static bool strict_probe = true;
bool rc522_debug_bus;
bool rc522_swap_data;
static bool gpio_selftest;

module_param(configure_mux, bool, 0444);
MODULE_PARM_DESC(configure_mux,
	"configure the 2K500 GPIO125-129 mux registers (default: true)");
module_param(strict_probe, bool, 0444);
MODULE_PARM_DESC(strict_probe,
	"fail module loading when VersionReg is 0x00/0xff (default: true)");
module_param_named(debug_bus, rc522_debug_bus, bool, 0444);
MODULE_PARM_DESC(debug_bus, "print pinmux, GPIO and raw register diagnostics");
module_param_named(swap_data, rc522_swap_data, bool, 0444);
MODULE_PARM_DESC(swap_data,
	"swap the documented GPIO126 MISO and GPIO127 MOSI assignments");
module_param(gpio_selftest, bool, 0444);
MODULE_PARM_DESC(gpio_selftest,
	"drive the selected MISO GPIO low/high once before RC522 initialization");

static struct gpio rc522_gpios[] = {
	{ RC522_GPIO_CLK,  GPIOF_OUT_INIT_LOW,  "rc522-clk" },
	{ RC522_GPIO_MISO, GPIOF_IN,            "rc522-miso" },
	{ RC522_GPIO_MOSI, GPIOF_OUT_INIT_LOW,  "rc522-mosi" },
	{ RC522_GPIO_CS,   GPIOF_OUT_INIT_HIGH, "rc522-cs" },
	{ RC522_GPIO_RST,  GPIOF_OUT_INIT_HIGH, "rc522-rst" },
};

static int rc522_open(struct inode *inode, struct file *file)
{
	struct rc522_file_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	ctx->block = 1;
	memset(ctx->key, 0xff, sizeof(ctx->key));
	file->private_data = ctx;
	return nonseekable_open(inode, file);
}

static int rc522_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static ssize_t rc522_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *position)
{
	struct rc522_file_ctx *ctx = file->private_data;
	u8 data[RC522_BLOCK_SIZE];
	int ret;

	if (count < sizeof(data))
		return -EINVAL;
	if (mutex_lock_interruptible(&reader_lock))
		return -ERESTARTSYS;
	ret = rc522_read_block(ctx->block, ctx->key, data);
	mutex_unlock(&reader_lock);
	if (ret)
		return ret;
	if (copy_to_user(buffer, data, sizeof(data)))
		return -EFAULT;
	return sizeof(data);
}

static ssize_t rc522_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *position)
{
	struct rc522_file_ctx *ctx = file->private_data;
	u8 data[RC522_BLOCK_SIZE];
	int ret;

	if (count != sizeof(data))
		return -EINVAL;
	if (ctx->block == 0 || ctx->block >= 64 || ctx->block % 4 == 3)
		return -EPERM;
	if (copy_from_user(data, buffer, sizeof(data)))
		return -EFAULT;
	if (mutex_lock_interruptible(&reader_lock))
		return -ERESTARTSYS;
	ret = rc522_write_block(ctx->block, ctx->key, data);
	mutex_unlock(&reader_lock);
	return ret ? ret : sizeof(data);
}

static long rc522_ioctl(struct file *file, unsigned int command,
			unsigned long argument)
{
	struct rc522_file_ctx *ctx = file->private_data;
	struct rc522_uid uid;
	struct rc522_key key;
	u8 block;
	u8 version;
	int ret = 0;

	if (_IOC_TYPE(command) != RC522_IOC_MAGIC)
		return -ENOTTY;

	switch (command) {
	case RC522_IOC_SET_BLOCK:
		if (copy_from_user(&block, (void __user *)argument, sizeof(block)))
			return -EFAULT;
		if (block >= 64)
			return -EINVAL;
		ctx->block = block;
		break;
	case RC522_IOC_SET_KEY_A:
		if (copy_from_user(&key, (void __user *)argument, sizeof(key)))
			return -EFAULT;
		memcpy(ctx->key, key.bytes, sizeof(ctx->key));
		break;
	case RC522_IOC_GET_UID:
		if (mutex_lock_interruptible(&reader_lock))
			return -ERESTARTSYS;
		ret = rc522_get_uid(uid.bytes);
		mutex_unlock(&reader_lock);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)argument, &uid, sizeof(uid)))
			return -EFAULT;
		break;
	case RC522_IOC_GET_VERSION:
		if (mutex_lock_interruptible(&reader_lock))
			return -ERESTARTSYS;
		version = rc522_chip_version();
		mutex_unlock(&reader_lock);
		if (copy_to_user((void __user *)argument, &version, sizeof(version)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations rc522_fops = {
	.owner = THIS_MODULE,
	.open = rc522_open,
	.release = rc522_release,
	.read = rc522_read,
	.write = rc522_write,
	.unlocked_ioctl = rc522_ioctl,
	.llseek = no_llseek,
};

static struct miscdevice rc522_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &rc522_fops,
	.mode = 0600,
};

static int configure_pinmux(void)
{
	if (!configure_mux)
		return 0;

	mux1 = ioremap(GPIO_MUX1_PHYS, sizeof(u32));
	if (!mux1)
		return -ENOMEM;
	mux2 = ioremap(GPIO_MUX2_PHYS, sizeof(u32));
	if (!mux2) {
		iounmap(mux1);
		mux1 = NULL;
		return -ENOMEM;
	}

	mux1_saved = ioread32(mux1);
	mux2_saved = ioread32(mux2);
	iowrite32(mux1_saved & GPIO_MUX1_MASK, mux1);
	iowrite32(mux2_saved & GPIO_MUX2_MASK, mux2);
	if (rc522_debug_bus)
		pr_info("rc522: pinmux before %08x/%08x, after %08x/%08x\n",
			mux1_saved, mux2_saved, ioread32(mux1), ioread32(mux2));
	return 0;
}

static void restore_pinmux(void)
{
	if (mux1) {
		iowrite32(mux1_saved, mux1);
		iounmap(mux1);
		mux1 = NULL;
	}
	if (mux2) {
		iowrite32(mux2_saved, mux2);
		iounmap(mux2);
		mux2 = NULL;
	}
}

static int __init rc522_module_init(void)
{
	u8 version;
	int ret;

	ret = configure_pinmux();
	if (ret)
		return ret;
	if (rc522_swap_data) {
		rc522_gpios[1].flags = GPIOF_OUT_INIT_LOW;
		rc522_gpios[2].flags = GPIOF_IN;
		pr_warn("rc522: diagnostic MISO/MOSI swap enabled\n");
	}
	ret = gpio_request_array(rc522_gpios, ARRAY_SIZE(rc522_gpios));
	if (ret)
		goto err_mux;
	if (gpio_selftest)
		rc522_gpio_selftest();
	ret = rc522_chip_init();
	if (ret)
		goto err_gpio;
	rc522_bus_diagnose();
	version = rc522_chip_version();
	if (version == 0x00 || version == 0xff) {
		pr_err("rc522: invalid VersionReg 0x%02x; check wiring and pinmux\n",
		       version);
		if (strict_probe) {
			ret = -ENODEV;
			goto err_gpio;
		}
		pr_warn("rc522: continuing because strict_probe=0 (diagnostic mode)\n");
	}
	ret = misc_register(&rc522_miscdev);
	if (ret)
		goto err_gpio;

	pr_info("rc522: registered /dev/%s, VersionReg=0x%02x\n",
		DEVICE_NAME, version);
	return 0;

err_gpio:
	gpio_free_array(rc522_gpios, ARRAY_SIZE(rc522_gpios));
err_mux:
	restore_pinmux();
	return ret;
}

static void __exit rc522_module_exit(void)
{
	misc_deregister(&rc522_miscdev);
	gpio_free_array(rc522_gpios, ARRAY_SIZE(rc522_gpios));
	restore_pinmux();
	pr_info("rc522: unloaded\n");
}

module_init(rc522_module_init);
module_exit(rc522_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI, adapted from the Loongson 2K500 textbook experiment");
MODULE_DESCRIPTION("MFRC522 NFC/RFID driver for the Loongson 2K500 teaching platform");
