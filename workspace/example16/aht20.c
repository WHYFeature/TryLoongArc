#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/io.h>


/*
 * ============================================================
 * AHT20 基本参数
 * ============================================================
 */

#define DEVICE_NAME         "aht20"

/*
 * 教材指定的 misc 次设备号。
 */
#define DEVICE_MINOR        147


/*
 * 教材：
 * AHT20 挂载在 I2C3
 *
 * SDA = GPIO68
 * SCL = GPIO69
 */
#define SDA_GPIO            68
#define SCL_GPIO            69


/*
 * AHT20 7-bit 地址。
 *
 * write = 0x70
 * read  = 0x71
 */
#define DEVICE_ADDR         0x38

#define DEVICE_WRITE_ADDR   ((DEVICE_ADDR << 1) | 0)
#define DEVICE_READ_ADDR    ((DEVICE_ADDR << 1) | 1)


/*
 * 教材软件 I2C 延迟。
 */
#define DELAY_TIME          10


/*
 * GPIO64~71 的复用配置寄存器。
 *
 * 教材第16章给出的地址。
 */
#define GPCFG               0x1fe104b0


static void __iomem *GPCFG_V;


/*
 * ============================================================
 * AHT20 私有数据
 * ============================================================
 */

struct aht20_dev {

    dev_t devid;

    void *private_data;

    int sda_gpio;

    int scl_gpio;

    unsigned char write_addr;

    unsigned char read_addr;
};


static struct aht20_dev my_dev;


/*
 * GPIO 是否已经申请成功。
 * 用于错误处理和卸载。
 */
static int sda_requested = 0;
static int scl_requested = 0;


/*
 * ============================================================
 * 软件 I2C GPIO 基本操作
 * ============================================================
 */

static void SCL_SET(void)
{
    gpio_set_value(
        my_dev.scl_gpio,
        1);
}


static void SCL_CLR(void)
{
    gpio_set_value(
        my_dev.scl_gpio,
        0);
}


static void SDA_SET(void)
{
    gpio_set_value(
        my_dev.sda_gpio,
        1);
}


static void SDA_CLR(void)
{
    gpio_set_value(
        my_dev.sda_gpio,
        0);
}


/*
 * 教材第11章：
 * 接收数据时 SDA 需要切换输入/输出方向。
 */
static void SDA_D_OUT(void)
{
    gpio_direction_output(
        my_dev.sda_gpio,
        1);
}


static void SDA_D_IN(void)
{
    gpio_direction_input(
        my_dev.sda_gpio);
}


static int SDA_READ(void)
{
    return gpio_get_value(
        my_dev.sda_gpio);
}


/*
 * ============================================================
 * I2C START
 *
 * 教材流程：
 *
 * SDA=1
 * SCL=1
 * SDA 1->0
 * SCL=0
 * ============================================================
 */

static void I2c_Start(void)
{
    SDA_D_OUT();

    SDA_SET();

    SCL_SET();

    udelay(DELAY_TIME);


    SDA_CLR();

    udelay(DELAY_TIME);


    SCL_CLR();

    udelay(DELAY_TIME);
}


/*
 * ============================================================
 * I2C STOP
 *
 * SCL 高电平期间 SDA 从 0 -> 1。
 * ============================================================
 */

static void I2c_Stop(void)
{
    SDA_D_OUT();

    SDA_CLR();

    SCL_CLR();

    udelay(DELAY_TIME);


    SCL_SET();

    udelay(DELAY_TIME);


    SDA_SET();

    udelay(DELAY_TIME);
}


/*
 * ============================================================
 * I2C ACK
 * ============================================================
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

    udelay(DELAY_TIME);
}


/*
 * ============================================================
 * I2C NACK
 * ============================================================
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

    udelay(DELAY_TIME);
}


/*
 * ============================================================
 * 写 1 字节
 *
 * 基本逻辑保持教材：
 *
 * MSB -> LSB
 *
 * 每位：
 *     设置 SDA
 *     SCL 拉高
 *     SCL 拉低
 *
 * 第9个时钟为 ACK 周期。
 * ============================================================
 */

