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


#define LED_NAME        "led"

#define LEDOFF          0
#define LEDON           1

/*
 * 教材：GPIO136~143 复用配置寄存器
 */
#define GPIO_143_136    0x1fe104d4


static void __iomem *gpio_v;


struct led_dev {
    void *private_data;

    int green_gpio;
    int yellow_gpio;
    int red_gpio;
};


static struct led_dev led;


/*
 * ==========================================================
 * open
 * ==========================================================
 */
static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &led;

    return 0;
}


/*
 * ==========================================================
 * write
 *
 * 用户空间传入三个字节：
 *
 * databuf[0] -> 绿灯
 * databuf[1] -> 黄灯
 * databuf[2] -> 红灯
 *
 * 0 = 关闭
 * 1 = 开启
 * ==========================================================
 */
static ssize_t led_write(struct file *filp,
                         const char __user *buf,
                         size_t cnt,
                         loff_t *offt)
{
    int retvalue;
    unsigned char databuf[3];

    struct led_dev *dev = filp->private_data;


    /*
     * 教材测试程序固定写入三个字节。
     */
    if (cnt < 3)
        return -EINVAL;


    retvalue = copy_from_user(databuf, buf, 3);

    if (retvalue != 0) {
        printk(KERN_ERR
               "led: kernel write failed!\n");

        return -EFAULT;
    }


    /*
     * 绿灯
     */
    if (databuf[0] == LEDON)
        gpio_set_value(dev->green_gpio, 1);
    else if (databuf[0] == LEDOFF)
        gpio_set_value(dev->green_gpio, 0);


    /*
     * 黄灯
     */
    if (databuf[1] == LEDON)
        gpio_set_value(dev->yellow_gpio, 1);
    else if (databuf[1] == LEDOFF)
        gpio_set_value(dev->yellow_gpio, 0);


    /*
     * 红灯
     */
    if (databuf[2] == LEDON)
        gpio_set_value(dev->red_gpio, 1);
    else if (databuf[2] == LEDOFF)
        gpio_set_value(dev->red_gpio, 0);


    return cnt;
}


/*
 * ==========================================================
 * release
 * ==========================================================
 */
static int led_release(struct inode *inode,
                       struct file *filp)
{
    return 0;
}


/*
 * ==========================================================
 * file_operations
 * ==========================================================
 */
static const struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .write   = led_write,
    .release = led_release,
};


/*
 * ==========================================================
 * 改写点：
 *
 * 教材原版：
 *     cdev
 *     class_create
 *     device_create
 *
 * 这里：
 *     miscdevice
 *
 * 最终设备节点仍然叫 /dev/led
 * ==========================================================
 */
static struct miscdevice led_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = LED_NAME,
    .fops  = &led_fops,
};


/*
 * ==========================================================
 * 模块初始化
 * ==========================================================
 */
static int __init led_init(void)
{
    int ret;
    u32 gpio_read;
    u32 gpio_write;


    /*
     * GPIO 编号完全按照教材。
     */
    led.green_gpio  = 136;
    led.yellow_gpio = 138;
    led.red_gpio    = 137;


    /*
     * ======================================================
     * 1. 配置 GPIO 复用寄存器
     * ======================================================
     */

    gpio_v = ioremap(GPIO_143_136,
                     sizeof(u32));

    if (!gpio_v) {
        printk(KERN_ERR
               "led: ioremap failed!\n");

        return -ENOMEM;
    }


    gpio_read = ioread32(gpio_v);


    /*
     * 教材原代码：
     *
     * gpio_write = gpio_read & 0x77777000;
     */
    gpio_write =
        gpio_read & 0x77777000;


    iowrite32(gpio_write, gpio_v);


    printk(KERN_INFO
           "led: mux before=0x%08x after=0x%08x\n",
           gpio_read,
           ioread32(gpio_v));


    /*
     * ======================================================
     * 2. 请求 GPIO136
     * ======================================================
     */

    ret = gpio_request(led.green_gpio,
                       "green_gpio");

    if (ret) {
        printk(KERN_ERR
               "led: request GPIO136 failed: %d\n",
               ret);

        goto err_green;
    }


    /*
     * ======================================================
     * 3. 请求 GPIO138
     * ======================================================
     */

    ret = gpio_request(led.yellow_gpio,
                       "yellow_gpio");

    if (ret) {
        printk(KERN_ERR
               "led: request GPIO138 failed: %d\n",
               ret);

        goto err_yellow;
    }


    /*
     * ======================================================
     * 4. 请求 GPIO137
     * ======================================================
     */

    ret = gpio_request(led.red_gpio,
                       "red_gpio");

    if (ret) {
        printk(KERN_ERR
               "led: request GPIO137 failed: %d\n",
               ret);

        goto err_red;
    }


    /*
     * ======================================================
     * 5. 配置三个 GPIO 为输出
     *
     * 教材初始化为 0。
     * ======================================================
     */

    ret = gpio_direction_output(
        led.green_gpio, 0);

    if (ret) {
        printk(KERN_ERR
               "led: GPIO136 direction failed: %d\n",
               ret);

        goto err_direction;
    }


    ret = gpio_direction_output(
        led.yellow_gpio, 0);

    if (ret) {
        printk(KERN_ERR
               "led: GPIO138 direction failed: %d\n",
               ret);

        goto err_direction;
    }


    ret = gpio_direction_output(
        led.red_gpio, 0);

    if (ret) {
        printk(KERN_ERR
               "led: GPIO137 direction failed: %d\n",
               ret);

        goto err_direction;
    }


    /*
     * ======================================================
     * 6. 注册 misc 设备
     * ======================================================
     */

    ret = misc_register(&led_miscdev);

    if (ret) {
        printk(KERN_ERR
               "led: misc_register failed: %d\n",
               ret);

        goto err_direction;
    }


    printk(KERN_INFO
           "led: module loaded\n");

    printk(KERN_INFO
           "led: green=GPIO%d yellow=GPIO%d red=GPIO%d\n",
           led.green_gpio,
           led.yellow_gpio,
           led.red_gpio);


    return 0;


err_direction:

    gpio_free(led.red_gpio);

err_red:

    gpio_free(led.yellow_gpio);

err_yellow:

    gpio_free(led.green_gpio);

err_green:

    iounmap(gpio_v);

    return ret;
}


/*
 * ==========================================================
 * 模块卸载
 * ==========================================================
 */
static void __exit led_exit(void)
{
    /*
     * 卸载前全部关闭。
     */
    gpio_set_value(
        led.green_gpio, 0);

    gpio_set_value(
        led.yellow_gpio, 0);

    gpio_set_value(
        led.red_gpio, 0);


    /*
     * 注销 misc 设备。
     */
    misc_deregister(
        &led_miscdev);


    /*
     * 释放 GPIO。
     */
    gpio_free(
        led.green_gpio);

    gpio_free(
        led.yellow_gpio);

    gpio_free(
        led.red_gpio);


    /*
     * 取消寄存器映射。
     */
    iounmap(gpio_v);


    printk(KERN_INFO
           "led: module unloaded\n");
}


module_init(led_init);
module_exit(led_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("yangjinrun");
MODULE_DESCRIPTION("Loongson 2K500 LED driver using miscdevice");