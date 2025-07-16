obj-m := \
	proxyfs.o

proxyfs-objs := \
	proxyfs-mapping-ops.o \
	proxyfs-main.o \
	proxyfs-buffer-pool.o \
	proxyfs-context.o \
	proxyfs-dentry-ops.o \
	proxyfs-super-block.o \
	proxyfs-super-ops.o \
	proxyfs-inode-ops.o \
	proxyfs-file-ops.o \
	proxyfs-buffer-pool.o \
	proxyfs-socket.o \
	proxyfs-procfs.o

ccflags-y += -g -Og

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
