#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "ch422g.h"


struct ch422g_dev {
    dev_t devid;             /* 设备号 */
    void *private_data;      /* 私有数据 */
    int sda_gpio;
    int scl_gpio;
};

/*
 * 教材其他代码全部使用 my_dev，
 * 因此这里按教材文字统一为 my_dev。
 */
struct ch422g_dev my_dev;


#define SDA3_GPIO 142
#define SCL3_GPIO 141

#define GPCFG 0x1fe104d4    //0x1fe104b0

#define SDA_D_OUT { }
#define SCL_D_OUT { }
#define DELAY_TIME 10


static void SCL_SET(void)
{
    gpio_set_value(my_dev.scl_gpio, 1);
}

static void SCL_CLR(void)
{
    gpio_set_value(my_dev.scl_gpio, 0);
}

static void SDA_SET(void)
{
    gpio_set_value(my_dev.sda_gpio, 1);
}

static void SDA_CLR(void)
{
    gpio_set_value(my_dev.sda_gpio, 0);
}


void I2c_Start(void)
{
    SDA_SET();
    SDA_D_OUT;

    SCL_SET();
    SCL_D_OUT;

    udelay(DELAY_TIME);

    SDA_CLR();
    udelay(DELAY_TIME);

    SCL_CLR();
}


void I2c_Stop(void)
{
    SDA_CLR();
    udelay(DELAY_TIME);

    SCL_SET();
    udelay(DELAY_TIME);

    SDA_SET();
    udelay(DELAY_TIME);
}


void I2c_WrByte(unsigned char dat)
{
    unsigned char i;

    for(i = 0; i != 8; i++)
    {
        if(dat & 0x80)
        {
            SDA_SET();
        }
        else
        {
            SDA_CLR();
        }

        udelay(DELAY_TIME);

        SCL_SET();

        dat <<= 1;

        udelay(DELAY_TIME);

        SCL_CLR();
    }

    SDA_SET();
    udelay(DELAY_TIME);

    SCL_SET();          /* 接收应答 */
    udelay(DELAY_TIME);

    SCL_CLR();
}


void CH422_WriteByte(unsigned short cmd)
{
    I2c_Start();

    I2c_WrByte((unsigned char)(cmd >> 8));
    I2c_WrByte((unsigned char)cmd);

    I2c_Stop();
}


static int ch422g_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &my_dev;
    return 0;
}


static ssize_t ch422g_write(struct file *filp,
                            const char __user *buf,
                            size_t cnt,
                            loff_t *offt)
{
    int retvalue;
    struct ch422g_dev *dev = filp->private_data;
    unsigned char databuf[5], cmd, ch1, ch2, ch3, ch4;

    retvalue = copy_from_user(databuf, buf, cnt);

    if(retvalue < 0)
        return -EFAULT;

    cmd = databuf[0];
    ch1 = databuf[1];
    ch2 = databuf[2];
    ch3 = databuf[3];
    ch4 = databuf[4];

    CH422_WriteByte(CH422_SET_IO_CMD + 0x07);  //0111
    CH422_WriteByte(CH422_DIG0 | ch1);

    CH422_WriteByte(CH422_SET_IO_CMD + 0x0b);  //1011
    CH422_WriteByte(CH422_DIG1 | ch2);

    CH422_WriteByte(CH422_SET_IO_CMD + 0x0d);  //1101
    CH422_WriteByte(CH422_DIG2 | ch3);

    CH422_WriteByte(CH422_SET_IO_CMD + 0x0e);  //1110
    CH422_WriteByte(CH422_DIG3 | ch4);

    return 0;
}


#define CH422G_NAME  "ch422g"
#define CH422G_MINOR 146

static struct file_operations misc_ch422g_fops = {
    .owner = THIS_MODULE,
    .open  = ch422g_open,
    .write = ch422g_write,
};

static struct miscdevice ch422g_miscdev = {
    .minor = CH422G_MINOR,
    .name  = CH422G_NAME,
    .fops  = &misc_ch422g_fops,
};


#define CH422_SYS_CMD     0x4800
#define CH422_IO_OE_BIT   0x0001
#define CH422_A_SCAN_BIT  0x0004


static void __iomem *GPCFG_V;


static int __init ch422g_init(void)
{
    int ret;
    u32 GPFCFG_read, GPFCFG_write;

    /* 配置 GPIO 复用 */
    GPCFG_V = ioremap(GPCFG, sizeof(u32));

    GPFCFG_read = ioread32(GPCFG_V);

    GPFCFG_write = (GPFCFG_read & 0x70077777);

    iowrite32(GPFCFG_write, GPCFG_V);

    my_dev.sda_gpio = SDA3_GPIO;
    my_dev.scl_gpio = SCL3_GPIO;

    gpio_request(my_dev.sda_gpio, "sda3");
    gpio_request(my_dev.scl_gpio, "scl3");

    ret = gpio_direction_output(my_dev.sda_gpio, 0);

    if(ret < 0)
        printk("can't set sda gpio!\r\n");

    ret = gpio_direction_output(my_dev.scl_gpio, 0);

    if(ret < 0)
        printk("can't set scl gpio!\r\n");


    /*
     * 教材文字要求注册 misc 设备，
     * 但打印出的源码漏掉了这一调用。
     */
    ret = misc_register(&ch422g_miscdev);

    if(ret < 0)
    {
        printk("misc device register failed!\r\n");
        return -EFAULT;
    }


    CH422_WriteByte(CH422_SYS_CMD |
                    CH422_IO_OE_BIT |
                    CH422_A_SCAN_BIT);

    return ret;
}


static void __exit ch422g_exit(void)
{
    iounmap(GPCFG_V);

    misc_deregister(&ch422g_miscdev);

    gpio_free(my_dev.sda_gpio);
    gpio_free(my_dev.scl_gpio);
}


module_init(ch422g_init);
module_exit(ch422g_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yangjinrun");