ifneq ($(KERNELRELEASE),)
rc522-objs := rc522_drv.o rc522_core.o
obj-m += rc522.o
else

KDIR := /home/user/loonarch/linux-5.10-2k500-cbd-src
ARCH := loongarch
CROSS_COMPILE := loongarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc

.PHONY: all module user clean

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

user: rc522_test

rc522_test: rc522_test.c rc522_uapi.h
	$(CC) -O2 -Wall -Wextra -o $@ rc522_test.c

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	$(RM) rc522_test

endif
