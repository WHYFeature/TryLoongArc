#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


int main(int argc, char *argv[])
{
    int fd;

    int ret;


    char *filename =
        "/dev/aht20";


    /*
     * 教材：
     * 忽略第7字节 CRC。
     */
    unsigned char readByte[6];


    unsigned int H1 = 0;

    unsigned int T1 = 0;


    fd =
        open(
            filename,
            O_RDWR);


    if (fd < 0) {

        printf(
            "can't open file %s\n",
            filename);

        return -1;
    }


    printf(
        "AHT20 test start\n");


    while (1) {

        ret =
            read(
                fd,
                readByte,
                sizeof(readByte));


        if (ret < 0) {

            perror(
                "read AHT20 failed");

            sleep(2);

            continue;
        }


        /*
         * 调试时把原始 6 字节一起打印。
         */
        printf(
            "RAW: %02X %02X %02X %02X %02X %02X\n",
            readByte[0],
            readByte[1],
            readByte[2],
            readByte[3],
            readByte[4],
            readByte[5]);


        /*
         * 教材：
         *
         * 检查状态。
         */
        if ((readByte[0] & 0x68) == 0x08) {


            /*
             * ======================================
             * 湿度20 bit
             * ======================================
             */

            H1 =
                readByte[1];


            H1 =
                (H1 << 8) |
                readByte[2];


            H1 =
                (H1 << 8) |
                readByte[3];


            H1 =
                H1 >> 4;


            /*
             * 湿度保留1位小数：
             *
             * H = raw * 1000 / 2^20
             */
            H1 =
                (H1 * 1000) >>
                20;


            /*
             * ======================================
             * 温度20 bit
             * ======================================
             */

            T1 =
                readByte[3];


            /*
             * Byte3 低4位才属于温度。
             */
            T1 =
                T1 &
                0x0000000F;


            T1 =
                (T1 << 8) |
                readByte[4];


            T1 =
                (T1 << 8) |
                readByte[5];


            /*
             * 教材：
             *
             * T = raw * 200 / 2^20 - 50
             *
             * 为保存1位小数：
             *
             * raw * 2000 / 2^20 - 500
             */
            T1 =
                ((T1 * 2000) >>
                20) -
                500;


            printf(
                "Temp: %d%d.%d C\n",
                T1 / 100,
                (T1 / 10) % 10,
                T1 % 10);


            printf(
                "Hum : %d%d.%d %%RH\n",
                H1 / 100,
                (H1 / 10) % 10,
                H1 % 10);
        }
        else {

            printf(
                "AHT20 data read failed, status=0x%02X\n",
                readByte[0]);
        }


        sleep(2);
    }


    close(fd);


    return 0;
}