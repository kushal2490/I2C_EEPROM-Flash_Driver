#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "libioctl.h" 

#define DEVICE "i2c_flash"

struct i2c_flash {
	struct cdev cdev;
	struct device *dev;
	struct i2c_adapter *adap;
	}*i2c_dev;

/* work queue structure */
static struct workqueue_struct *my_wq;
typedef struct 
{
	struct work_struct my_work;
	struct i2c_client *client;
	char	*buffer;
	int 	page_count;
}my_work_t;

struct class *i2c_dev_class;
struct i2c_client *client;

static int busy;
static int ready;
static int current_page;
static dev_t i2c_dev_number;
unsigned char addr_highbyte = 0x00;
unsigned char addr_lowbyte = 0x00;

my_work_t *write_work, *read_work;

/* function to calculate the low and high bytes of the address
according to the page number */
void calc_highlow(int num_page)
{
	int count = 0;
	int temp_byte;
	int addr_bytes;
	addr_highbyte = 0x00;
	addr_lowbyte = 0x00;
	while(count != num_page)
	{
	addr_bytes = addr_highbyte;
	temp_byte = addr_lowbyte;
	addr_bytes = (addr_bytes << 8 ) | (temp_byte);
	addr_bytes = addr_bytes + 64;
	addr_lowbyte = (addr_bytes & 0xFF);
	addr_highbyte = (addr_bytes & 0xFF00) >> 8;
	count+=1;
	}
}


static void wq_read_function(struct work_struct *work)
{
	int ret;
	int i=0, m=0, n=0;	
	char *data;
	
	my_work_t *my_work = (my_work_t*)read_work;
	int count = my_work->page_count;

	data = (char*)kmalloc(sizeof (char)*64, GFP_KERNEL);
	busy = 1;
	
	gpio_set_value_cansleep(10, 1);

	while (n != count)
	{	
		
		calc_highlow(current_page);
		ret = i2c_master_recv(my_work->client, data, 64);
		msleep(1);
		for (m=0;m<64;m++)
		{
			my_work->buffer[i] = data[m];
			i++;		
		}
		n++;
		
		if (current_page == 511)
			current_page = 0;
		else
			current_page++;
	}
	gpio_set_value_cansleep(10, 0);
	
	
	busy = 0;										// EEPROM is available
	ready = 1;
	
	kfree(data);
	
}
/* my write workqueue function for writing messages from queue to the i2c device */
static void wq_write_function(struct work_struct *work)
{
	int i, j, ret = 0, page;
	int page_count;
	unsigned char* data;

	int data_byte_count = 66;

	my_work_t *my_work = (my_work_t*)write_work;	
	
	client->addr= 0x51;

	data = (char*)kmalloc(sizeof(char)*data_byte_count, GFP_KERNEL);
	page_count  = my_work->page_count;
	page =0;
	/* set EEPROM busy flag to 1, glow the led and perform the i2c device write */
	busy = 1;
	gpio_set_value_cansleep(10, 1);

	while (page != page_count)
	{	
		
		calc_highlow(current_page);
		data[0] = addr_highbyte;
        	data[1] = addr_lowbyte;
		j = 2;
		for(i=page*64;i<(page*64)+64;i++)
		{
			data[j] = my_work->buffer[i];
			j++;
		}
		data[66] = '\0';
		ret = i2c_master_send(my_work->client, data, 66);
		msleep (5);
		page++;
				
		if (current_page == 511)
			current_page = 0;
		else
			current_page++;
		
	}
	gpio_set_value_cansleep(10, 0);	
	busy = 0;									
	
	kfree(data);	
	kfree(write_work->buffer);
	kfree(write_work);
}

/* function to read a sequence of count pages from the EEPROM device into the user 
memory pointed by buf. The pages to be read start from the current page
position of the EEPROM. The page position is then advanced by count and, 
if reaching the end of pages, wrapped around to the beginning of the EEPROM. */
static ssize_t i2cdev_read(struct file *file, char __user *buf, size_t count,
		loff_t *offset)
{
	int ret = 0;
	struct i2c_client *client = file->private_data;	
		
	if (ready == 1)										// read work is ready
	{	
			
		ret = copy_to_user((void __user *)buf,(void*)read_work->buffer,64*count);
		kfree(read_work->buffer);
		ready = 0;		
		return 0;
	
	}
	/* EEPROM is busy and data is available in the workqueue buffer */
	else if (ready != 1 && busy == 1)
	{
		return -EBUSY;
	}
	else if (ready != 1 && busy == 0)
	{
		/* queueing read work into the work queue */
		read_work->buffer = (char*)kmalloc(sizeof(char)*64*count,GFP_KERNEL);
		if (read_work->buffer == NULL)
			return -ENOMEM;
			
		read_work->page_count = count;
		read_work->client = client;
		
		INIT_WORK( (struct work_struct*)read_work,wq_read_function);
		
		ret = queue_work(my_wq,(struct work_struct*)read_work);
		msleep(1);
		
		return -EAGAIN;			//no EEPROM data is ready to read and EEPROM is not busy
	}	
	else 
		return -EPERM;
	
}

