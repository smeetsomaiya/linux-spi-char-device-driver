
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/device.h>
//#include <linux/jiffies.h>
#include <linux/param.h>
#include <linux/list.h>
//#include <linux/semaphore.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#define DRIVERNAME "spidev"
#define DEVICENAME "LEDMATRIX"
#define DEVICECLASS "PATDISPLAY"
#define MAJORNO 154
#define MINORNO 0

#define BEGIN 11

bool is_gpio_requested = false;

#define ROW_SIZE(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))
#define COLUMN_SIZE(arr) ((int) sizeof ((arr)[0]) / sizeof (int))

// Array containing the gpio pin configuration
int spi_mapping_array[20][4] = {
			                                {100,24,44,72}, //72-low
							{15,42,100,100},
							{100,30,46,100}
};

// Array containing the gpio pin configuration
int spi_values_array[20][4] = {
			                               {100,0,1,0},
							{0,0,100,100},
							{100,0,1,100},
};

static struct spidev_data *spi_data;
static struct class *spimatrix_class;
static struct spi_message sm;

static unsigned int flag = 0;
uint8_t seq_disp[11][2];

uint8_t animation_arr[10][8];
uint8_t arr_msg_tx[2]={0};


struct spi_transfer spi_tr = {

	.tx_buf = &arr_msg_tx[0],
	.rx_buf = 0,
	.len = 2,
	.bits_per_word = 8,
	.speed_hz = 10000000,
	.delay_usecs = 1,
	.cs_change = 1,
};

struct spidev_data {
	dev_t  devt;
	struct spi_device *spi;
};

int spi_matrix_val(uint8_t a1,uint8_t a2)
{          //transfer data to be displayed on matrix
	int retval=0;

	arr_msg_tx[0]=a1;
	arr_msg_tx[1]=a2;
	spi_message_init(&sm);
	spi_message_add_tail((void*)&spi_tr,&sm);
	gpio_set_value(15,0);
	retval=spi_sync(spi_data->spi, &sm);
	gpio_set_value(15,1);

	return 0;
}


int spi_transfer_write(void *data)
{
     
    int k=0,l=0,n=0;

   
/*    if(seq_disp[0][0]==0 && seq_disp[0][1]==0)    //Checking for end of sequence
      {
	printk(KERN_ALERT "Term. seq\n");
        for(k=0x00;k < 0x09;k++)
         {
           spi_matrix_val(k,0x00);                //Clear Led matrix
         }
       flag = 0;
       goto CR;
      } */
	printk(KERN_ALERT "Row sz of seq_dis %d\n", ROW_SIZE(seq_disp));
      for(l=0;l<ROW_SIZE(seq_disp);l++)                           //sequence selection
      {
          for(n=0;n<ROW_SIZE(seq_disp);n++)                       //pattern selection
           {
            if(seq_disp[l][0]==n)
              {
                if(seq_disp[l][0]==0 && seq_disp[l][1]==0)
                 {
			for(k=0x00;k < 0x09;k++)
			{
				spi_matrix_val(k,0x00);                //Clear Led matrix
			}
			goto CR;
                 }
            else
             {
             spi_matrix_val(0x01,animation_arr[n][0]);
             spi_matrix_val(0x02,animation_arr[n][1]);
             spi_matrix_val(0x03,animation_arr[n][2]);
             spi_matrix_val(0x04,animation_arr[n][3]);
             spi_matrix_val(0x05,animation_arr[n][4]);
             spi_matrix_val(0x06,animation_arr[n][5]);
             spi_matrix_val(0x07,animation_arr[n][6]);
             spi_matrix_val(0x08,animation_arr[n][7]);
	     printk(KERN_ALERT "Sleeping %d ms\n", seq_disp[l][1]);
//	     printk(KERN_ALERT "Anim %d:  %x \t", n, animation_arr[n][0]);
//	     printk(KERN_ALERT "Anim %d: %x \n", n, animation_arr[n][7]);
             msleep(seq_disp[l][1]);
            }
         }
      }
    }
     
  CR:
  flag=0;
		printk(KERN_ALERT "kthread exiting\n");
do_exit(0);
return 0;   
}


