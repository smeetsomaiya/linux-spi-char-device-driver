#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>
#include<signal.h>
#include <inttypes.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include "rdtsc.h"

int gpio_export(unsigned int gpio);
int gpio_unexport(unsigned int gpio);
int gpio_set_dir(unsigned int gpio, unsigned int out_flag);
int gpio_set_value(unsigned int gpio, unsigned int value);
int gpio_get_value(unsigned int gpio, unsigned char* value);
int gpio_set_edge(unsigned int gpio, char *edge);
int gpio_fd_open(unsigned int gpio);
int gpio_fd_open_read(unsigned int gpio);
int gpio_fd_open_edge(unsigned int gpio);
int gpio_fd_close(int fd);
int mux_gpio_set(int gpio, int out_dir, int value);

#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64

#define ROW_SIZE(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))
#define COLUMN_SIZE(arr) ((int) sizeof ((arr)[0]) / sizeof (int))

#define BEGIN 11

pthread_t sensor_thread, display_thread;
pthread_mutex_t m1;

int exit_flag = 0;

int trigger_fd, echo_fd, echo_edge_fd;
unsigned int trigger_pin = 2, echo_pin = 3; //IO2 and 3
unsigned int mosi = 0, ss = 1, sck = 2; //Array index(s) corresponding to IO11, 12 and 13

// Configuration data array for settings of LED driver chip 
uint8_t config [] = 
{
	0x0F, 0x00,           // No display test
        0x0C, 0x01,           // Setting shutdown as Normal Operation
        0x09, 0x00,           //No decode mode required
        0x0A, 0x0F,           // Full intensity
        0x0B, 0x07,           // Scans for all rows
};

/* SPI variables */
struct spi_ioc_transfer tr = {0};  // initialising struct with 0 values

uint8_t tx_buffer[2];                // Transmit buffer
int spi_fd;



inline void IOSetup(void);
inline void IOSetup_spi(void);

struct pollfd PollEch = {0};

typedef struct {
	uint8_t led[26]; //1 byte represents 1 row of LEDs
} PATTERN;

PATTERN p0[] = {
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x00,
	0x04, 0x8B,
	0x05, 0x4B,
	0x06, 0x3F,
	0x07, 0x4B,
	0x08, 0x88
	}	
	},
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x13,
	0x04, 0xCB,
	0x05, 0x3F,
	0x06, 0xCB,
	0x07, 0x10,
	0x08, 0x00
	}	
	},
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x8B,
	0x03, 0x4B,
	0x04, 0x3F,
	0x05, 0x4B,
	0x06, 0x88,
	0x07, 0x00,
	0x08, 0x00
	}	
	},
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x13,
	0x03, 0xCB,
	0x04, 0x3F,
	0x05, 0xCB,
	0x06, 0x10,
	0x07, 0x00,
	0x08, 0x00,
	}	
	},
/*	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x8B,
	0x03, 0x4B,
	0x04, 0x3F,
	0x05, 0x4B,
	0x06, 0x88,
	0x07, 0x00,
	0x08, 0x00
	}	
	},
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x13,
	0x03, 0xCB,
	0x04, 0x3F,
	0x05, 0xCB,
	0x06, 0x10,
	0x07, 0x00,
	0x08, 0x00	
	}	
	},*/
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x03,
	0x02, 0x1B,
	0x03, 0xFF,
	0x04, 0x1B,
	0x05, 0x00,
	0x06, 0x00,
	0x07, 0x00,
	0x08, 0x00,
	}	
	}
};

PATTERN p1[] = {
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x88,
	0x02, 0x4B,
	0x03, 0x3F,
	0x04, 0x4B,
	0x05, 0x8B,
	0x06, 0x00,
	0x07, 0x00,
	0x08, 0x00,
	}
	},
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x10,
	0x03, 0xCB,
	0x04, 0x3F,
	0x05, 0xCB,
	0x06, 0x13,
	0x07, 0x00,
	0x08, 0x00,
	}	
	},
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x88,
	0x04, 0x4B,
	0x05, 0x3F,
	0x06, 0x4B,
	0x07, 0x8B,
	0x08, 0x00
	}	
	},
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x10,
	0x04, 0xCB,
	0x05, 0x3F,
	0x06, 0xCB,
	0x07, 0x13,
	0x08, 0x00
	}	
	},
/*	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x88,
	0x04, 0x4B,
	0x05, 0x3F,
	0x06, 0x4B,
	0x07, 0x8B,
	0x08, 0x00
	}	
	},
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x10,
	0x04, 0xCB,
	0x05, 0x3F,
	0x06, 0xCB,
	0x07, 0x13,
	0x08, 0x00
	}	
	}, */
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x00,
	0x04, 0x00,
	0x05, 0x1B,
	0x06, 0xFF,
	0x07, 0x1B,
	0x08, 0x03
	}	
	}
};

