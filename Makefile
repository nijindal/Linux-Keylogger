obj-m+= my_keyboard.o

KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all :
	echo $(PWD)
	make -C $(KDIR) M=$(PWD) modules
# 	make -C /lib/modules/2.6.31-generic/build M=$(PWD) modules
	rm -rf *.o *.out *.mod.c *.order *.symvers *.markers

clean :
	rm -rf *.o *.out *.ko *.mod.c *.order *.symvers *.c~ *.markers