static int spi_matrix_open(struct inode *inode, struct file *filp) {
	printk(KERN_ALERT " Exporting gpio pins from here causes slow path warning\n");
	return 0;
}

void spi_config(void) {
	int clr;
	spi_matrix_val(0x0F,0x01);                                 //configure led matrix
	spi_matrix_val(0x0F,0x00);
	spi_matrix_val(0x09,0x00);
	spi_matrix_val(0x0A,0x0F);
	spi_matrix_val(0x0B,0x07);
	spi_matrix_val(0x0C,0x01);
	for(clr=0x00; clr < 0x09; clr++)
	{
		spi_matrix_val(clr, 0x00);
	}

	printk(KERN_INFO"\nled matrix configured successfully");
}

    struct task_struct *func;

int is_gpio_64_to_79(int gpio) { //Check for gpio's which do not have a direction file
	if(gpio >= 64 && gpio <= 79) {
		return 1; //return true
	}	
	return 0; //return false
}

int mux_gpio_set(int gpio, int value)
{
	if(gpio == -1 || value == -1) {
		printk("-1 found in mux_gpio_set\n");
		return -1;
	}
	printk(KERN_ALERT "Gpio %d Value %d\n", gpio, value);
	gpio_request(gpio, "sysfs");

	if(is_gpio_64_to_79(gpio)) { //No direction file present for these GPIOs
		printk(KERN_ALERT "Gpio is between 64 and 79 %d\n", gpio);
//		gpio_set_value(gpio, value);
	} else {
		gpio_direction_output(gpio, value);
		printk(KERN_ALERT "Not setting direction, gpio %d", gpio);
	}
	return 0;
}

void configure_spi_pins(int spi_array_row)
{                    
    int column;
    int columns_of_spi_array = COLUMN_SIZE(spi_mapping_array);

    for(column = 0; column < columns_of_spi_array; column++) {
        int read_value = spi_mapping_array[spi_array_row][column];
        if(read_value != 100) {
//		printf("\n\n Configure pins for IO %d - %d\n\n", spi_array_row, spi_mapping_array[spi_array_row][column]);
		//output for trigger pins and input for echo pins
		mux_gpio_set(spi_mapping_array[spi_array_row][column],/* 1,*/ spi_values_array[spi_array_row][column]);
	}
    }
	is_gpio_requested = true;
}
int mosi = 0;
int ss = 1;
int sck = 2;
void cleanup_gpio(void) {
	if(is_gpio_requested) {
		int k, read_value;
		for (k = 0; k < COLUMN_SIZE(spi_mapping_array); k++) {
			read_value = spi_mapping_array[mosi][k];
		        if(read_value != 100) {
				gpio_free(read_value);
				printk(KERN_ALERT "Gpio free %d\n", read_value);
			}
			read_value = spi_mapping_array[ss][k];
		        if(read_value != 100) {
				gpio_free(read_value);
				printk(KERN_ALERT "Gpio free %d\n", read_value);
			}
			read_value = spi_mapping_array[sck][k];
		        if(read_value != 100) {
				gpio_free(read_value);
				printk(KERN_ALERT "Gpio free %d\n", read_value);
			}
		}
	}
}

static int spi_matrix_release(struct inode *i,struct file *filp){

	int k;
	for(k=0x00;k < 0x09;k++) {
           spi_matrix_val(k,0x00);
	}

	printk(KERN_INFO" Releasing all gpio pins\n");
	cleanup_gpio();
	return 0;
}

static ssize_t spi_matrix_write(struct file *f,const char *spi_matrix, size_t sz, loff_t *loffptr){
      
	int retval, i=0,j=0;
	uint8_t seq[sz];
	printk(KERN_ALERT "Called write with %d bytes\n", sz);
	if (flag==1) {
		return -EBUSY;
	}

	retval=copy_from_user((void *)&seq, (void* __user)spi_matrix, sizeof(seq));
	for(i=0;i<sz;i=i+2)
	{       
		j=i/2;
		seq_disp[j][0] = seq[i]; 
		seq_disp[j][1] = seq[i+1];
		printk(KERN_INFO"Order of display pattern %d: %u %u ",j,seq_disp[j][0],seq_disp[j][1]);
	}
	if(retval !=0)
	{
		printk("\n Unable to copy bytes%d\n",retval);
	}
	flag = 1;
	func = kthread_run(&spi_transfer_write, (void *)seq_disp,"kthread_spi_matrix_transfer");
	printk(KERN_ALERT "exiting write\n");
	return retval; 
}


