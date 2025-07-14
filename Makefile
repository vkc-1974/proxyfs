obj-m := \
	proxyfs.o

proxyfs-objs := \
	proxyfs-dir-inode-ops.o \
	proxyfs-file-ops.o \
	proxyfs-main.o \
	proxyfs_buffer_pool.o \
	proxyfs_socket.o \
	proxyfs_procfs.o

ccflags-y += -g -Og

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
