#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "i2c-dev.h"
#include "gpioaccess.h"

#define DEVICE "/dev/i2c-0"
#define ADDRESS 0x51		//EEPROM address

unsigned char addr_highbyte = 0x00;
unsigned char addr_lowbyte = 0x00;
int fd;
static int current_page;

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
	addr_lowbyte = (addr_bytes & 0xFF);			//Low Byte
	addr_highbyte = (addr_bytes & 0xFF00) >> 8;	//High Byte
	count+=1;
	}
}

/* function that returns the current page position and set it to
the new position. -1 should be returned if the new position is out of range */
int EEPROM_set(int new_position)
{
	unsigned char buf[10];

	/* check whether new position is in range page 0-511 */
	if(new_position > 511 || new_position < 0)
		return -1;

	/* set the slave address to write at page offset */
	if(ioctl(fd, I2C_SLAVE, ADDRESS) < 0)
	{
		printf("Failed to set the slave address : %s\n", strerror(errno));
		exit(-1);
	}
	
	printf("------->Current position is at page %d address = 0x%02x%02x\n", current_page, addr_highbyte, addr_lowbyte);
	
	current_page = new_position;

	/* Set position of new page to new_position */
	calc_highlow(current_page);
	printf("------->Setting current position to page %d address = 0x%02x%02x \n", current_page, addr_highbyte, addr_lowbyte);

	buf[0] = addr_highbyte;
	buf[1] = addr_lowbyte;
	if(write(fd, buf, 2) != 2)
	{	
		printf("Error in setting new page pointer\n");
		return -1;
	}
	usleep(5000);

return 0;
}

/* function to read a sequence of “count” pages from the EEPROM device
into the user memory pointed by buf. The pages to be read start from the
current page position of the EEPROM. The page position is then 
advanced by “count” and, if reaching the end of pages, wrapped around
to the beginning of the EEPROM */
int EEPROM_read(char *buf, int count)
{
	int bytecount = count*64;
	char *rcvbuf;
	char *rbuf;
	int pagecount=0;
	int i,j=0;
	char buffer[10];

	rcvbuf = (char*)malloc(sizeof(char)*64);
	rbuf = (char*)malloc(sizeof(char)*bytecount);

	/* set the slave address to write at page offset */
	if(ioctl(fd, I2C_SLAVE, ADDRESS) < 0)
	{
		printf("Failed to set the slave address : %s\n", strerror(errno));
		exit(-1);
	}

	gpioSet(10, 1);
	while(pagecount != count)
	{

		/* Set pointer position for reading */
		calc_highlow(current_page);
#if 1
		buffer[0] = addr_highbyte;
		buffer[1] = addr_lowbyte;
		if(write(fd, buffer, 2) != 2)
		{		
			printf("Error in setting new page pointer\n");	
			return -1;			
		}
		usleep(5000);
#endif	
		/* read from the set location */
		if(read(fd, rcvbuf, 64) != 64)
		{
			printf("Failed to read from EEPROM \n");
			return -1;
		}
		usleep(1000);
#if 1
		for(i=0; i<64; i++)
		{
			rbuf[j] = rcvbuf[i];
			buf[j] = rcvbuf[i];
			j++;
		}
#endif
		pagecount++;

		if(current_page == 511)
			current_page = 0;
		else
			current_page++;
	}
	gpioSet(10, 0);

	free(rcvbuf);
	free(rbuf);

return 0;
}

/* function to write a sequence of “count” pages to an EEPROM device starting
from the current page position of the EEPROM. The page position is then
advanced by “count” and, if reaching the end of pages, wrapped around to
the beginning of the EEPROM */
int EEPROM_write(void *buf, int count)
{
	int pagecount = 0;
	int bytecount = 66;
	char *sendbuf;
	char *data;
	char *rcvdbuf;
	int bufindex;
	int i,k;

	sendbuf = (char *)malloc(sizeof(char)*bytecount);
	data = (char *)malloc(sizeof(char)*64*count);
	rcvdbuf = (char*)malloc(sizeof(char)*64);

	/* set the slave address to write at page offset */
	if(ioctl(fd, I2C_SLAVE, ADDRESS) < 0)
	{
		printf("Failed to set the slave address : %s\n", strerror(errno));
		exit(-1);
	}

	for(k=0;k<64*count;k++)
		data[k] = *(char *)buf++;
	data[64*count] = '\0';

	gpioSet(10, 1);
	while(pagecount != count)
	{
		calc_highlow(current_page);
		sendbuf[0] = addr_highbyte;
		sendbuf[1] = addr_lowbyte;
		bufindex = 2;
		for(i=pagecount*64; i<(pagecount*64)+64; i++)
		{
			sendbuf[bufindex] = data[i];
			bufindex++;
		}
		sendbuf[66] = '\0';

		write(fd, sendbuf, 66);
		usleep(5000);

		pagecount++;
		
		if(current_page == 511)
			current_page = 0;
		else
			current_page++;
	}
	gpioSet(10, 0);

	free(data);
	free(sendbuf);
	free(rcvdbuf);

return 0;
}