static void I2c_WrByte(unsigned char dat)
{
    unsigned char i;


    SDA_D_OUT();

    SCL_CLR();


    for (i = 0; i < 8; i++) {

        if (dat & 0x80)
            SDA_SET();
        else
            SDA_CLR();


        udelay(DELAY_TIME);


        SCL_SET();

        udelay(DELAY_TIME);


        SCL_CLR();

        dat <<= 1;
    }


    /*
     * 教材原代码这里使用：
     *
     * SDA_SET();
     * SCL_SET();
     *
     * 然后产生 ACK 时钟，但没有检查 ACK。
     *
     * 为了尽量保持教材行为，这里仍采用相同方案。
     */

    SDA_SET();

    udelay(DELAY_TIME);


    SCL_SET();

    udelay(DELAY_TIME);


    SCL_CLR();

    udelay(DELAY_TIME);
}


/*
 * ============================================================
 * 读 1 字节
 *
 * ack != 0：读取后发送 ACK
 * ack == 0：读取后发送 NACK
 *
 * 教材 AHT20_Recv：
 *
 * cnt-i-1
 *
 * 因此最后一个字节发送 NACK。
 * ============================================================
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
 * ============================================================
 * AHT20 发送命令
 *
 * 教材：
 *
 * START
 * 0x70
 * 命令数据
 * STOP
 * ============================================================
 */

static void AHT20_SendCmd(
        unsigned char *cmd_buf,
        size_t cnt)
{
    int i;


    I2c_Start();


    I2c_WrByte(
        my_dev.write_addr);


    for (i = 0; i < cnt; i++) {

        I2c_WrByte(
            cmd_buf[i]);
    }


    I2c_Stop();
}


/*
 * ============================================================
 * AHT20 接收
 *
 * 教材：
 *
 * START
 * 0x71
 * 连续读取
 * 最后一个 byte NACK
 * STOP
 * ============================================================
 */

static void AHT20_Recv(
        unsigned char *buf,
        size_t cnt)
{
    int i;


    I2c_Start();


    I2c_WrByte(
        my_dev.read_addr);


    for (i = 0; i < cnt; i++) {

        buf[i] =
            I2c_RdByte(
                cnt - i - 1);
    }


    I2c_Stop();
}


/*
 * ============================================================
 * open
 * ============================================================
 */

static int aht20_open(
        struct inode *inode,
        struct file *filp)
{
    filp->private_data =
        &my_dev;


    return 0;
}


/*
 * ============================================================
 * read
 *
 * 教材流程：
 *
 * delay 50ms
 *
 * 发送：
 * AC 33 00
 *
 * delay 80ms
 *
 * 读取7字节：
 *
 * status
 * humidity[19:12]
 * humidity[11:4]
 * humidity[3:0] + temp[19:16]
 * temp[15:8]
 * temp[7:0]
 * CRC
 * ============================================================
 */

static ssize_t aht20_read(
        struct file *filp,
        char __user *buf,
        size_t cnt,
        loff_t *offt)
{
    unsigned char databuf[7];

    unsigned char cmd[3];

    size_t copy_len;


    /*
     * 教材原代码。
     */
    mdelay(50);


    cmd[0] = 0xAC;
    cmd[1] = 0x33;
    cmd[2] = 0x00;


    AHT20_SendCmd(
        cmd,
        3);


    /*
     * AHT20 完成一次测量需要等待。
     *
     * 教材使用 80ms。
     */
    mdelay(80);


    AHT20_Recv(
        databuf,
        7);


    /*
     * 教材原代码直接：
     *
     * copy_to_user(buf, databuf, cnt)
     *
     * 如果 cnt > 7 就可能越界。
     *
     * 这里做唯一必要的边界修复。
     */
    copy_len =
        cnt > sizeof(databuf) ?
        sizeof(databuf) :
        cnt;


    if (copy_to_user(
            buf,
            databuf,
            copy_len))
        return -EFAULT;


    /*
     * 教材直接 return copy_to_user(...)
     * 成功时会返回 0，不符合 read() 常规语义。
     *
     * 修改为成功读取的字节数。
     */
    return copy_len;
}


/*
 * ============================================================
 * release
 * ============================================================
 */

static int aht20_release(
        struct inode *inode,
        struct file *filp)
{
    return 0;
}


/*
 * ============================================================
 * file_operations
 * ============================================================
 */

static const struct file_operations aht20_ops = {

    .owner   = THIS_MODULE,

    .open    = aht20_open,

    .read    = aht20_read,

    .release = aht20_release,
};


/*
 * ============================================================
 * miscdevice
 *
 * 教材就是 miscdevice，不需要改成 cdev。
 * ============================================================
 */

static struct miscdevice aht20_miscdev = {

    .minor = DEVICE_MINOR,

    .name = DEVICE_NAME,

    .fops = &aht20_ops,
};


/*
 * ============================================================
 * 模块初始化
 * ============================================================
 */

