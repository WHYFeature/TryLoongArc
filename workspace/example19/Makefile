ifneq ($(KERNELRELEASE),)
obj-m += irdrv.o
else

KDIR := /home/user/loonarch/linux-5.10-2k500-cbd-src
ARCH := loongarch
CROSS_COMPILE := loongarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc

.PHONY: all module user clean

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

user: ir_test

ir_test: ir_test.c ir_keymap.h
	$(CC) -O2 -Wall -Wextra -o $@ ir_test.c

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	$(RM) ir_test

endif
