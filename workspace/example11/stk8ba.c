#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#include "stk8ba.h"


#define DEVICE_NAME     "stk8ba"
#define DEVICE_MINOR    148

#define DELAY_TIME      10

#define ACK             1
#define NACK            0


static void __iomem *GPCFG1_V;
static void __iomem *GPCFG2_V;


struct stk8ba_dev {
    dev_t devid;

    void *private_data;

    int sda_gpio;
    int scl_gpio;
    int irq_num;

    unsigned char write_addr;
    unsigned char read_addr;
};


static struct stk8ba_dev my_dev;


/*
 * ==========================================================
 * GPIO 基础操作
 * ==========================================================
 */

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


/*
 * 第 11 章新增：
 * SDA 在读取数据时需要在输入/输出之间切换。
 */
static void SDA_D_OUT(void)
{
    gpio_direction_output(my_dev.sda_gpio, 1);
}


static void SDA_D_IN(void)
{
    gpio_direction_input(my_dev.sda_gpio);
}


static int SDA_READ(void)
{
    return gpio_get_value(my_dev.sda_gpio);
}


/*
 * ==========================================================
 * 软件 I2C
 * ==========================================================
 */

/*
 * I2C START
 */
static void I2c_Start(void)
{
    /*
     * SDA 恢复为输出。
     */
    SDA_D_OUT();

    SDA_SET();
    SCL_SET();

    udelay(DELAY_TIME);

    /*
     * SCL 高电平期间：
     * SDA 从高变低 -> START
     */
    SDA_CLR();

    udelay(DELAY_TIME);

    SCL_CLR();
}


/*
 * I2C STOP
 */
static void I2c_Stop(void)
{
    SDA_D_OUT();

    SDA_CLR();

    udelay(DELAY_TIME);

    SCL_SET();

    udelay(DELAY_TIME);

    /*
     * SCL 高电平期间：
     * SDA 从低变高 -> STOP
     */
    SDA_SET();

    udelay(DELAY_TIME);
}


/*
 * 写一个字节。
 *
 * 教材按照 MSB -> LSB 发送。
 */
static void I2c_WrByte(unsigned char dat)
{
    unsigned char i;

    SDA_D_OUT();

    for (i = 0; i < 8; i++) {

        if (dat & 0x80)
            SDA_SET();
        else
            SDA_CLR();

        udelay(DELAY_TIME);

        SCL_SET();

        dat <<= 1;

        udelay(DELAY_TIME);

        SCL_CLR();
    }


    /*
     * 第九个时钟：ACK。
     *
     * 教材这里没有实际判断 ACK，
     * 仅释放 SDA 后产生第九个时钟。
     */
    SDA_SET();

    udelay(DELAY_TIME);

    SCL_SET();

    udelay(DELAY_TIME);

    SCL_CLR();
}


/*
 * 主机发送 ACK。
 */
static void I2c_Ack(void)
{
    SCL_CLR();

    SDA_D_OUT();

    SDA_CLR();

    udelay(DELAY_TIME);

    SCL_SET();

    udelay(DELAY_TIME);

    SCL_CLR();
}


/*
 * 主机发送 NACK。
 */
static void I2c_NAck(void)
{
    SCL_CLR();

    SDA_D_OUT();

    SDA_SET();

    udelay(DELAY_TIME);

    SCL_SET();

    udelay(DELAY_TIME);

    SCL_CLR();
}


/*
 * 读取一个字节。
 */
static unsigned char I2c_RdByte(int ack)
{
    unsigned char i;
    unsigned char ret = 0;

    SDA_D_IN();

    for (i = 0; i < 8; i++) {

        SCL_CLR();

        udelay(DELAY_TIME);

        SCL_SET();

        ret <<= 1;

        if (SDA_READ())
            ret++;

        udelay(DELAY_TIME);
    }


    if (ack)
        I2c_Ack();
    else
        I2c_NAck();


    return ret;
}


/*
 * ==========================================================
 * STK8BA58 寄存器访问
 * ==========================================================
 */

/*
 * 教材的单寄存器读取过程：
 *
 * START
 * 写地址
 * 寄存器地址
 * STOP
 *
 * START
 * 读地址
 * 读取数据
 * NACK
 * STOP
 */
static unsigned char STK8BA_Read_Reg(unsigned char reg)
{
    unsigned char val;

    I2c_Start();

    I2c_WrByte(my_dev.write_addr);

    I2c_WrByte(reg);

    I2c_Stop();


    I2c_Start();

    I2c_WrByte(my_dev.read_addr);

    val = I2c_RdByte(NACK);

    I2c_Stop();


    return val;
}


/*
 * 写 STK8BA58 寄存器。
 */
static void STK8BA_Write_Reg(unsigned char reg_addr,
                             unsigned char new_value)
{
    I2c_Start();

    I2c_WrByte(my_dev.write_addr);

    I2c_WrByte(reg_addr);

    I2c_WrByte(new_value);

    I2c_Stop();
}


/*
 * ==========================================================
 * XYZ 数据读取
 * ==========================================================
 */

