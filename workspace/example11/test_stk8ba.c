#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


short axis_out_encode(unsigned char out1,
                      unsigned char out2)
{
    short val;

    val =
        (out1 >> 4) |
        (out2 << 4);


    /*
     * 12 位补码符号扩展。
     */
    if (val & (1 << 11)) {

        val |= 0xF000;
    }


    return val;
}


int main(int argc, char *argv[])
{
    int fd;

    char *filename =
        "/dev/stk8ba";


    unsigned char databuf[6];

    int ret = 0;


    unsigned char xout1;
    unsigned char xout2;

    unsigned char yout1;
    unsigned char yout2;

    unsigned char zout1;
    unsigned char zout2;


    short x;
    short y;
    short z;


    int t = 20;


    fd = open(
        filename,
        O_RDWR);


    if (fd < 0) {

        printf(
            "can't open file %s\n",
            filename);

        return -1;
    }


    while (t--) {

        ret =
            read(
                fd,
                databuf,
                sizeof(databuf));


        /*
         * 教材驱动的 read()
         * 成功时返回 copy_to_user() 的返回值，
         * 因此成功为 0。
         */
        if (ret == 0) {

            xout1 = databuf[0];
            xout2 = databuf[1];

            yout1 = databuf[2];
            yout2 = databuf[3];

            zout1 = databuf[4];
            zout2 = databuf[5];


            x =
                axis_out_encode(
                    xout1,
                    xout2);

            y =
                axis_out_encode(
                    yout1,
                    yout2);

            z =
                axis_out_encode(
                    zout1,
                    zout2);
        }


        printf(
            "x:%d, y:%d, z:%d\n",
            x,
            y,
            z);


        /*
         * 教材：500ms
         */
        usleep(500000);
    }


    close(fd);


    return 0;
}