/* function to write a sequence of count pages to an EEPROM device starting 
from the current page position of the EEPROM. The page position is 
then advanced by count and, if reaching the end of pages, wrapped 
around to the beginning of the EEPROM. */
static ssize_t i2cdev_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
	int ret = 0;
	int res = 0;
	client = file->private_data;
	
	if (busy == 1)
		return -EBUSY;
		
	/* queueing write work into the work queue */
	write_work = (my_work_t *)kmalloc(sizeof(my_work_t), GFP_KERNEL);
	if (write_work == NULL)
			return -ENOMEM;
			
	write_work->buffer = (char *)kmalloc (sizeof(char)*count*66, GFP_KERNEL);
	if (write_work->buffer == NULL)
			return -ENOMEM;
			
	res = copy_from_user((void*)write_work->buffer, (void __user *)buf, count*64);
	msleep(5);
	
	write_work->client = client;
	write_work->page_count = count;
	
	INIT_WORK( (struct work_struct *)write_work, wq_write_function );
	ret = queue_work(my_wq,(struct work_struct*)write_work);
	
	return 0;
}

/* I2C ioctl commands to operate on the EEPROM */
long i2cflash_ioctl(struct file *file,unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned char buf[2];
	switch(cmd) 
	{
		case FLASHGETS:
		{
			/* checks if the EEPROM is busy or available */
			if ( busy == 1)
				return -EBUSY;	
			else
				return 0;
		}
		break;
		case FLASHSETP:
		{			
			/* Set current page position to Read, Write or Erase from */
			struct i2c_client *client = file->private_data;
			client->addr= 0x51;			
			current_page = arg;			
			calc_highlow(current_page);
			buf[0] = addr_highbyte;
			buf[1] = addr_lowbyte;
			ret = i2c_master_send(client, buf, 2);
			msleep (5);
		}
		break;
		case FLASHGETP:	
		{
			/* Get current page position */
			ret = copy_to_user((int *)arg, &current_page, sizeof(int));	
		}
		break;
		case FLASHERASE:
		{
			int i=0, j, retr=0, low, addr_hilo; 
			int data_count = 66;
			int num_page;
			unsigned char* data;
			unsigned char addr_highbyte = 0x00;
			unsigned char addr_lowbyte = 0x00;
			struct i2c_client *client = file->private_data;
			client->addr= 0x51;
			
			data = (char*)kmalloc(sizeof(char)*data_count, GFP_KERNEL);
			if (data == NULL)
				return -ENOMEM;
			num_page = 0;
						
			/* start Erase operation when EEPROM becomes free */
			if (busy != 1)
			{ 
				gpio_set_value_cansleep(10, 1);
				while (num_page != 512)
				{	
					data[0] = addr_highbyte;
	        			data[1] = addr_lowbyte;
	    		
					j = 2;
					for(i = num_page*64; i < (num_page*64)+ 64;i++)
					{
						data[j] = 0xFF;
						j++;
					}
					data [66] = '\0';
					
    					/* write 1's to 512 pages of EEPROM */
    			    		retr = i2c_master_send(client, data, data_count);
					msleep (5);
					num_page = num_page +1;			
					
					/* calculate the next address high and low bytes */
					low = addr_lowbyte;
					addr_hilo = addr_highbyte;
					addr_hilo = (addr_hilo << 8) | low;
					addr_hilo = addr_hilo + 64;
					addr_highbyte = (addr_hilo & 0xFF00) >> 8;
					addr_lowbyte = (addr_hilo & 0xFF);
			
				}				
				gpio_set_value_cansleep(10, 0);
			}
			else 
				return -EBUSY;
			kfree(data);
			
		}	
		break;
 	}
	
return ret;

}

