#include <stdio.h>
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
    int fd, ret;

    fd = open("/dev/motor", O_RDWR);

    printf("test: fd=%d\n", fd);

    if(fd == -1) {
        printf("test: Error Opening motor\n");
        return -1;
    }

    ioctl(fd, CMD_forward);
    sleep(3);

    ioctl(fd, CMD_standby);
    sleep(3);

    ioctl(fd, CMD_backward);
    sleep(3);

    ioctl(fd, CMD_brake);
    sleep(3);

    close(fd);

    return 0;
}