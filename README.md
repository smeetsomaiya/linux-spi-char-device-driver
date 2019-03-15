# SPI Device Programming and Pulse Measurement

ABOUT:

Part 1: A user application program for distance-controlled animation
Part 2: A device driver for SPI-based LED matrix

Code files:
		spi_display.c - The ledmatrix driver code
		main.c - Userspace code for Part 1
		main2.c - Userspace program for Part 2
		main.h - Header with all declarations and definitions
		Makefile - Commands to copy and install the files

PREREQUISITES:
Linux machine is used as host and galileo Gen2 board as target.
Linux Kernel of minimum v2.6.19 is required.
SDK:iot-devkit-glibc-x86_64-image-full-i586-toolchain-1.7.2.sh
GCC used i586-poky-linux-gcc
Bread-board, wires LEDMatrix Module and Ultrasonic sensor HC-SR04.

STEPS TO SETUP AND COMPILE:
This is how you can connect the ledmatrix and ultrasonic sensor to the Galileo:

		Ultrasonic sensor trigger pin - IO2
		Ultrasonic sensor echo pin - IO3
		Ledmatrix DIN - IO11 (MOSI)
		Ledmatrix CS - IO12(gpio15)
		Ledmatrix CLK - IO13(SCK)

STEPS TO COMPILE:
To cross-compile for the galileo:
Open the make file and change the path of SROOT. Point it to your i586-poky-linux-gcc
Also, change the IP address to that of the board in the “scp” block.
Navigate to the path where all the files are stored and open the terminal and type

		make all 
This will generate the executables (.ko and .o files), next:

		make scp
This will transfer the required files to the board and take you to the Galileo shell, where the module can be installed.

STEPS TO EXECUTE:
Once in the Galileo shell:

Select the path
		cd /media/realroot
To run Part 1:
		./main.o
Note: This assumes that the device driver spidev is installed at "/dev/spidev1.0"

To install the LedMatrix driver:
		rmmod spidev
		insmod spi_display.ko

To execute the program, after installing the driver(spi_display.ko) type:
		./main2.o

To capture the kernel log use: 
		cat /dev/kmsg
OR
		dmesg

EXPECTED OUTPUT:
The pattern is of a person walking/running.

-> Initially all the leds will glow till the time the ultrasonic sensor is initialized
-> Based on the distance, the speed and direction of the person will change.
->For closer distances, he'll move forward, with speed increasing as distance decreases.
->For distances > 100 cm, he'll move in the reverse direction, with speed increasing as distance decreases.
Note: Sometime, it may be hard to notice the difference in the speed.
-> Also, in part 2, the led's will be off initially until the sensor has been initialized.
-> (Part2) They will also be "ON" when distance is < 10 cms
-> (Part2) They will also be "OFF" when distance is < 10 cms