static long spi_ioctl(struct file *filp, unsigned int arg,unsigned long cmd){
	int retval = 0;
	if(cmd == BEGIN) {
		int i=0,j=0;
		uint8_t pattrn_arr[10][8];
		configure_spi_pins(mosi);
		configure_spi_pins(ss);
		configure_spi_pins(sck);
		spi_config();
		retval=copy_from_user((void *)&pattrn_arr, (void *)arg, sizeof(pattrn_arr));
		if(retval !=0)
		{
			printk("\nUnable to copy bytes in ioctl%d",retval);
		}
		for(i=0;i < 10;i++)
		{
			 for(j=0;j < 8;j++)
			 {
			 	animation_arr[i][j]=pattrn_arr[i][j];
				 printk(KERN_INFO"pattern array is %d",animation_arr[i][j]);
			 }
		}
	}
	return retval;
}


static const struct file_operations matrix_fops = {
	.open = spi_matrix_open,
	.release = spi_matrix_release,
	.write	= spi_matrix_write,
	.unlocked_ioctl = spi_ioctl,
};


static int spimatrix_probe(struct spi_device *spi)
{
	                                                             
	int status = 0;
	struct device *dev;                                           //struct spidev_data *spidev;

	                                                             
	spi_data = kzalloc(sizeof(*spi_data), GFP_KERNEL);            // Allocate driver data
	if(!spi_data)
	{
		return -ENOMEM;
        }
	spi_data->spi = spi;                                         //initialize driver data

	spi_data->devt = MKDEV(MAJORNO, MINORNO);

	dev = device_create(spimatrix_class, &spi->dev, spi_data->devt, spi_data, DEVICENAME);

	if(dev == NULL)
	{
		printk("Device Creation Failed\n");
		kfree(spi_data);
		return -1;
	}
	printk("SPI LED Driver Probed.\n");
	return status;
}


static int spimatrix_remove(struct spi_device *spi)
{
	device_destroy(spimatrix_class, spi_data->devt);
	kfree(spi_data);
	printk("SPI led matrix driver Removed.\n");
	return 0;
}


static struct spi_driver spimatrix_driver = {
         .driver = {
                 .name = DRIVERNAME,
                 .owner=THIS_MODULE,
         },
         .probe = spimatrix_probe,
         .remove =spimatrix_remove,
};


int __init spimatrix_init(void)
{
	int retval;
	                                                                          
	retval = register_chrdev(MAJORNO, DEVICENAME, &matrix_fops);                  //Register the Device
	if(retval < 0)
	{
		printk("Device Registration Failed\n");
		return -1;
	}
	
	spimatrix_class = class_create(THIS_MODULE, DEVICECLASS);                    //Create the class
	if(spimatrix_class == NULL)
	{
		printk("Class Creation Failed\n");
		unregister_chrdev(MAJORNO, spimatrix_driver.driver.name);
		return -1;
	}
	
	retval = spi_register_driver(&spimatrix_driver);                              //Register the Driver
	if(retval < 0)
	{
		printk("Driver Registraion Failed\n");
		class_destroy(spimatrix_class);
		unregister_chrdev(MAJORNO, spimatrix_driver.driver.name);
        return -1;

        }
        printk("\nSPI driver Initialized");
	return retval;
}


void __exit spimatrix_exit(void)
{
	spi_unregister_driver(&spimatrix_driver);
	class_destroy(spimatrix_class);
	unregister_chrdev(MAJORNO,spimatrix_driver.driver.name);
        printk("\nUninitialized Driver");
	
}

module_init(spimatrix_init);
module_exit(spimatrix_exit);
MODULE_LICENSE("GPL v2");






    

