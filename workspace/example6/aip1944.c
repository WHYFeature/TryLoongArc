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

#include "aip1944.h"


/*
 * 教材给出的 GPIO 复用寄存器
 */
#define GPIO5548ADDR    0x1fe104a8
#define GPIO119112      0x1fe104c8


static void __iomem *GPIO5548ADDR_V;
static void __iomem *GPIO119112_V;


struct aip_dev {
    dev_t devid;             /* 设备号 */
    void *private_data;      /* 私有数据 */

    int clk_gpio;
    int dio_gpio;
    int stb_gpio;
};


static struct aip_dev misc_aip;


/*
 * ==========================================================
 * AiP1944 底层串行发送
 * ==========================================================
 *
 * 数据手册：
 *
 * 1. 数据从低位开始发送；
 * 2. AiP1944 在 CLK 上升沿读取 DIO；
 * 3. STB 拉低以后开始一次传输；
 * 4. STB 为高时 CLK 被忽略。
 *
 * 这里严格按照手册时序：
 *
 *          CLK
 *           ----\____/----\____/----
 *
 *          DIO
 *               --b0------b1---------
 *
 * 数据在 CLK 低电平期间准备，
 * 上升沿由 AiP1944 锁存。
 */
static void aip_write_byte(unsigned char dat)
{
    int i;

    for (i = 0; i < 8; i++) {

        /*
         * 先把 CLK 拉低。
         */
        gpio_set_value(misc_aip.clk_gpio, 0);

        /*
         * AiP1944 低位先传。
         */
        if (dat & 0x01)
            gpio_set_value(misc_aip.dio_gpio, 1);
        else
            gpio_set_value(misc_aip.dio_gpio, 0);

        /*
         * 数据建立时间。
         *
         * 数据手册要求最小 100ns，
         * 这里用 2us，留足裕量。
         */
        udelay(2);

        /*
         * CLK 上升沿：
         * AiP1944 在这里读取 DIO。
         */
        gpio_set_value(misc_aip.clk_gpio, 1);

        /*
         * 数据保持时间。
         */
        udelay(2);

        /*
         * 下一个 bit。
         */
        dat >>= 1;
    }

    /*
     * 特别注意：
     *
     * 一字节发送完成以后 CLK 保持高电平。
     * 不像上一版那样人为再拉成低电平。
     *
     * 这与 AiP1944 数据手册图(4)的时序一致。
     */
}


/*
 * ==========================================================
 * 发送独立命令
 * ==========================================================
 */
static void write_cmd(unsigned char cmd)
{
    /*
     * 串行总线空闲：
     *
     * CLK = 1
     * STB = 1
     */
    gpio_set_value(misc_aip.clk_gpio, 1);

    udelay(2);

    /*
     * STB 下降沿初始化串行接口。
     */
    gpio_set_value(misc_aip.stb_gpio, 0);

    udelay(2);

    /*
     * STB 拉低后的第一个字节就是命令。
     */
    aip_write_byte(cmd);

    udelay(2);

    /*
     * 结束本次命令。
     */
    gpio_set_value(misc_aip.stb_gpio, 1);

    udelay(2);
}


/*
 * ==========================================================
 * 显示 16x16 点阵
 * ==========================================================
 *
 * buf：
 * 一个字符为 32 字节。
 *
 * 每一行使用两个字节：
 *
 * byte 0 -> SEG1~SEG8
 * byte 1 -> SEG9~SEG16
 *
 * AiP1944 的一个 GRID 占四字节 RAM，
 * 因为这里只使用 16 段，所以另外两个字节写 0。
 */
