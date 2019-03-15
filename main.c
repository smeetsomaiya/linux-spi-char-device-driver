#include "main.h"

int is_gpio_64_to_79(int gpio) { //Check for gpio's which do not have a direction file
	if(gpio >= 64 && gpio <= 79) {
		return 1; //return true
	}	
	return 0; //return false
}


void configure_spi_pins(int spi_array_row)
{                    
    int column;
    int columns_of_spi_array = COLUMN_SIZE(spi_mapping_array);

    for(column = 0; column < columns_of_spi_array; column++) {
        int read_value = spi_mapping_array[spi_array_row][column];
        if(read_value != 100) {
		//output for trigger pins and input for echo pins
		mux_gpio_set(spi_mapping_array[spi_array_row][column], 1, spi_values_array[spi_array_row][column]);
	}
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

void send_config_command() {
	int i, err;
	tr.tx_buf = (unsigned long)tx_buffer;  //Set transmit buffer
	tr.rx_buf = 0;                      //No data is to be received
	tr.len = 2;                         //Number of bytes of data in one transfer
	tr.delay_usecs = 1;                 //Delay in us
	tr.speed_hz = 10000000;             //Bus speed in Hz 
	tr.bits_per_word = 8;               //Number of bits in each word
	tr.cs_change = 1;                   //Changing chip select line value between transfers

	//Sending configuration data to initialize the LedMatrix
	for(i = 0; i < 10 ; i += 2) {
		tx_buffer[0] = config[i];
		tx_buffer[1] = config[i+1];

		gpio_set_value(spi_mapping_array[ss][0], 0);

		err = ioctl(spi_fd, SPI_IOC_MESSAGE (1), &tr);
		if(err < 0)
			printf("\nioctl message transfer %d\n", err);

		usleep(1000);
		gpio_set_value(spi_mapping_array[ss][0], 1);
	}
}

void spi_transfer(uint8_t tx_arr[]) {
	int i, err;
	for(i = 0; i < 26; i+=2) {
		tx_buffer[0] = tx_arr[i];
		tx_buffer[1] = tx_arr[i+1];
//		printf("Tx_buffer %x %x", tx_buffer[0],tx_buffer[1]);
		gpio_set_value(spi_mapping_array[ss][0], 0);

		err = ioctl(spi_fd, SPI_IOC_MESSAGE (1), &tr);
		if(err < 0)
			printf("\nioctl message transfer %d Count %d\n", err, i);

		usleep(1000);
		gpio_set_value(spi_mapping_array[ss][0], 1);
	}
}


void IOSetup_spi() {
	configure_spi_pins(mosi); //Trigger pin IO11
	configure_spi_pins(ss); //Trigger pin IO12
	configure_spi_pins(sck); //Trigger pin IO13

	spi_fd = open("/dev/spidev1.0", O_RDWR);
	if (spi_fd<=0) { 
		printf("Device not found\n");
		exit(-1);
	}

	send_config_command();
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
	}
	else
	{
		printf("\nError detecting rising edge %d\n", PollEch.revents);
	//			lseek(echo_fd, 0, SEEK_SET); 		// rest to the starting position of the file
	}

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
		time1 = time0;
//		printf("\nMove at least 10 cms away\n");
//		lseek(echo_fd, 0, SEEK_SET); 		// rest to the starting position of the file
	//			goto restart;
	}

	distance_cm = ((long)(time1 - time0)*340)/(4000000*2);  // Distance calculations in cm
	if(distance_cm < 0) distance_cm = 0;
	return distance_cm;
}

void* sensor_function(void *arguments) {
	int tmp;

	while(1) {
		for(tmp = 0; tmp < 4; tmp++) {
			//Take readings every 30ms and avg them
			distance_arr[tmp] = ping_sensor();
//			printf("Raw Distance %ld cm\t", distance_arr[tmp]);
			usleep(150000);
		}
		pthread_mutex_lock(&m1);
		distance = (distance_arr[0] + distance_arr[1] + distance_arr[2] + distance_arr[3])/4;
		pthread_mutex_unlock(&m1);
		printf("Filtered Distance %ld cm\n", distance);
	}
}

void move_forward(int d) {
	int i;
	for(i = 0; i < 5; i++) {
		spi_transfer(p0[i].led); //Move left
		usleep(1000*d);
	}
	for(i = 0; i < 5; i++) {
		spi_transfer(p1[i].led); //Move right
		usleep(1000*d);
	}
}

void move_reverse(int d) {
	int i;
	for(i = 5; i > 0; i--) {
		spi_transfer(p0[i].led); //Move left
		usleep(1000*d);
	}
	for(i = 5; i > 0; i--) {
		spi_transfer(p1[i].led); //Move right
		usleep(1000*d);
	}
}

void* display_function(void *arguments) {
	int dist_cm;
	while(1) {
		pthread_mutex_lock(&m1);
		dist_cm = distance; //Copy the current global value
		pthread_mutex_unlock(&m1);
		if(dist_cm == 0) {
			spi_transfer(on[0].led);
			printf("Move more than 10cms away or wait for sensor to initialize\n");
		} else if(dist_cm > 10 && dist_cm < 100) {
			move_forward(dist_cm);
		} else {
			move_reverse(dist_cm);		
		}
	}
	return 0;
}

/*
 Signal handler to terminate the sensor thread
*/
static void sighandler_sensor(int signum) {
	if(signum == SIGUSR1) {
		pthread_cancel(sensor_thread);
	}
}

/*
 Signal handler to terminate the display thread
*/
static void sighandler_display(int signum) {
	spi_transfer(off[0].led); //Turn off everything while exiting
	printf("Closing spidev1.0\n");
	close(spi_fd);
	if(signum == SIGUSR1) {
		pthread_cancel(display_thread);
	}
}


int main(void)
{
	IOSetup(); //Setup gpios for ultrasonic sensor
	IOSetup_spi(); //Setup spi pins
	pthread_mutex_init(&m1, NULL);
	pthread_create(&sensor_thread,NULL,sensor_function,NULL);
	pthread_create(&display_thread,NULL,display_function,NULL);

	sleep(60); // Let the program run for 60 secs

	signal(SIGUSR1, sighandler_sensor);
	pthread_kill(sensor_thread, SIGUSR1);
	pthread_join(sensor_thread,NULL);

	signal(SIGUSR1, sighandler_display);
	pthread_kill(display_thread, SIGUSR1);
	pthread_join(display_thread,NULL);
	
	return 0;
}
