#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    int fd;
    int ret;
    unsigned char ctl;

    if (argc != 2) {
        printf("Usage: %s 0|1\n", argv[0]);
        printf("  1 : beep on\n");
        printf("  0 : beep off\n");
        return -1;
    }

    if (argv[1][0] == '1')
        ctl = 1;
    else if (argv[1][0] == '0')
        ctl = 0;
    else {
        printf("parameter must be 0 or 1\n");
        return -1;
    }

    fd = open("/dev/beep", O_RDWR);

    if (fd < 0) {
        perror("open /dev/beep");
        return -1;
    }

    ret = write(fd, &ctl, 1);

    if (ret < 0) {
        perror("Beep Control Failed");
        close(fd);
        return -1;
    }

    printf("beep %s\n",
           ctl ? "ON" : "OFF");

    close(fd);

    return 0;
}