static void display_arr(unsigned char *buf)
{
    int i;


    /*
     * --------------------------------------------------
     * 教材：
     * 0x08 -> 16 位、16 段模式
     * --------------------------------------------------
     */
    write_cmd(0x08);


    /*
     * --------------------------------------------------
     * 教材：
     * 0x40 -> 写显示数据、地址自动增加
     * --------------------------------------------------
     */
    write_cmd(0x40);


    /*
     * --------------------------------------------------
     * 写 RAM
     * --------------------------------------------------
     *
     * 数据手册规定：
     *
     * 地址命令发完以后 STB 不能马上拉高，
     * 后面的显示数据必须在同一次 STB 低电平期间
     * 连续发送。
     *
     * 因此这里不能直接：
     *
     *     write_cmd(0xc0);
     *
     * 再单独发送数据。
     *
     * 教材省略了 write/write_cmd 的底层实现，
     * 这里按 AiP1944 数据手册补齐。
     */

    gpio_set_value(misc_aip.clk_gpio, 1);

    udelay(2);

    gpio_set_value(misc_aip.stb_gpio, 0);

    udelay(2);


    /*
     * 0xC0：
     * 显示 RAM 起始地址 = 00H。
     */
    aip_write_byte(0xC0);


    /*
     * GRID1 ~ GRID16
     */
    for (i = 0; i < 16; i++) {

        /*
         * SEG1 ~ SEG8
         */
        aip_write_byte(buf[i * 2]);

        /*
         * SEG9 ~ SEG16
         */
        aip_write_byte(buf[i * 2 + 1]);

        /*
         * SEG17 ~ SEG24 不使用。
         *
         * 教材明确要求：
         * 每写两个字节以后填写两个空字节。
         */
        aip_write_byte(0x00);
        aip_write_byte(0x00);
    }


    /*
     * 所有 RAM 数据发送完成，
     * 才结束 STB。
     */
    udelay(2);

    gpio_set_value(misc_aip.stb_gpio, 1);

    udelay(2);


    /*
     * --------------------------------------------------
     * 教材：
     * 0x8F -> 显示开
     * --------------------------------------------------
     */
    write_cmd(0x8F);
}


/*
 * ==========================================================
 * file_operations
 * ==========================================================
 */

static int aip_open(struct inode *inode, struct file *filp)
{
    /*
     * 与教材一致。
     */
    filp->private_data = &misc_aip;

    return 0;
}


static ssize_t aip_write(struct file *filp,
                         const char __user *buf,
                         size_t cnt,
                         loff_t *offt)
{
    int ret;

    /*
     * 教材原代码使用 64 字节缓冲区。
     */
    unsigned char databuf[64];

    struct aip_dev *dev = filp->private_data;

    /*
     * 当前 16x16 字模至少需要 32 byte。
     *
     * 此检查是为了避免缓冲区越界，
     * 不改变教材实验流程。
     */
    if (cnt < 32 || cnt > sizeof(databuf)) {
        pr_info("aip1944: invalid data size: %zu\n", cnt);
        return -EINVAL;
    }


    ret = copy_from_user(databuf, buf, cnt);

    /*
     * 教材写的是：
     *
     *     if(ret < 0)
     *
     * 但 copy_from_user 返回的是“未复制字节数”，
     * 因此这里仅作内核 API 层面的必要修正。
     */
    if (ret != 0) {
        pr_info("kernel write failed!\n");
        return -EFAULT;
    }


    /*
     * 与教材一致：
     * write 最后调用 display_arr。
     */
    display_arr(databuf);

    return cnt;
}


static const struct file_operations aip_fops = {
    .owner = THIS_MODULE,
    .open  = aip_open,
    .write = aip_write,
};


/*
 * 教材使用 misc 设备。
 *
 * 教材当前页面没有给出固定 minor，
 * 所以使用动态 minor。
 */
static struct miscdevice aip_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = AIP_NAME,
    .fops  = &aip_fops,
};


/*
 * ==========================================================
 * 模块初始化
 * ==========================================================
 */
