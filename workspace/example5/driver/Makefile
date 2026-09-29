obj-m :=ch422g.o

KDIR :=/home/user/loonarch/linux-5.10-2k500-cbd-src

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) modules clean