#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <unistd.h>
#include <linux/kernel.h>

#include "main.h"


#define SPI_DEVICE "/dev/LEDMATRIX"



uint8_t seq_0[2] = {0,0};
//uint8_t seq_1[22] = {0, 5, 1, 5, 2, 5, 3, 5, 4, 5, 5, 5, 6, 5, 7, 5, 8, 5, 0, 0};
uint8_t seq_1[22] = {0, 50, 1, 40, 2, 40, 3, 40, 4, 30, 5, 40, 6, 50, 7, 20, 8, 20, 0, 0};
uint8_t seq_2[22] = {0, 250, 1, 250, 2, 250, 3, 250, 4, 250, 5, 250, 6, 250, 7, 250, 8, 250, 9, 250, 0, 0};
uint8_t seq_3[22] = {9, 250, 8, 250, 7, 250, 6, 250, 5, 250, 4, 250, 3, 250, 2, 250, 1, 250, 0, 250, 0, 0};
uint8_t seq_order[22] = {0, 100, 1, 200, 2, 250, 3, 100, 4, 200, 5, 150, 6, 200, 7, 250, 8,250,9, 200, 0,0};




uint8_t animation_arr[10][8] = {
{0x00,0x00,0x00,0x8B,0x4B,0x3F,0x4B,0x88},
{0x00,0x00,0x13,0xCB,0x3F,0xCB,0x10,0x00},
{0x00,0x8B,0x4B,0x3F,0x4B,0x88,0x00,0x00},
{0x00,0x13,0xCB,0x3F,0xCB,0x10,0x00,0x00},
{0x03,0x1B,0xFF,0x1B,0x00,0x00,0x00,0x00},
{0x88,0xFB,0x3F,0x4B,0x8B,0x00,0x00,0x00},
{0x00,0x10,0xCB,0x3F,0xCB,0x13,0x00,0x00},
{0x00,0x00,0x88,0x4B,0x3F,0x4B,0x8B,0x00},
{0x00,0x00,0x10,0xCB,0x3F,0xCB,0x13,0x00},
{0x00,0x00,0x00,0x00,0x1B,0xFF,0x1B,0x03},

};

/*
 Signal handler to terminate the sensor thread
*/
static void sighandler_sensor(int signum) {
	if(signum == SIGINT) {
		pthread_cancel(sensor_thread);
	}
}

/*
 Signal handler to terminate the display thread
*/
static void sighandler_display(int signum) {
	if(signum == SIGINT) {
		pthread_cancel(display_thread);
	}
}

void configure_gpio_pins(int gpio_array_row, int is_trigger)
{
    int column;
    int columns_of_gpio_array = COLUMN_SIZE(gpio_mapping_array);

    for(column = 0; column < columns_of_gpio_array; column++) {
        int read_value = gpio_mapping_array[gpio_array_row][column];
        if(read_value != 100) {
            printf("\n\n Configure pins for IO %d - %d\n\n", gpio_array_row, gpio_mapping_array[gpio_array_row][column]);
            if(column == 0) {
		//output for trigger pins and input for echo pins
                mux_gpio_set(gpio_mapping_array[gpio_array_row][column], is_trigger, gpio_values_array[gpio_array_row][column]);
	    } else {
                mux_gpio_set(gpio_mapping_array[gpio_array_row][column], 1, gpio_values_array[gpio_array_row][column]);
	    }
        }
    }
}

void IOSetup(void)
{

	char buf[MAX_BUF];

	configure_gpio_pins(trigger_pin, 1); //Trigger pin IO2
	configure_gpio_pins(echo_pin, 0); //Echo pin on IO3
		
	/* Open the edge and value files */

	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio_mapping_array[trigger_pin][0]);
	trigger_fd = open(buf, O_WRONLY);
	printf("Open value file trigger %s %d\n",buf, trigger_fd);
	memset(buf, '\0', sizeof(buf));

	/* Open the echo pin value file */
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio_mapping_array[echo_pin][0]);
	echo_fd = open(buf, O_RDWR);
	printf("Open value file echo %s %d\n",buf, echo_fd);
	memset(buf, '\0', sizeof(buf));

	
	sprintf(buf, "/sys/class/gpio/gpio%d/edge", gpio_mapping_array[echo_pin][0]);
	echo_edge_fd = open(buf, O_WRONLY);
	printf("Open edge file echo %s %d\n",buf, echo_fd);
	memset(buf, '\0', sizeof(buf));

    	/* Prepare poll fd structure */
    	PollEch.fd = echo_fd;
    	PollEch.events = POLLPRI|POLLERR;
	PollEch.revents = 0;
}


void send_trigger() {
	write(trigger_fd,"0",1);
	write(trigger_fd,"1",1);
	usleep(12);
	write(trigger_fd,"0",1);
}


