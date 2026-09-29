#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define MAGIC_NUMBER 'k'

#define CMD_standby  _IO(MAGIC_NUMBER, 1)
#define CMD_forward  _IO(MAGIC_NUMBER, 2)
#define CMD_backward _IO(MAGIC_NUMBER, 3)
#define CMD_brake    _IO(MAGIC_NUMBER, 4)

int main(int argc, char **argv)
{
    int fd;
    unsigned long cmd;

    if (argc != 2) {
        printf("Usage: %s 0|1|2|3\n", argv[0]);
        printf("  0: standby\n");
        printf("  1: forward\n");
        printf("  2: backward\n");
        printf("  3: brake\n");
        return -1;
    }

    fd = open("/dev/motor", O_RDWR);

    if (fd < 0) {
        perror("open /dev/motor");
        return -1;
    }

    switch (atoi(argv[1])) {
    case 0:
        cmd = CMD_standby;
        break;

    case 1:
        cmd = CMD_forward;
        break;

    case 2:
        cmd = CMD_backward;
        break;

    case 3:
        cmd = CMD_brake;
        break;

    default:
        printf("Invalid argument: %s\n", argv[1]);
        close(fd);
        return -1;
    }

    printf("send cmd = 0x%lx\n", cmd);

    if (ioctl(fd, cmd) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}