static int __init aip_init(void)
{
    int ret;

    u32 GPIO119112_READ;
    u32 GPIO119112_WRITE;

    u32 GPIO5548_READ;
    u32 GPIO5548_WRITE;


    /*
     * GPIO 编号严格采用教材。
     */
    misc_aip.clk_gpio = AIP_CLK_GPIO;
    misc_aip.dio_gpio = AIP_DIO_GPIO;
    misc_aip.stb_gpio = AIP_STB_GPIO;


    /*
     * =====================================================
     * GPIO115 复用配置
     * =====================================================
     *
     * 以下寄存器地址和掩码全部照教材。
     */
    GPIO119112_V =
        ioremap(GPIO119112, sizeof(u32));

    if (!GPIO119112_V)
        return -ENOMEM;


    GPIO119112_READ =
        ioread32(GPIO119112_V);


    GPIO119112_WRITE =
        GPIO119112_READ & 0x77770777;


    iowrite32(GPIO119112_WRITE,
              GPIO119112_V);


    /*
     * =====================================================
     * GPIO50、GPIO51 复用配置
     * =====================================================
     *
     * 以下同样完全采用教材。
     */
    GPIO5548ADDR_V =
        ioremap(GPIO5548ADDR, sizeof(u32));

    if (!GPIO5548ADDR_V) {

        iounmap(GPIO119112_V);

        return -ENOMEM;
    }


    GPIO5548_READ =
        ioread32(GPIO5548ADDR_V);


    GPIO5548_WRITE =
        GPIO5548_READ & 0x77770077;


    iowrite32(GPIO5548_WRITE,
              GPIO5548ADDR_V);


    /*
     * =====================================================
     * 申请 GPIO
     * =====================================================
     */

    ret = gpio_request(misc_aip.clk_gpio,
                       "aip_clk");

    if (ret) {
        pr_info("aip1944: request CLK GPIO%d failed: %d\n",
                misc_aip.clk_gpio, ret);

        goto err_clk;
    }


    ret = gpio_request(misc_aip.dio_gpio,
                       "aip_dio");

    if (ret) {
        pr_info("aip1944: request DIO GPIO%d failed: %d\n",
                misc_aip.dio_gpio, ret);

        goto err_dio;
    }


    ret = gpio_request(misc_aip.stb_gpio,
                       "aip_stb");

    if (ret) {
        pr_info("aip1944: request STB GPIO%d failed: %d\n",
                misc_aip.stb_gpio, ret);

        goto err_stb;
    }


    /*
     * =====================================================
     * 设置初始电平
     * =====================================================
     *
     * 这里和上一版不同。
     *
     * 根据 AiP1944 时序图：
     *
     * CLK 空闲 = 高
     * STB 空闲 = 高
     *
     * DIO 也先置高。
     */

    ret = gpio_direction_output(
        misc_aip.clk_gpio, 1);

    if (ret)
        goto err_direction;


    ret = gpio_direction_output(
        misc_aip.dio_gpio, 1);

    if (ret)
        goto err_direction;


    ret = gpio_direction_output(
        misc_aip.stb_gpio, 1);

    if (ret)
        goto err_direction;


    /*
     * 稳定一下接口。
     */
    udelay(10);


    /*
     * =====================================================
     * 注册 misc
     * =====================================================
     */

    ret = misc_register(&aip_miscdev);

    if (ret) {
        pr_info("aip1944: misc_register failed: %d\n",
                ret);

        goto err_direction;
    }


    /*
     * 注意：
     *
     * 与上一版不同，这里不额外发送 0x80。
     * 教材没有要求模块加载时发送显示关闭命令。
     */
    pr_info("aip1944: module loaded\n");

    pr_info("aip1944: CLK=GPIO%d DIO=GPIO%d STB=GPIO%d\n",
            misc_aip.clk_gpio,
            misc_aip.dio_gpio,
            misc_aip.stb_gpio);

    return 0;


err_direction:

    gpio_free(misc_aip.stb_gpio);

err_stb:

    gpio_free(misc_aip.dio_gpio);

err_dio:

    gpio_free(misc_aip.clk_gpio);

err_clk:

    iounmap(GPIO119112_V);
    iounmap(GPIO5548ADDR_V);

    return ret;
}


/*
 * ==========================================================
 * 模块卸载
 * ==========================================================
 */
static void __exit aip_exit(void)
{
    /*
     * 教材原文就是 0x89，并注释“显示关”。
     *
     * 这里为了严格复现教材，先保留教材写法。
     *
     * 注意：
     * AiP1944 数据手册对该位的定义与教材注释
     * 存在不一致，但它不会影响我们当前的显示实验。
     */
    write_cmd(0x89);


    iounmap(GPIO119112_V);

    iounmap(GPIO5548ADDR_V);


    misc_deregister(&aip_miscdev);


    gpio_free(misc_aip.clk_gpio);

    gpio_free(misc_aip.dio_gpio);

    gpio_free(misc_aip.stb_gpio);


    pr_info("aip1944: module unloaded\n");
}


module_init(aip_init);
module_exit(aip_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("SDU");
MODULE_DESCRIPTION("AiP1944 dot matrix driver");