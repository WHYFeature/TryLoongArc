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


#define KEY_NAME        "key"

/*
 * 教材给出的 GPIO 复用寄存器
 */
#define GPIO_96_103     0x1fe104c0
#define GPIO_104_111    0x1fe104c4


/*
 * GPIO 复用寄存器虚拟地址
 */
static void __iomem *gpio_103_96_v;
static void __iomem *gpio_111_104_v;


/*
 * 教材原代码使用 g_flag 防止重复初始化。
 */
static int g_flag = 0;


/*
 * ============================================================
 * 设备结构体
 *
 * 教材原结构体中还有：
 *
 * dev_t
 * cdev
 * class
 * device
 *
 * 因为这里用 miscdevice 替代字符设备注册，
 * 这些成员不再需要。
 * ============================================================
 */
struct key_dev {
    int key_gpio[8];
    atomic_t keyvalue;
};


static struct key_dev keydev;


/*
 * ============================================================
 * GPIO 初始化
 *
 * 严格依据教材：
 *
 * GPIO103~106：行线，输入
 * GPIO107~110：列线，输出
 * ============================================================
 */
static int keyio_init(void)
{
    int ret;
    int i;

    u32 gpio_read;
    u32 gpio_write;


    /*
     * 如果已经初始化，不再重复申请 GPIO。
     */
    if (g_flag)
        return 0;


    /*
     * --------------------------------------------------------
     * GPIO96~103 复用寄存器
     *
     * 教材：
     * gpio_write = gpio_read & 0x07777777;
     *
     * 将 GPIO103 配置为 GPIO 功能。
     * --------------------------------------------------------
     */

    gpio_103_96_v =
        ioremap(GPIO_96_103, sizeof(u32));

    if (!gpio_103_96_v) {

        pr_err(
            "key: ioremap GPIO96~103 failed\n");

        return -ENOMEM;
    }


    gpio_read =
        ioread32(gpio_103_96_v);


    gpio_write =
        gpio_read & 0x07777777;


    iowrite32(
        gpio_write,
        gpio_103_96_v);


    pr_info(
        "key: GPIO96~103 mux before=0x%08x after=0x%08x\n",
        gpio_read,
        ioread32(gpio_103_96_v));


    /*
     * --------------------------------------------------------
     * GPIO104~111 复用寄存器
     *
     * 教材：
     * gpio_write = gpio_read & 0x70000000;
     *
     * 将 GPIO104~110 配置为 GPIO 功能。
     * --------------------------------------------------------
     */

    gpio_111_104_v =
        ioremap(GPIO_104_111, sizeof(u32));

    if (!gpio_111_104_v) {

        pr_err(
            "key: ioremap GPIO104~111 failed\n");

        iounmap(gpio_103_96_v);
        gpio_103_96_v = NULL;

        return -ENOMEM;
    }


    gpio_read =
        ioread32(gpio_111_104_v);


    gpio_write =
        gpio_read & 0x70000000;


    iowrite32(
        gpio_write,
        gpio_111_104_v);


    pr_info(
        "key: GPIO104~111 mux before=0x%08x after=0x%08x\n",
        gpio_read,
        ioread32(gpio_111_104_v));


    /*
     * --------------------------------------------------------
     * GPIO 编号
     *
     * 教材原顺序。
     * --------------------------------------------------------
     */

    keydev.key_gpio[0] = 103;
    keydev.key_gpio[1] = 104;
    keydev.key_gpio[2] = 105;
    keydev.key_gpio[3] = 106;

    keydev.key_gpio[4] = 107;
    keydev.key_gpio[5] = 108;
    keydev.key_gpio[6] = 109;
    keydev.key_gpio[7] = 110;


    /*
     * --------------------------------------------------------
     * GPIO103：行输入
     * --------------------------------------------------------
     */

    ret = gpio_request(
        keydev.key_gpio[0],
        "key0");

    if (ret) {

        pr_err(
            "key: request GPIO103 failed: %d\n",
            ret);

        goto err_gpio;
    }


    ret = gpio_direction_input(
        keydev.key_gpio[0]);

    if (ret) {

        pr_err(
            "key: GPIO103 input failed: %d\n",
            ret);

        goto err_gpio0;
    }


    /*
     * GPIO104
     */

    ret = gpio_request(
        keydev.key_gpio[1],
        "key1");

    if (ret) {

        pr_err(
            "key: request GPIO104 failed: %d\n",
            ret);

        goto err_gpio0;
    }


    ret = gpio_direction_input(
        keydev.key_gpio[1]);

    if (ret) {

        pr_err(
            "key: GPIO104 input failed: %d\n",
            ret);

        goto err_gpio1;
    }


    /*
     * GPIO105
     */

    ret = gpio_request(
        keydev.key_gpio[2],
        "key2");

    if (ret) {

        pr_err(
            "key: request GPIO105 failed: %d\n",
            ret);

        goto err_gpio1;
    }


    ret = gpio_direction_input(
        keydev.key_gpio[2]);

    if (ret) {

        pr_err(
            "key: GPIO105 input failed: %d\n",
            ret);

        goto err_gpio2;
    }


    /*
     * GPIO106
     */

    ret = gpio_request(
        keydev.key_gpio[3],
        "key3");

    if (ret) {

        pr_err(
            "key: request GPIO106 failed: %d\n",
            ret);

        goto err_gpio2;
    }


    ret = gpio_direction_input(
        keydev.key_gpio[3]);

    if (ret) {

        pr_err(
            "key: GPIO106 input failed: %d\n",
            ret);

        goto err_gpio3;
    }


    /*
     * --------------------------------------------------------
     * GPIO107~110：
     * 教材将列线设置为输出，初始为 0。
     * --------------------------------------------------------
     */

    ret = gpio_request(
        keydev.key_gpio[4],
        "key4");

    if (ret) {

        pr_err(
            "key: request GPIO107 failed: %d\n",
            ret);

        goto err_gpio3;
    }


    ret = gpio_direction_output(
        keydev.key_gpio[4],
        0);

    if (ret)
        goto err_gpio4;


    /*
     * GPIO108
     */

    ret = gpio_request(
        keydev.key_gpio[5],
        "key5");

    if (ret) {

        pr_err(
            "key: request GPIO108 failed: %d\n",
            ret);

        goto err_gpio4;
    }


    ret = gpio_direction_output(
        keydev.key_gpio[5],
        0);

    if (ret)
        goto err_gpio5;


    /*
     * GPIO109
     */

    ret = gpio_request(
        keydev.key_gpio[6],
        "key6");

    if (ret) {

        pr_err(
            "key: request GPIO109 failed: %d\n",
            ret);

        goto err_gpio5;
    }


    ret = gpio_direction_output(
        keydev.key_gpio[6],
        0);

    if (ret)
        goto err_gpio6;


    /*
     * GPIO110
     */

    ret = gpio_request(
        keydev.key_gpio[7],
        "key7");

    if (ret) {

        pr_err(
            "key: request GPIO110 failed: %d\n",
            ret);

        goto err_gpio6;
    }


    ret = gpio_direction_output(
        keydev.key_gpio[7],
        0);

    if (ret)
        goto err_gpio7;


    g_flag = 1;


    pr_info(
        "key: GPIO initialization success\n");


    pr_info(
        "key: rows GPIO103~106 input, "
        "columns GPIO107~110 output\n");


    return 0;



err_gpio7:

    gpio_free(keydev.key_gpio[7]);

err_gpio6:

    gpio_free(keydev.key_gpio[6]);

err_gpio5:

    gpio_free(keydev.key_gpio[5]);

err_gpio4:

    gpio_free(keydev.key_gpio[4]);

err_gpio3:

    gpio_free(keydev.key_gpio[3]);

err_gpio2:

    gpio_free(keydev.key_gpio[2]);

err_gpio1:

    gpio_free(keydev.key_gpio[1]);

err_gpio0:

    gpio_free(keydev.key_gpio[0]);

err_gpio:

    if (gpio_111_104_v) {
        iounmap(gpio_111_104_v);
        gpio_111_104_v = NULL;
    }

    if (gpio_103_96_v) {
        iounmap(gpio_103_96_v);
        gpio_103_96_v = NULL;
    }

    return ret;
}