static void stk8ba_read_xyz(unsigned char *data)
{
    data[0] =
        STK8BA_Read_Reg(STK8BA_XOUT1);

    data[1] =
        STK8BA_Read_Reg(STK8BA_XOUT2);

    data[2] =
        STK8BA_Read_Reg(STK8BA_YOUT1);

    data[3] =
        STK8BA_Read_Reg(STK8BA_YOUT2);

    data[4] =
        STK8BA_Read_Reg(STK8BA_ZOUT1);

    data[5] =
        STK8BA_Read_Reg(STK8BA_ZOUT2);
}


/*
 * ==========================================================
 * 文件操作
 * ==========================================================
 */

static int stk8ba_open(struct inode *inode,
                       struct file *filp)
{
    filp->private_data = &my_dev;

    return 0;
}


/*
 * 注意：
 *
 * 为了与教材用户程序完全一致，
 * 这里成功时仍然返回 copy_to_user() 的返回值。
 *
 * 因此：
 *
 *     成功 -> 0
 *
 * 教材用户程序也是用 if(ret == 0) 判断成功。
 */
static ssize_t stk8ba_read(struct file *filp,
                           char __user *buf,
                           size_t cnt,
                           loff_t *off)
{
    unsigned char data[6];

    size_t size;

    stk8ba_read_xyz(data);


    /*
     * 教材测试程序读取 6 字节。
     * 加这一层只是防止用户传入大于 6 的 cnt。
     */
    size = cnt;

    if (size > sizeof(data))
        size = sizeof(data);


    return copy_to_user(buf, data, size);
}


/*
 * 教材还提供了 write：
 *
 * 用户写两个字节：
 *
 * [寄存器地址][寄存器新值]
 */
static ssize_t stk8ba_write(struct file *filp,
                            const char __user *buf,
                            size_t cnt,
                            loff_t *offt)
{
    int ret;

    unsigned char databuf[2];


    if (cnt < 2)
        return -EINVAL;


    ret = copy_from_user(databuf,
                         buf,
                         sizeof(databuf));


    if (ret != 0) {

        pr_info(
            "stk8ba: kernel write failed!\n");

        return -EFAULT;
    }


    STK8BA_Write_Reg(
        databuf[0],
        databuf[1]);


    /*
     * 教材原版返回 0。
     */
    return 0;
}


static int stk8ba_release(struct inode *inode,
                          struct file *filp)
{
    return 0;
}


/*
 * ==========================================================
 * 中断处理
 * ==========================================================
 */

static irqreturn_t read_handler(int irq,
                                void *dev_id)
{
    /*
     * 教材这里只演示中断注册，
     * 中断处理函数没有进行额外处理。
     */
    return IRQ_HANDLED;
}


/*
 * ==========================================================
 * miscdevice
 * ==========================================================
 */

static const struct file_operations stk8ba_ops = {

    .owner   = THIS_MODULE,

    .open    = stk8ba_open,

    .read    = stk8ba_read,

    .write   = stk8ba_write,

    .release = stk8ba_release,
};


static struct miscdevice stk8ba_miscdev = {

    /*
     * 教材指定 minor = 148。
     */
    .minor = DEVICE_MINOR,

    .name = DEVICE_NAME,

    .fops = &stk8ba_ops,
};


/*
 * ==========================================================
 * 模块初始化
 * ==========================================================
 */

