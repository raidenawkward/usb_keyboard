##############################
#usbkdb Makefile for linux
##############################
obj-m := usb_keyboard.o
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules 
