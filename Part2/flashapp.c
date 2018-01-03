#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>	
#include <sys/ioctl.h>
#include "libioctl.h"

#define DEVICE "/dev/i2c_flash"

void gen_rand(char* string, int length)
{
	char charset[] = "0123456789"
                   "abcdefghijklmnopqrstuvwxyz"
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	while (length-- > 0) {
		int index = rand() % 62;
		*string++ = charset[index];
		}
	*string = '\0';
}

int main()
{
	int fd;
	int pages;
	int select;
	int page_position;
	int retr, rete, retu;
	char* message_rcv;
	char* message_snd;
	char* random_string = NULL;
	unsigned long current_page;
	
	fd= open(DEVICE,O_RDWR);
	if(fd==-1)
	{
     	printf("file %s does not exist or is open\n", DEVICE);
     	exit(-1);
	}
	printf ("\nDevice Opened\n");

	while (1) 
	{
		select = 0;
		printf("\n\t\t\t*********************************\n\t\t\t 1. Check EEPROM status\n\t\t\t 2. Get Current Page Position \n\t\t\t 3. Set Current Page Position\n\t\t\t 4. Write Data to EEPROM\n\t\t\t 5. Read Data from EEPROM\n\t\t\t 6. Erase Data from EEPROM\n\t\t\t 7. EXIT\n\t\t\t*********************************\n\n\t\t\tEnter EEPROM operation number to perform : ");

		scanf("%d",&select);
		switch (select)
		{
			case 1:
				retu = ioctl(fd,FLASHGETS, 1);
				if (retu != 0)
					printf ("\nStatus of EEPROM is - Busy\n");
				else
					printf ("\nStatus of EEPROM is - Available\n");
				break;

			case 2:
				ioctl(fd,FLASHGETP, &page_position);  				//ioctl call
				printf("\n------->Current Page position is at %d\n", page_position);
				break;

			case 3:
				printf("\n------->Enter the current page position you wish to set (0 to 511)\t");
				scanf("%ld",&current_page);
				ioctl(fd,FLASHSETP, current_page);  //ioctl call
				break;

			case 4:
				printf ("\n------->Enter number of pages to write (1 - 512)\t");
				scanf("%d",&pages);
				message_snd = (char*)malloc(sizeof(char)*64*pages);
				random_string = (char *)malloc(sizeof(char) * pages * 64);
				gen_rand(random_string, pages * 64);
				strcpy(message_snd, random_string);
				write(fd,message_snd, pages);
				printf("\nMessage sent by User to EEPROM : %s\n",message_snd);
				free (message_snd);		
				break;

			case 5:
				printf ("\n------->Enter number of pages to READ (1 - 512)\t");
				scanf("%d",&pages);
				message_rcv = (char*)malloc(sizeof(char)*64*pages);
				printf("Wait for EEPROM to become ready, message is being enqueued...");

				/* wait for the pointer to return reading enter data, otherwise we require multiple read
				operations to read all data correctly */
				do		
				{
					retr = read(fd, message_rcv, pages);
					usleep(1);
				}while(retr != 0);

				message_rcv[64*pages] = '\0';
				printf("\n------->Read return value:%d\n", retr);
				printf("Message received by USER from EEPROM : %s\n", message_rcv);
				free (message_rcv);
				break;

			case 6:
				rete = ioctl(fd,FLASHERASE, 1);  				//ioctl call
				if (rete != 0)
					printf ("EEPROM Busy...\n");
				else
					printf("EEPROM successfully erased\n");
				break;	

			case 7:
				close(fd);
				printf("EXIT from EEPROM\n");
				return 0;

			default:
				printf("Wrong operation selected\n");
				break;
		}
	}
	
}