/* function to trigger an erase operation to the EEPROM which writes all 1’s to
all 512 pages and reset current page position to 0 */
int EEPROM_erase()
{
	int pagecount;
	int temp_byte;
	int addr_bytes;
	int bytecount = 66;
	char* sendbuf;
	int bufindex;
	int i;
	unsigned char addr_highbyte = 0x00;
	unsigned char addr_lowbyte = 0x00;
	
	sendbuf = (char *)malloc(sizeof(char)*bytecount);
	
	/* set the slave address to write at page offset */
	if(ioctl(fd, I2C_SLAVE, ADDRESS) < 0)
	{
		printf("Failed to set the slave address : %s\n", strerror(errno));
		exit(-1);
	}

	pagecount = 0;
	gpioSet(10, 1);

	while(pagecount != 512)
	{
		sendbuf[0] = addr_highbyte;
		sendbuf[1] = addr_lowbyte;
		bufindex = 2;
		for(i=pagecount*64; i<(pagecount*64)+64; i++)
		{
			sendbuf[bufindex] = 0xFF;
			bufindex++;
		}
		sendbuf[66] = '\0';

		write(fd, sendbuf, 66);
		usleep(5000);
		pagecount = pagecount + 1;

		/* calculate address for next page */
		addr_bytes = addr_highbyte;
		temp_byte = addr_lowbyte;
		addr_bytes = (addr_bytes << 8 ) | (temp_byte);
		addr_bytes = addr_bytes + 64;
		addr_lowbyte = (addr_bytes & 0xFF);			//Low Byte
		addr_highbyte = (addr_bytes & 0xFF00) >> 8;	//High Byte
	}
	gpioSet(10, 0);

	/* Reset current page pointer to page 0 (0x0000) */
	EEPROM_set(0);

	free(sendbuf);	

return 0;
}

/* function to set up the signals for following I2C bus operations
 and to initialize the current page position to 0 */
int EEPROM_init()
{
	char buf[10];

	/* Initialise gpio 60 */
	gpioExport(60);
	gpioDirection(60, 1);
	gpioSet(60, 0);

	/* Initialise gpio 74 which acts as select pin for mux */
	gpioExport(74);
	gpioSet(74, 0);

	/* Initialise gpio 10 with initial value 0 LED OFF */
	gpioExport(10);
	gpioDirection(10, 1);
	gpioSet(10, 0);

	/* Initialise gpio 26 to direction out */
	gpioExport(26);
	gpioDirection(26, 1);

	fd = open(DEVICE, O_RDWR);
	if(fd==-1)
	{
		printf("File %s does not exist or is open\n", DEVICE);
		exit(-1);
	}
	printf("\nDevice Opened\n");

	/* Set the I2C slave address to 0x51*/
	if(ioctl(fd, I2C_SLAVE, ADDRESS) < 0)
	{
		printf("Failed to set the slave address : %s\n", strerror(errno));
		exit(-1);
	}
	
	/* Set I2C pointer position */
	buf[0] = addr_highbyte;
	buf[1] = addr_lowbyte;
	if(write(fd, buf, 2) != 2)
	{	
		printf("Error in setting pointer\n");
		return -1;			
	}
	usleep(5000);	

return 0;
}
	
void* EEPROM_exit()
{
	/* unexport the gpio 60 and gpio 26 pins */
	gpioUnexport(60);
	gpioUnexport(26);
	/* close I2C device i2c-0 */
	close(fd);

return 0;
}