/*
 * ============================================================
 * open
 *
 * 教材：
 * private_data = &keydev
 * keyio_init()
 * ============================================================
 */
static int key_open(struct inode *inode,
                    struct file *filp)
{
    int ret;


    filp->private_data =
        &keydev;


    ret = keyio_init();

    if (ret < 0)
        return ret;


    return 0;
}


/*
 * ============================================================
 * read
 *
 * 教材：
 *
 * 依次读取 GPIO103~110，
 * 向用户空间复制 8 个 int。
 * ============================================================
 */
static ssize_t key_read(struct file *filp,
                        char __user *buf,
                        size_t cnt,
                        loff_t *offt)
{
    int ret;

    int keyvalue[8];

    int i;


    struct key_dev *dev =
        filp->private_data;


    /*
     * 教材测试程序读取：
     *
     * int keyvalue[8]
     *
     * 即 32 字节。
     */
    if (cnt < sizeof(keyvalue))
        return -EINVAL;


    for (i = 0; i < 8; i++) {

        keyvalue[i] =
            gpio_get_value(
                dev->key_gpio[i]);
    }


    /*
     * 教材原版直接返回 copy_to_user 的值。
     *
     * 这里改成标准 read 语义：
     * 成功返回实际字节数。
     */
    ret =
        copy_to_user(
            buf,
            keyvalue,
            sizeof(keyvalue));


    if (ret != 0)
        return -EFAULT;


    return sizeof(keyvalue);
}