static int i2cdev_open(struct inode *inode, struct file *file)
{
         struct i2c_adapter *adap;
         struct i2c_flash *i2c_dev;
 
         i2c_dev = container_of(inode->i_cdev, struct i2c_flash, cdev);
         if (!i2c_dev)
                 return -ENODEV;
 
         adap = i2c_get_adapter(0);
         if (!adap)
                 return -ENODEV;
 		/* Allocate memory to create the client */
         client = kzalloc(sizeof(*client), GFP_KERNEL);	
         if (!client) 
		{
                 i2c_put_adapter(adap);
                 return -ENOMEM;
         }
         snprintf(client->name, I2C_NAME_SIZE, "i2c-dev %d", 0);
         client->adapter = adap;
         file->private_data = client;
         return 0;
}


static int i2cdev_release(struct inode *inode, struct file *file)
{
	
	printk(KERN_ALERT" device released\n");
        file->private_data = NULL; 
    	return 0;
}


static const struct file_operations fops = {
	.owner			= THIS_MODULE,
	.read			= i2cdev_read,
	.write			= i2cdev_write,
	.unlocked_ioctl	= i2cflash_ioctl,
	.open 			= i2cdev_open,
	.release	 	= i2cdev_release,
};

static int __init i2c_dev_init(void)
{
	int res;
	int gpiores;
	struct i2c_adapter *adap;
	

	adap = kmalloc (sizeof (struct i2c_adapter), GFP_KERNEL);
	if (adap == NULL)
		return -ENOMEM;
	printk(KERN_ALERT"i2c dev entries driver\n");
	
	/* Allocate Major & Minor number for the Device */
	if (alloc_chrdev_region(&i2c_dev_number, 0, 1, DEVICE) < 0) 
	{
			printk(KERN_DEBUG "Can't register device\n"); 
			return -1;
	}
	
	i2c_dev_class = class_create(THIS_MODULE, DEVICE);
	printk(KERN_ALERT"Class created\n");
	
	/* Bind to existing i2c_adapter */
		
	adap = to_i2c_adapter(0);

	i2c_dev = kzalloc (sizeof (*i2c_dev), GFP_KERNEL);

	/* Register this i2c flash device with the driver core */
	i2c_dev->adap = adap;

	device_create(i2c_dev_class, NULL ,i2c_dev_number, NULL, DEVICE);
	
	printk(KERN_ALERT"adapter attached to i2c core\n");
		
	/* Connect the file operations with the cdev */
	cdev_init(&i2c_dev->cdev, &fops);

	/* Connect the Major/Minor number to the cdev */
	res = cdev_add(&i2c_dev->cdev, (i2c_dev_number), 1);

	if (res) 
	{
		printk("Bad cdev\n");
		return res;
	}
	
	/* Create my workqueue */
	my_wq = create_workqueue("my_queue");
	
	read_work = (my_work_t*)kmalloc(sizeof(my_work_t),GFP_KERNEL);
	if (read_work == NULL)
		return -ENOMEM;

	/* SDA and SCL are enabled by MUX pin gpio60 with a low signal */
	gpio_request_one(60, GPIOF_OUT_INIT_LOW, "mux");
	gpiores = gpio_get_value_cansleep(60);
	printk(KERN_INFO "GPIO 60 value is %d\n", gpiores);

	/* gpio 74 acts as a select pin to allow gpio 10 input */
	gpio_request_one(74, GPIOF_OUT_INIT_LOW, "select");

	/* LED output value is setup with gpio10 */
	gpio_request_one(10, GPIOF_OUT_INIT_LOW, "led");
	gpiores = gpio_get_value_cansleep(10);
	printk(KERN_INFO "GPIO 10 value is %d\n", gpiores);

	/* LED is connected to IO10, we set the direction out for gpio26 */
	gpio_request_one(26, GPIOF_DIR_OUT, "ledout");

		
	return 0;

}

static void __exit i2c_dev_exit(void)
{
	device_destroy(i2c_dev_class, i2c_dev_number);
	class_destroy(i2c_dev_class);
	unregister_chrdev(i2c_dev_number, DEVICE);
	printk("driver removed\n");
	cdev_del(&i2c_dev->cdev);
	i2c_put_adapter(client->adapter);
	kfree(client);

	gpio_free(26);
	gpio_free(60);
	gpio_free(74);
	gpio_free(10);
	
}

MODULE_AUTHOR("Kushal Anil Parmar");
MODULE_DESCRIPTION("Accessing EEPROM via I2c bus");
MODULE_LICENSE("GPL");

module_init(i2c_dev_init);
module_exit(i2c_dev_exit);



