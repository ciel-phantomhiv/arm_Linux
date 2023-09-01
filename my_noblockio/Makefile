KERNELDIR := /home/xumouren/atk_stm32mp157/atk_5.4linux
CURRENT_PATH := $(shell pwd)

obj-m := noblockio.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean