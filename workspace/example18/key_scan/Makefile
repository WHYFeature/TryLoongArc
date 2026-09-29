KDIR := /home/user/loonarch/linux-5.10-2k500-cbd-src
ARCH := loongarch
CROSS_COMPILE := loongarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -O2 -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all clean
all: test_all

test_all: test_all.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f test_all
