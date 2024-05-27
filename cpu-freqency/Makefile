# Makefile to build kernel module and ncurses application

# Variables
KERNELDIR ?= /data/Sandbox/linux-stable
PWD := $(shell pwd)
MODULENAME := cpu-freqency

obj-m += cpu-freqency.o

# Default target
all: module

# Build the kernel module
module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

# Clean up
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -f $(MODULENAME).ko

.PHONY: all module clean
