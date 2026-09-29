#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>


/*
 * “科”
 *
 * 数据来自教材第 6 章。
 */
const unsigned char ke[] = {
    0x10,0x08,0xB8,0x08,
    0x0F,0x09,0x08,0x09,
    0x08,0x08,0xBF,0x08,
    0x08,0x09,0x1C,0x09,
    0x2C,0x08,0x0A,0x78,
    0xCA,0x0F,0x09,0x08,
    0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08
};


/*
 * “技”
 *
 * 数据来自教材第 6 章。
 */
const unsigned char ji[] = {
    0x08,0x04,0x08,0x04,
    0x08,0x04,0xC8,0x7F,
    0x3D,0x04,0x08,0x04,
    0x08,0x04,0xA8,0x3F,
    0x18,0x21,0x0C,0x11,
    0x0B,0x12,0x08,0x0A,
    0x08,0x04,0x08,0x0A,
    0x8A,0x11,0x64,0x60
};


int main(int argc, char **argv)
{
    int aip_fd;
    int ret;

    aip_fd = open("/dev/aip", O_RDWR);

    printf("test: aip_fd=%d\n", aip_fd);

    if (aip_fd == -1) {

        perror("test: Error Opening aip1944");

        return -1;
    }


    /*
     * 显示“科”
     */
    ret = write(aip_fd, ke, sizeof(ke));

    if (ret < 0) {

        perror("write ke failed");

        close(aip_fd);

        return -1;
    }

    printf("display: 科\n");

    sleep(3);


    /*
     * 显示“技”
     */
    ret = write(aip_fd, ji, sizeof(ji));

    if (ret < 0) {

        perror("write ji failed");

        close(aip_fd);

        return -1;
    }

    printf("display: 技\n");

    sleep(3);


    close(aip_fd);

    return 0;
}