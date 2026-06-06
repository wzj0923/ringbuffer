obj-m := ringbuf_driver.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod ringbuf_driver.ko

remove:
	sudo rmmod ringbuf_driver

info:
	modinfo ringbuf_driver.ko

log:
	dmesg | tail -20