PATTERN off[] = {
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x00,
	0x04, 0x00,
	0x05, 0x00,
	0x06, 0x00,
	0x07, 0x00,
	0x08, 0x00
	}		
	}
};

PATTERN on[] = {
	{
	{
	0x0F, 0x00,           
        0x0C, 0x01,          
        0x09, 0x00,          
        0x0A, 0x0F,          
        0x0B, 0x07,

	0x01, 0xFF,
	0x02, 0xFF,
	0x03, 0xFF,
	0x04, 0xFF,
	0x05, 0xFF,
	0x06, 0xFF,
	0x07, 0xFF,
	0x08, 0xFF
	}		
	}
};


long distance = 0;
long distance_arr[4] = {0,0,0,0};

// Array containing the gpio pin configuration
int gpio_mapping_array[20][4] = {
			                                {11,32,100,100},
							{12,28,45,100},
							{13,34,77,100},
 							{14,16,76,64}
};

// Array containing the gpio pin configuration
int gpio_values_array[20][4] = {
			                               {100,0,100,100},
							{100,0,0,100},
							{100,0,0,100},
 							{100,1,0,0} //make 16 high for echo input
};

// Array containing the gpio pin configuration
int spi_mapping_array[20][4] = {
			                                {100,24,44,100}, //72-low
							{15,42,100,100},
							{100,30,46,100}
};

// Array containing the gpio pin configuration
int spi_values_array[20][4] = {
			                               {100,0,1,0},
							{0,0,100,100},
							{100,0,1,100},
};

/****************************************************************
 * gpio_export
 ****************************************************************/
int gpio_export(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
 
	return 0;
}

/****************************************************************
 * gpio_unexport
 ****************************************************************/
int gpio_unexport(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_set_dir
 ****************************************************************/
int gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
	int fd;
	char buf[MAX_BUF];
 
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/direction");
		return fd;
	}
 
	if (out_flag)
		write(fd, "out", 3);
	else
		write(fd, "in", 2);
 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_set_value
 ****************************************************************/
int gpio_set_value(unsigned int gpio, unsigned int value)
{
	int fd;
	char buf[MAX_BUF];
 
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		printf("No such file gpio%d/value", gpio);
		return fd;
	}
 
	if (value)
		write(fd, "1", 1);
	else
		write(fd, "0", 1);
 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_get_value
 ****************************************************************/
int gpio_get_value(unsigned int gpio, unsigned char *value)
{
	int fd;
	char buf[MAX_BUF];
	char ch;

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		perror("gpio/get-value");
		return fd;
	} else {
		printf("Reading\n");	
	}
 
	read(fd, &ch, 1);
	if (ch != '0') {
		*value = 1;
	} else {
		*value = 0;
	}
	printf("After Reading %d\n", *value); 
	close(fd);
	return *value;
}


/****************************************************************
 * gpio_set_edge
 ****************************************************************/

int gpio_set_edge(unsigned int gpio, char *edge)
{
	int fd;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-edge");
		return fd;
	}
 
	write(fd, edge, strlen(edge) + 1); 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_fd_open  for value
 ****************************************************************/

int gpio_fd_open(unsigned int gpio)
{
	int fd;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_RDONLY | O_WRONLY| O_NONBLOCK );
	if (fd < 0) {
		perror("gpio/fd_open");
	}
	return fd;
}

/****************************************************************
 * gpio_fd_open_read  for value
 ****************************************************************/

int gpio_fd_open_read(unsigned int gpio)
{
	int fd;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_RDONLY | O_NONBLOCK );
	if (fd < 0) {
		perror("gpio/fd_open");
	}
	return fd;
}
/****************************************************************
 * gpio_fd_open_edge
 ****************************************************************/

int gpio_fd_open_edge(unsigned int gpio)
{
	int fd;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);
 
	fd = open(buf, O_RDONLY | O_WRONLY | O_NONBLOCK );
	if (fd < 0) {
		perror("gpio/fd_open_edge");
	}
	return fd;
}

/****************************************************************
 * gpio_fd_close
 ****************************************************************/

int gpio_fd_close(int fd)
{
	return close(fd);
}

int mux_gpio_set(int gpio, int out_dir, int value)   
{	
	gpio_export(gpio);
	if(gpio < 64 || gpio > 79)
		gpio_set_dir(gpio, out_dir);

	if(value != 100)
		gpio_set_value(gpio, value);

	return 0;
}

