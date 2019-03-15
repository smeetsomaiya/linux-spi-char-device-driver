APP = main.o
APP2 = main2.o
SROOT = /opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux/i586-poky-linux-gcc
SRC = /opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel
obj-m:= spi_display.o

all:
	make -C $(SRC) M=$(PWD) modules -Wall
	$(SROOT) -pthread -Wall -o $(APP2) main2.c
	$(SROOT) -pthread -Wall -o $(APP) main.c

scp:
	scp spi_display.ko root@192.168.1.6:/media/realroot
	scp main2.o root@192.168.1.6:/media/realroot
	scp main.o root@192.168.1.6:/media/realroot
	ssh root@192.168.1.6

clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP)
