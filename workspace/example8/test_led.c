#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{
    int fd;
    int retvalue;

    unsigned char databuf[3];


    if (argc != 2) {
        printf("Usage: %s 000~111\n",
               argv[0]);

        printf("example:\n");
        printf("  %s 000   all off\n",
               argv[0]);

        printf("  %s 111   all on\n",
               argv[0]);

        printf("  %s 100   green on\n",
               argv[0]);

        return -1;
    }


    if (strlen(argv[1]) != 3) {
        printf("parameter must contain 3 bits\n");

        return -1;
    }


    /*
     * 打开 LED 驱动。
     */
    fd = open("/dev/led",
              O_RDWR);

    if (fd < 0) {
        printf("file /dev/led open failed!\n");

        return -1;
    }


    /*
     * ASCII '0'/'1' 转数字 0/1。
     */
    databuf[0] =
        argv[1][0] - '0';

    databuf[1] =
        argv[1][1] - '0';

    databuf[2] =
        argv[1][2] - '0';


    printf(
        "databuf[0]:%d, "
        "databuf[1]:%d, "
        "databuf[2]:%d\n",
        databuf[0],
        databuf[1],
        databuf[2]);


    retvalue =
        write(fd,
              databuf,
              sizeof(databuf));


    if (retvalue < 0) {

        printf("LED Control Failed!\n");

        close(fd);

        return -1;
    }


    close(fd);

    return 0;
}