/*
 * ============================================================
 * write
 *
 * 教材：
 *
 * 用户发送 4 个字节，
 * 分别控制 GPIO107~110。
 * ============================================================
 */
static ssize_t key_write(struct file *filp,
                         const char __user *buf,
                         size_t cnt,
                         loff_t *offt)
{
    int ret;

    unsigned char databuf[4];


    struct key_dev *dev =
        filp->private_data;


    /*
     * 教材只需要 4 个字节。
     *
     * 避免教材原代码：
     *
     * copy_from_user(databuf, buf, cnt)
     *
     * 在 cnt > 4 时造成数组越界。
     */
    if (cnt != 4)
        return -EINVAL;


    ret =
        copy_from_user(
            databuf,
            buf,
            sizeof(databuf));


    /*
     * copy_from_user 返回未复制字节数，
     * 不是负数。
     */
    if (ret != 0) {

        pr_err(
            "key: kernel write failed\n");

        return -EFAULT;
    }


    /*
     * GPIO107~110。
     */
    gpio_set_value(
        dev->key_gpio[4],
        databuf[0] ? 1 : 0);


    gpio_set_value(
        dev->key_gpio[5],
        databuf[1] ? 1 : 0);


    gpio_set_value(
        dev->key_gpio[6],
        databuf[2] ? 1 : 0);


    gpio_set_value(
        dev->key_gpio[7],
        databuf[3] ? 1 : 0);


    return cnt;
}


/*
 * ============================================================
 * release
 * ============================================================
 */
static int key_release(struct inode *inode,
                       struct file *filp)
{
    return 0;
}


/*
 * ============================================================
 * file_operations
 * ============================================================
 */
static const struct file_operations key_fops = {

    .owner   = THIS_MODULE,

    .open    = key_open,

    .read    = key_read,

    .write   = key_write,

    .release = key_release,
};


/*
 * ============================================================
 * miscdevice
 *
 * 教材本身使用 cdev + class_create + device_create。
 *
 * 由于当前开发板 device_create 的 symbol version
 * 与编译源码树不一致，这里只把注册方式替换成 miscdevice。
 *
 * 用户接口仍然是 /dev/key。
 * ============================================================
 */
static struct miscdevice key_miscdev = {

    .minor = MISC_DYNAMIC_MINOR,

    .name = KEY_NAME,

    .fops = &key_fops,
};


/*
 * ============================================================
 * module init
 *
 * 和教材相比：
 *
 * 教材此处注册 cdev；
 * 我们注册 miscdevice。
 *
 * GPIO 初始化仍然保留在 open() 中，
 * 与教材一致。
 * ============================================================
 */
static int __init mykey_init(void)
{
    int ret;


    atomic_set(
        &keydev.keyvalue,
        0);


    ret =
        misc_register(
            &key_miscdev);


    if (ret < 0) {

        pr_err(
            "key: misc_register failed: %d\n",
            ret);

        return ret;
    }


    pr_info(
        "key: module loaded\n");


    return 0;
}


/*
 * ============================================================
 * module exit
 * ============================================================
 */
static void __exit mykey_exit(void)
{
    int i;


    misc_deregister(
        &key_miscdev);


    /*
     * 只有真正 open 过以后才申请了 GPIO。
     */
    if (g_flag) {

        for (i = 0; i < 8; i++)
            gpio_free(
                keydev.key_gpio[i]);


        if (gpio_111_104_v) {

            iounmap(
                gpio_111_104_v);

            gpio_111_104_v = NULL;
        }


        if (gpio_103_96_v) {

            iounmap(
                gpio_103_96_v);

            gpio_103_96_v = NULL;
        }


        g_flag = 0;
    }


    pr_info(
        "key: module unloaded\n");
}


module_init(mykey_init);
module_exit(mykey_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("yangjinrun");
MODULE_DESCRIPTION("Loongson 2K500 4x4 matrix keyboard driver");