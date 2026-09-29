obj-m += aip1944.o

KDIR := /home/user/loonarch/linux-5.10-2k500-cbd-src
PWD  := $(shell pwd)

ARCH := loongarch
CROSS_COMPILE := loongarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	$(CROSS_COMPILE)gcc -O2 test_aip1944.c -o test_aip1944

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_aip1944