static int __init aht20_init(void)
{
    int ret;

    u32 GPCFG_read;
    u32 GPCFG_write;


    printk(KERN_INFO
           "aht20 module init\n");


    /*
     * 教材：
     * 先注册 miscdevice。
     */
    ret =
        misc_register(
            &aht20_miscdev);


    if (ret < 0) {

        printk(KERN_ERR
               "aht20: misc device register failed: %d\n",
               ret);

        return ret;
    }


    /*
     * GPIO68、GPIO69 复用配置。
     */
    GPCFG_V =
        ioremap(
            GPCFG,
            sizeof(u32));


    if (!GPCFG_V) {

        printk(KERN_ERR
               "aht20: ioremap GPCFG failed\n");

        ret = -ENOMEM;

        goto err_misc;
    }


    GPCFG_read =
        ioread32(
            GPCFG_V);


    /*
     * 教材原值：
     *
     * 0x77007777
     *
     * 将 GPIO68/69 对应 mux 清零，
     * 配置为 GPIO 模式。
     */
    GPCFG_write =
        GPCFG_read &
        0x77007777;


    iowrite32(
        GPCFG_write,
        GPCFG_V);


    printk(KERN_INFO
           "aht20: GPCFG before=0x%08x after=0x%08x\n",
           GPCFG_read,
           ioread32(GPCFG_V));


    /*
     * 私有数据。
     */
    my_dev.sda_gpio =
        SDA_GPIO;


    my_dev.scl_gpio =
        SCL_GPIO;


    my_dev.write_addr =
        DEVICE_WRITE_ADDR;


    my_dev.read_addr =
        DEVICE_READ_ADDR;


    /*
     * GPIO68
     */
    ret =
        gpio_request(
            my_dev.sda_gpio,
            "sda3");


    if (ret < 0) {

        printk(KERN_ERR
               "aht20: request GPIO68 failed: %d\n",
               ret);

        goto err_iomap;
    }


    sda_requested = 1;


    /*
     * GPIO69
     */
    ret =
        gpio_request(
            my_dev.scl_gpio,
            "scl3");


    if (ret < 0) {

        printk(KERN_ERR
               "aht20: request GPIO69 failed: %d\n",
               ret);

        goto err_sda;
    }


    scl_requested = 1;


    /*
     * I2C 空闲状态：
     *
     * SDA=1
     * SCL=1
     */
    ret =
        gpio_direction_output(
            my_dev.sda_gpio,
            1);


    if (ret < 0) {

        printk(KERN_ERR
               "aht20: can't set SDA GPIO: %d\n",
               ret);

        goto err_gpio;
    }


    ret =
        gpio_direction_output(
            my_dev.scl_gpio,
            1);


    if (ret < 0) {

        printk(KERN_ERR
               "aht20: can't set SCL GPIO: %d\n",
               ret);

        goto err_gpio;
    }


    printk(KERN_INFO
           "aht20: SDA=GPIO%d SCL=GPIO%d\n",
           my_dev.sda_gpio,
           my_dev.scl_gpio);


    printk(KERN_INFO
           "aht20: write_addr=0x%02x read_addr=0x%02x\n",
           my_dev.write_addr,
           my_dev.read_addr);


    printk(KERN_INFO
           "aht20 module loaded\n");


    return 0;



err_gpio:

    if (scl_requested) {

        gpio_free(
            my_dev.scl_gpio);

        scl_requested = 0;
    }


err_sda:

    if (sda_requested) {

        gpio_free(
            my_dev.sda_gpio);

        sda_requested = 0;
    }


err_iomap:

    if (GPCFG_V) {

        iounmap(
            GPCFG_V);

        GPCFG_V = NULL;
    }


err_misc:

    misc_deregister(
        &aht20_miscdev);


    return ret;
}


/*
 * ============================================================
 * 模块卸载
 * ============================================================
 */

static void __exit aht20_exit(void)
{
    if (scl_requested) {

        gpio_free(
            my_dev.scl_gpio);

        scl_requested = 0;
    }


    if (sda_requested) {

        gpio_free(
            my_dev.sda_gpio);

        sda_requested = 0;
    }


    if (GPCFG_V) {

        iounmap(
            GPCFG_V);

        GPCFG_V = NULL;
    }


    misc_deregister(
        &aht20_miscdev);


    printk(KERN_INFO
           "aht20 module unloaded\n");
}


module_init(aht20_init);
module_exit(aht20_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("yangjinrun");
MODULE_DESCRIPTION("Loongson 2K500 AHT20 humidity and temperature sensor");