#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

const unsigned char BCD_decode_tab[29] = {
    0X3F, 0X06, 0X5B, 0X4F,
    0X66, 0X6D, 0X7D, 0X07,
    0X7F, 0X6F, 0X77, 0X7C,
    0X58, 0X5E, 0X79, 0X71,
    0x00, 0x46, 0x40, 0x41,
    0x39, 0x0F, 0x08, 0x76,
    0x38, 0x73, 0x80, 0xFF,
    0x00
};

unsigned char databuf[5];

void set_digital_tube(int fd, int count)
{
    int i;

    for(i = 4; i >= 1; i--)
    {
        databuf[i] = BCD_decode_tab[count % 10];
        count /= 10;
    }

    write(fd, databuf, sizeof(databuf));
}


int main(void)
{
    int fd;
    int count;

    fd = open("/dev/ch422g", O_RDWR);

    printf("test: fd=%d\n", fd);

    if(fd == -1)
    {
        printf("test: Error Opening ch422g\n");
        return -1;
    }

    databuf[0] = 0;

    /*
     * 教材要求实现秒表功能。
     * 这里先从 0000 数到 0030 验证。
     */
    for(count = 0; count <= 30; count++)
    {
        set_digital_tube(fd, count);
        sleep(1);
    }

    close(fd);

    return 0;
}