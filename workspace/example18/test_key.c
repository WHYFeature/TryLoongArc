#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


int main(int argc, char *argv[])
{
    int fd;
    int ret;

    int i;

    int keyvalue[8];

    unsigned char databuf[4];


    if (argc != 2) {

        printf(
            "Usage: %s 1000|0100|0010|0001\n",
            argv[0]);

        return -1;
    }


    /*
     * 必须恰好 4 位。
     */
    if (strlen(argv[1]) != 4) {

        printf(
            "parameter must contain exactly 4 bits\n");

        return -1;
    }


    /*
     * 只允许 0 和 1。
     */
    for (i = 0; i < 4; i++) {

        if (argv[1][i] != '0' &&
            argv[1][i] != '1') {

            printf(
                "parameter must contain only 0 or 1\n");

            return -1;
        }


        databuf[i] =
            argv[1][i] - '0';
    }


    /*
     * 打开教材规定的设备节点。
     */
    fd =
        open(
            "/dev/key",
            O_RDWR);


    if (fd < 0) {

        perror(
            "open /dev/key");

        return -1;
    }


    /*
     * 设置 GPIO107~110。
     */
    ret =
        write(
            fd,
            databuf,
            sizeof(databuf));


    if (ret < 0) {

        perror(
            "Key Control Failed");

        close(fd);

        return -1;
    }


    printf(
        "column output: "
        "GPIO107=%d GPIO108=%d GPIO109=%d GPIO110=%d\n",
        databuf[0],
        databuf[1],
        databuf[2],
        databuf[3]);


    printf(
        "Press keys, Ctrl+C to stop.\n");


    /*
     * 循环读取 GPIO。
     */
    while (1) {

        ret =
            read(
                fd,
                keyvalue,
                sizeof(keyvalue));


        if (ret < 0) {

            perror(
                "read /dev/key");

            break;
        }


        printf(
            "GPIO103~110 = "
            "%d-%d-%d-%d-%d-%d-%d-%d\r",
            keyvalue[0],
            keyvalue[1],
            keyvalue[2],
            keyvalue[3],
            keyvalue[4],
            keyvalue[5],
            keyvalue[6],
            keyvalue[7]);


        fflush(stdout);


        /*
         * 100 ms，避免疯狂刷屏。
         * 教材最后是 sleep(1)，
         * 这里稍快一点，更容易观察按键。
         */
        usleep(100000);
    }


    close(fd);

    return 0;
}