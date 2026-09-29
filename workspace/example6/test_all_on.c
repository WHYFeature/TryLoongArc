#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
    int fd;
    unsigned char all_on[32];
    int i;

    for (i = 0; i < 32; i++)
        all_on[i] = 0xFF;

    fd = open("/dev/aip", O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (write(fd, all_on, sizeof(all_on)) < 0) {
        perror("write");
        close(fd);
        return -1;
    }

    printf("sent 32 bytes 0xFF\n");

    sleep(10);

    close(fd);
    return 0;
}