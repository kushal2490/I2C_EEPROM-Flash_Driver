#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libmain.h"

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
	int num_page;
	char *message = NULL;
	char *message_snd;
	char *message_rcv;
	unsigned long current_page;
	int select;
	int ret;


	while(1) 
	{
		select = 0;
		printf("\n\n\t\t\t*********************************\n\t\t\t 1. init EEPROM \n\t\t\t 2. Get & Set Current Page Position \n\t\t\t 3. Write Data to EEPROM\n\t\t\t 4. Read Data from EEPROM\n\t\t\t 5. Erase Data from EEPROM\n\t\t\t 6. EXIT\n\t\t\t*********************************\n\n\t\t\tEnter EEPROM operation number to perform :\t");

		scanf("%d", &select);
		switch(select)
		{
			case 1:	/* init EEPROM and set current page position to 0 */
				ret = EEPROM_init();
				if(ret < 0)
				printf("EEPROM not initialised\n");
				else
				printf("EEPROM initialised successfully\n");
				break;

			case 2:	/* Get current page position and Set page position with new value */
				printf("------->Enter page position you wish to set (0 - 511)\n");
				scanf("%ld", &current_page);
				ret = EEPROM_set(current_page);
				if(ret < 0)
					printf("\nnew position entered is out of range. Re-enter page between 0-511\n");

				break;
			
			case 3: /* Write pages to EEPROM */
				printf("------->Enter number of pages to write to EEPROM\t");
				scanf("%d", &num_page);
				message_snd = (char*)malloc(sizeof(char)*64*num_page);
				message = (char *)malloc(sizeof(char)*num_page*64);
				gen_rand(message, num_page*64);
				strcpy(message_snd, message);
				printf("\nMessage SENT from USER to EEPROM : %s\n", message_snd);
				EEPROM_write(message_snd, num_page);
				if(ret == 0)
				free(message_snd);
				break;
		
			case 4: /* Read pages from EEPROM */
				printf("------->Enter number of pages to read from EEPROM\t");
				scanf("%d", &num_page);
				message_rcv = (char*)malloc(sizeof(char)*64*num_page);
				EEPROM_read(message_rcv, num_page);
				message_rcv[64*num_page] = '\0';
				printf("\nMessage RECEIVED by USER from EEPROM : %s\n", message_rcv);
				free(message_rcv);
				break;
		
			case 5: /* Erase EEPROM by writing 1's on 512 pages */
				printf("\nErasing EEPROM...\n");
				EEPROM_erase();
				printf("------->EEPROM successfully Erased \n");
				break;

			case 6: /* Close and Release the device */
				EEPROM_exit();
				printf("\nDevice Closed\n");
				return 0;

			default:
				printf("\nInvalid Option.. Select options from 1-6\n\n");
				break;
		}
	}
}
