long ping_sensor() {
	int res;
	char* ReadValue = (char*) malloc(2);
	unsigned long long time0 = 0, time1 = 0;
	long distance_cm;

	time1 = 0;
	time0 = 0;
	res = lseek(echo_fd,0,SEEK_SET);
	if(res < 0)                             //lseek failed
		printf("lseek %d\n", res);

	res = write(echo_edge_fd,"rising",6);
	//		printf("Write rising %d\n", res);
	send_trigger();
	/* Start polling for the rising edge now */
	//		printf("Start polling for the rising edge now\n");
	poll(&PollEch,1,2000);
	if (PollEch.revents & POLLPRI)
	{	
			time0 = rdtsc();
			res = read(echo_fd, ReadValue, 1);	// read in characters
	//				printf("Read rising %c\n", *ReadValue);
			PollEch.revents = 0;
	lseek(echo_fd,0,SEEK_SET);
	//		if(res < 0)                             
	//			printf("lseek %d\n", res);

	write(echo_edge_fd,"falling",7);

	/* Start polling for the faling edge now */
	poll(&PollEch,1,2000);
	if (PollEch.revents & POLLPRI)
	{	
		time1 = rdtsc();
		res = read(echo_fd, ReadValue, 1);	// read in characters
	//			printf("Read falling %c\n", *ReadValue);
		PollEch.revents = 0;
	}
	else
	{
//		printf("\nMove at least 10 cms away\n");
//		time1 = time0;
//		lseek(echo_fd, 0, SEEK_SET); 		// rest to the starting position of the file
	//			goto restart;
	}


	}
//	else
//	{
//		printf("\nError detecting rising edge %d\n", PollEch.revents);
	//			lseek(echo_fd, 0, SEEK_SET); 		// rest to the starting position of the file
//	}


	distance_cm = ((long)(time1 - time0)*340)/(4000000*2);  // Distance calculations in cm
	if(distance_cm < 0) distance_cm = 0;
	return distance_cm;
}

void* sensor_function(void *arguments) {
	int tmp;
	IOSetup();
	while(1) {
		for(tmp = 0; tmp < 4; tmp++) {
			//Take readings every 250ms and avg them
			distance_arr[tmp] = ping_sensor();
//			printf("Raw Distance %ld cm\t", distance_arr[tmp]);
			usleep(75000);
		}
		pthread_mutex_lock(&m1);
		distance = (distance_arr[0] + distance_arr[1] + distance_arr[2] + distance_arr[3])/4;
		pthread_mutex_unlock(&m1);
		printf("Filtered Distance %ld cm\n", distance);
	}
}


void* led_display();

//pthread_t display_thread, sensor_thread;

int main ()
{
	pthread_mutex_init(&m1, NULL);
	
	pthread_create(&sensor_thread,NULL,sensor_function,NULL);
	pthread_create(&display_thread,NULL,&led_display,NULL);

	sleep(60); //Run program for 60 secs

	signal(SIGINT, sighandler_sensor);
	pthread_kill(sensor_thread, SIGINT);
	pthread_join(sensor_thread,NULL);

	signal(SIGINT, sighandler_display);
	pthread_kill(display_thread, SIGINT);
	pthread_join(display_thread,NULL);
	return 0;
}


void* led_display() {

	int err,fd;
	int i = 0, dist_cm = 0;
	fd =  open(SPI_DEVICE,O_RDWR);
	if(fd<0) {
		printf("\nDevice not found");
	}
	else
	{
		printf("\nDevice opened successfully");
	}
	err=ioctl(fd,(unsigned long int)animation_arr,BEGIN);
	if(err<0) {
		printf("\nError in ioctl");

	}
 
	while(1) {
		while(1)
		{
			pthread_mutex_lock(&m1);
			dist_cm = distance;
			pthread_mutex_unlock(&m1);

			if(dist_cm <= 10) {
				err= write(fd,(void*)seq_0,sizeof(seq_0));
				printf("Move more than 10cms away or wait for sensor to initialize\n");
			} else if(dist_cm > 10 && dist_cm <= 50) {
				err= write(fd,(void*)seq_1,sizeof(seq_1));
			} else if(dist_cm > 50 && dist_cm <= 100) {
				err= write(fd,(void*)seq_2,sizeof(seq_2));
			} else {
				err= write(fd,(void*)seq_3,sizeof(seq_3));
			}
			if(err > 0)
			{
				printf("write successful%d\n",err);
				break;
			}
			usleep(50000);
		}
	}

	printf("Closing spi_display\n");
	i = close(fd);
	perror("close");
	printf("Closed spi_display %d\n", i);
	return 0;
}
   
  

