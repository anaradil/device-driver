#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int main(int argc, char * argv[]) {
	char buf[256];
	int fwp, chars, flag = 0;
	FILE *frp;
	if(argc == 3) 
	{
		printf("Please, enter a password (10 characters): \n");

		do{
			if(gets(buf) == NULL) {
				printf("Password should be 10 characters");
				return 0;
			}
		}while(strlen(buf) != 10);

		if((fwp = open(argv[2], O_WRONLY) < 0) || (frp = fopen(argv[1], "r")) == NULL) 
		{
			printf("Couldn't open files");
			return 0;
		}
		
		if(write(fwp, buf, 10) != 10)
		{
			printf("Couldn't write password to a device");
			return 0;
		}

		do{
			chars = fgetc(frp);
			putchar(chars);
			if(chars == EOF) 
			{
				flag = 1;
			}
			if(chars < 0) 
			{
				printf("No data in the file");
				return 0;
			}
			char put = chars;
			if(write(fwp, &put, 1) != 1) 
			{
				printf("Couldn't write data to a device");
				return 0;
			}

		}while(flag == 0);

		return 0;
	} else {
		printf("Please, enter input file name and device file name");
	}
}