static int __init stk8ba_init(void)
{
    int ret;

    u32 GPCFG1_read;
    u32 GPCFG1_write;

    u32 GPCFG2_read;
    u32 GPCFG2_write;


    /*
     * ------------------------------------------------------
     * 1. 注册 misc 设备
     * ------------------------------------------------------
     */

    ret = misc_register(
        &stk8ba_miscdev);

    if (ret < 0) {

        printk(KERN_ERR
               "stk8ba: misc device register failed: %d\n",
               ret);

        return ret;
    }


    /*
     * ------------------------------------------------------
     * 2. 配置 GPIO100
     * ------------------------------------------------------
     */

    GPCFG1_V =
        ioremap(GPCFG1, sizeof(u32));

    if (!GPCFG1_V) {

        ret = -ENOMEM;

        goto err_gpcfg1;
    }


    GPCFG1_read =
        ioread32(GPCFG1_V);


    /*
     * 教材：
     * GPIO100 配置为 GPIO 功能。
     */
    GPCFG1_write =
        GPCFG1_read & 0x77707777;


    iowrite32(
        GPCFG1_write,
        GPCFG1_V);


    /*
     * ------------------------------------------------------
     * 3. 配置 GPIO68、GPIO69
     * ------------------------------------------------------
     */

    GPCFG2_V =
        ioremap(GPCFG2, sizeof(u32));

    if (!GPCFG2_V) {

        ret = -ENOMEM;

        goto err_gpcfg2;
    }


    GPCFG2_read =
        ioread32(GPCFG2_V);


    /*
     * 教材：
     * GPIO68、69 配置为 GPIO 功能。
     */
    GPCFG2_write =
        GPCFG2_read & 0x77007777;


    iowrite32(
        GPCFG2_write,
        GPCFG2_V);


    pr_info(
        "stk8ba: gpio100 mux before=0x%08x after=0x%08x\n",
        GPCFG1_read,
        ioread32(GPCFG1_V));

    pr_info(
        "stk8ba: gpio68/69 mux before=0x%08x after=0x%08x\n",
        GPCFG2_read,
        ioread32(GPCFG2_V));


    /*
     * ------------------------------------------------------
     * 4. 请求 GPIO
     * ------------------------------------------------------
     */

    ret = gpio_request(
        SDA_GPIO,
        "stk8ba_sda");

    if (ret) {

        pr_err(
            "stk8ba: request GPIO68 failed: %d\n",
            ret);

        goto err_sda;
    }


    ret = gpio_request(
        SCL_GPIO,
        "stk8ba_scl");

    if (ret) {

        pr_err(
            "stk8ba: request GPIO69 failed: %d\n",
            ret);

        goto err_scl;
    }


    ret = gpio_request(
        INT_GPIO,
        "stk8ba_int");

    if (ret) {

        pr_err(
            "stk8ba: request GPIO100 failed: %d\n",
            ret);

        goto err_int;
    }


    /*
     * ------------------------------------------------------
     * 5. 配置私有数据
     * ------------------------------------------------------
     */

    my_dev.sda_gpio =
        SDA_GPIO;

    my_dev.scl_gpio =
        SCL_GPIO;

    my_dev.write_addr =
        DEVICE_WRITE_ADDR;

    my_dev.read_addr =
        DEVICE_READ_ADDR;


    my_dev.irq_num =
        gpio_to_irq(INT_GPIO);


    pr_info(
        "stk8ba: INT GPIO100 irq=%d\n",
        my_dev.irq_num);


    /*
     * ------------------------------------------------------
     * 6. SDA/SCL 默认高
     * ------------------------------------------------------
     */

    ret =
        gpio_direction_output(
            my_dev.sda_gpio,
            1);

    if (ret < 0) {

        pr_err(
            "stk8ba: can't set SDA GPIO\n");

        goto err_direction;
    }


    ret =
        gpio_direction_output(
            my_dev.scl_gpio,
            1);

    if (ret < 0) {

        pr_err(
            "stk8ba: can't set SCL GPIO\n");

        goto err_direction;
    }


    /*
     * 中断脚输入。
     */
    ret =
        gpio_direction_input(
            INT_GPIO);

    if (ret < 0) {

        pr_err(
            "stk8ba: can't set INT GPIO\n");

        goto err_direction;
    }


    /*
     * ------------------------------------------------------
     * 7. 配置 STK8BA58 中断
     *
     * 以下四个值完全按照教材。
     * ------------------------------------------------------
     */

    STK8BA_Write_Reg(
        STK8BA_INTMAP1,
        0x05);

    STK8BA_Write_Reg(
        STK8BA_INTMAP2,
        0x01);

    STK8BA_Write_Reg(
        STK8BA_INTEN1,
        0x07);

    STK8BA_Write_Reg(
        STK8BA_INTEN2,
        0x10);


    /*
     * ------------------------------------------------------
     * 8. 申请中断
     * ------------------------------------------------------
     */

    ret =
        request_irq(
            my_dev.irq_num,
            read_handler,
            IRQF_TRIGGER_FALLING |
            IRQF_TRIGGER_RISING,
            "stk8ba-irq",
            &my_dev);


    if (ret < 0) {

        pr_err(
            "stk8ba: request irq failed: %d\n",
            ret);

        goto err_irq;
    }


    pr_info(
        "stk8ba: module loaded\n");

    pr_info(
        "stk8ba: SDA=GPIO68 SCL=GPIO69 INT=GPIO100 addr=0x18\n");


    return 0;


err_irq:

err_direction:

    gpio_free(INT_GPIO);

err_int:

    gpio_free(SCL_GPIO);

err_scl:

    gpio_free(SDA_GPIO);

err_sda:

    iounmap(GPCFG2_V);

err_gpcfg2:

    iounmap(GPCFG1_V);

err_gpcfg1:

    misc_deregister(
        &stk8ba_miscdev);

    return ret;
}


/*
 * ==========================================================
 * 模块卸载
 * ==========================================================
 */

static void __exit stk8ba_exit(void)
{
    free_irq(
        my_dev.irq_num,
        &my_dev);


    gpio_free(
        INT_GPIO);

    gpio_free(
        my_dev.sda_gpio);

    gpio_free(
        my_dev.scl_gpio);


    iounmap(
        GPCFG1_V);

    iounmap(
        GPCFG2_V);


    misc_deregister(
        &stk8ba_miscdev);


    pr_info(
        "stk8ba: module unloaded\n");
}


module_init(stk8ba_init);
module_exit(stk8ba_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("yangjinrun");
MODULE_DESCRIPTION("STK8BA58 driver for Loongson 2K500");