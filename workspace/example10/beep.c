#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define BEEP_NAME       "beep"

#define BEEPOFF         0
#define BEEPON          1

/* 教材：GPIO32~39 复用配置寄存器 */
#define GPMUX           0x1fe104a0

static void __iomem *GPFMUX0_V;

struct beep_dev {
    dev_t devid;
    void *private_data;
    int beep_gpio;
};

static struct beep_dev beep;


/*
 * 打开设备
 */
static int beep_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &beep;
    return 0;
}


/*
 * 写设备
 *
 * 用户写入：
 *   1 -> 蜂鸣器开启
 *   0 -> 蜂鸣器关闭
 *
 * 硬件实际：
 *   GPIO33 = 0 -> 响
 *   GPIO33 = 1 -> 不响
 */
static ssize_t beep_write(struct file *filp,
                          const char __user *buf,
                          size_t cnt,
                          loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char beepstat;

    struct beep_dev *dev = filp->private_data;

    if (cnt < 1)
        return -EINVAL;

    retvalue = copy_from_user(databuf, buf, 1);

    /*
     * 教材原文判断 retvalue < 0，
     * 这里按 copy_from_user 的实际返回规则修正。
     */
    if (retvalue != 0) {
        printk(KERN_ERR "beep: kernel write failed!\n");
        return -EFAULT;
    }

    beepstat = databuf[0];

    if (beepstat == BEEPON) {
        /* 教材：GPIO33 拉低，蜂鸣器响 */
        gpio_set_value(dev->beep_gpio, 0);
    } else if (beepstat == BEEPOFF) {
        /* GPIO33 拉高，蜂鸣器关闭 */
        gpio_set_value(dev->beep_gpio, 1);
    }

    return cnt;
}


static int beep_release(struct inode *inode, struct file *filp)
{
    return 0;
}


static const struct file_operations beep_fops = {
    .owner   = THIS_MODULE,
    .open    = beep_open,
    .write   = beep_write,
    .release = beep_release,
};


/*
 * 与教材唯一结构性不同：
 * 用 miscdevice 创建 /dev/beep，
 * 避免当前内核的 device_create CRC 冲突。
 */
static struct miscdevice beep_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = BEEP_NAME,
    .fops  = &beep_fops,
};


static int __init beep_init(void)
{
    int ret;
    u32 GPFMUX_READ;
    u32 GPFMUX_WRITE;

    /*
     * 教材明确指定 GPIO33。
     */
    beep.beep_gpio = 33;

    /*
     * GPIO33 复用配置。
     */
    GPFMUX0_V = ioremap(GPMUX, sizeof(u32));

    if (!GPFMUX0_V) {
        printk(KERN_ERR "beep: ioremap failed\n");
        return -ENOMEM;
    }

    GPFMUX_READ = ioread32(GPFMUX0_V);

    /*
     * 教材原代码：
     * GPFMUX_WRITE = GPFMUX_READ & 0x77777707;
     */
    GPFMUX_WRITE = GPFMUX_READ & 0x77777707;

    iowrite32(GPFMUX_WRITE, GPFMUX0_V);

    printk(KERN_INFO
           "beep: mux before=0x%08x after=0x%08x\n",
           GPFMUX_READ,
           ioread32(GPFMUX0_V));

    /*
     * 请求 GPIO33。
     *
     * 教材中 request label 写的是 "sda3"，
     * 这里保留。
     */
    ret = gpio_request(beep.beep_gpio, "sda3");

    if (ret) {
        printk(KERN_ERR
               "beep: request GPIO33 failed: %d\n",
               ret);

        iounmap(GPFMUX0_V);
        return ret;
    }

    /*
     * 教材：
     * GPIO33 输出高电平，默认关闭蜂鸣器。
     */
    ret = gpio_direction_output(beep.beep_gpio, 1);

    if (ret) {
        printk(KERN_ERR
               "beep: can't set GPIO33 output: %d\n",
               ret);

        gpio_free(beep.beep_gpio);
        iounmap(GPFMUX0_V);

        return ret;
    }

    /*
     * 注册 /dev/beep
     */
    ret = misc_register(&beep_miscdev);

    if (ret) {
        printk(KERN_ERR
               "beep: misc_register failed: %d\n",
               ret);

        gpio_set_value(beep.beep_gpio, 1);
        gpio_free(beep.beep_gpio);
        iounmap(GPFMUX0_V);

        return ret;
    }

    printk(KERN_INFO
           "beep: module loaded, GPIO%d\n",
           beep.beep_gpio);

    return 0;
}


static void __exit beep_exit(void)
{
    /*
     * 卸载前确保蜂鸣器关闭。
     */
    gpio_set_value(beep.beep_gpio, 1);

    misc_deregister(&beep_miscdev);

    gpio_free(beep.beep_gpio);

    iounmap(GPFMUX0_V);

    printk(KERN_INFO "beep: module unloaded\n");
}


module_init(beep_init);
module_exit(beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yangjinrun");
MODULE_DESCRIPTION("Loongson 2K500 buzzer driver");