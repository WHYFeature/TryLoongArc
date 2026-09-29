#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Compatible with the existing key.ko: write 4 bytes; read 8 ints. */
_Static_assert(sizeof(int) == 4, "key.ko requires 32-bit int");
static volatile sig_atomic_t running = 1;
static unsigned int position[16]; /* physical position -> electrical bit */
static uint16_t candidate, stable;
static uint64_t changed_at;

static void on_signal(int sig) { (void)sig; running = 0; }
static uint64_t now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}
static void pause_us(long us)
{
    struct timespec t = { us / 1000000, (us % 1000000) * 1000 };
    while (nanosleep(&t, &t) < 0 && errno == EINTR && running) {}
}
static int set_columns(int fd, const unsigned char cols[4])
{
    ssize_t n;
    do { n = write(fd, cols, 4); } while (n < 0 && errno == EINTR && running);
    if (n != 4) { if (n >= 0) errno = EIO; return -1; }
    return 0;
}
static int all_low(int fd)
{
    const unsigned char cols[4] = {0, 0, 0, 0};
    return set_columns(fd, cols);
}
static int scan(int fd, uint16_t *bits)
{
    unsigned char cols[4] = {0, 0, 0, 0};
    int values[8];
    *bits = 0;
    for (int c = 0; c < 4; ++c) {
        /* Break before make: never leave the previous column selected. */
        if (all_low(fd) < 0) return -1;
        cols[c] = 1;
        if (set_columns(fd, cols) < 0) return -1;
        pause_us(1000);
        ssize_t n;
        do { n = read(fd, values, sizeof(values)); }
        while (n < 0 && errno == EINTR && running);
        if (n != (ssize_t)sizeof(values)) {
            if (n >= 0) errno = EIO;
            return -1;
        }
        for (int r = 0; r < 4; ++r) {
            if (values[r] != 0 && values[r] != 1) { errno = EPROTO; return -1; }
            if (values[r]) *bits |= (uint16_t)(1u << (r * 4 + c));
        }
        cols[c] = 0;
    }
    return all_low(fd);
}
static int sample(int fd)
{
    uint16_t raw;
    if (scan(fd, &raw) < 0) return -1;
    uint64_t t = now_ms();
    if (raw != candidate) { candidate = raw; changed_at = t; }
    if (t - changed_at >= 30) stable = candidate;
    pause_us(5000);
    return 0;
}
static int wait_release(int fd)
{
    uint64_t since = 0;
    while (running) {
        if (sample(fd) < 0) return -1;
        if (!candidate && !stable) {
            if (!since) since = now_ms();
            if (now_ms() - since >= 100) return 0;
        } else since = 0;
    }
    return 1;
}
static int read_map(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return errno == ENOENT ? 1 : -1;
    unsigned int seen = 0;
    for (int i = 0; i < 16; ++i) {
        if (fscanf(f, "%u", &position[i]) != 1 || position[i] >= 16 ||
            (seen & (1u << position[i]))) { fclose(f); errno = EINVAL; return -1; }
        seen |= 1u << position[i];
    }
    char extra;
    if (fscanf(f, " %c", &extra) == 1) { fclose(f); errno = EINVAL; return -1; }
    fclose(f);
    return 0;
}
static int calibrate(int fd, const char *path)
{
    unsigned int seen = 0;
    puts("Calibration: press ONE key at a time, top-to-bottom, left-to-right.");
    puts("Release all keys first. Hold each requested key until accepted.");
    for (int i = 0; i < 16 && running; ) {
        int rc = wait_release(fd);
        if (rc) return rc;
        printf("Press physical row %d, column %d ...\n", i / 4 + 1, i % 4 + 1);
        while (running) {
            if (sample(fd) < 0) return -1;
            if (!stable) continue;
            if ((stable & (stable - 1)) != 0) {
                puts("Multiple intersections detected. Release and retry.");
                break;
            }
            unsigned int bit = 0;
            while (!(stable & (1u << bit))) ++bit;
            if (seen & (1u << bit)) {
                puts("Already assigned. Release and press the requested key.");
                break;
            }
            position[i] = bit;
            seen |= 1u << bit;
            printf("Accepted: row GPIO%u, column GPIO%u. Release.\n",
                   103 + bit / 4, 107 + bit % 4);
            ++i;
            break;
        }
    }
    if (!running) return 1;
    int rc = wait_release(fd);
    if (rc) return rc;
    /* Atomic replacement; leave the old map intact if saving fails. */
    char *tmp = malloc(strlen(path) + 12);
    if (!tmp) return -1;
    sprintf(tmp, "%s.tmpXXXXXX", path);
    int out = mkstemp(tmp);
    if (out < 0) { free(tmp); return -1; }
    FILE *f = fdopen(out, "w");
    if (!f) { close(out); unlink(tmp); free(tmp); return -1; }
    int failed = 0;
    for (int i = 0; i < 16; ++i)
        if (fprintf(f, "%u%c", position[i], i % 4 == 3 ? '\n' : ' ') < 0) failed = 1;
    if (fclose(f)) failed = 1;
    if (!failed && rename(tmp, path)) failed = 1;
    if (failed) { unlink(tmp); free(tmp); errno = EIO; return -1; }
    free(tmp);
    printf("Saved mapping: %s\nRun ./test_all to scan.\n", path);
    return 0;
}
static void show(uint16_t bits, uint64_t elapsed)
{
    printf("\n[t=%llu ms]  1=pressed, 0=released\n", (unsigned long long)elapsed);
    puts("      C1 C2 C3 C4");
    for (int r = 0; r < 4; ++r) {
        printf("R%d    ", r + 1);
        for (int c = 0; c < 4; ++c)
            printf("%u  ", !!(bits & (1u << position[r * 4 + c])));
        putchar('\n');
    }
    printf("Pressed:");
    if (!bits) printf(" none");
    for (int i = 0; i < 16; ++i)
        if (bits & (1u << position[i])) printf(" R%dC%d", i / 4 + 1, i % 4 + 1);
    putchar('\n');
}
int main(int argc, char **argv)
{
    int calibration = 0;
    unsigned long interval = 500;
    const char *map = getenv("KEY_MAP");
    if (!map || !*map) map = "keymap.conf";
    if (argc == 2 && !strcmp(argv[1], "--calibrate")) calibration = 1;
    else if (argc == 2) {
        char *end;
        errno = 0;
        interval = strtoul(argv[1], &end, 10);
        if (errno || !*argv[1] || *end || interval < 100 || interval > 60000) {
            fprintf(stderr, "Interval must be 100..60000 ms.\n"); return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [interval_ms | --calibrate]\n", argv[0]); return 1;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    if (!calibration) {
        int rc = read_map(map);
        if (rc < 0) { perror("read key map"); return 1; }
        if (rc == 1) {
            fprintf(stderr, "No position map: run ./test_all --calibrate first.\n");
            return 1;
        }
    }
    int fd = open("/dev/key", O_RDWR);
    if (fd < 0) { perror("open /dev/key"); return 1; }
    int status = 0;
    if (calibration) {
        if (calibrate(fd, map) < 0) { perror("calibration"); status = 1; }
    } else {
        printf("4x4 scan: interval=%lu ms; Ctrl+C to stop. Single-key use.\n", interval);
        uint64_t start = now_ms(), next = start + interval;
        while (running) {
            if (sample(fd) < 0) { perror("scan /dev/key"); status = 1; break; }
            uint64_t t = now_ms();
            if (t >= next) { show(stable, t - start); next = t + interval; }
        }
    }
    if (all_low(fd) < 0) { perror("reset columns"); status = 1; }
    close(fd);
    puts(status ? "Stopped with error; check messages above." : "Stopped; columns are low.");
    return status;
}
