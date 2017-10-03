#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int main(int argc, char * argv[]) {
	char buf[256];
	int size = 10, i; 
	if(argc == 3) 
	{
		int frp, fwp;
		if((frp = open(argv[1], O_RDONLY)) < 0 || (fwp = open(argv[2], O_WRONLY)) < 0) 
		{
			printf("Couldn't open files");
			return 0;
		}
		if(read(frp, buf, size) < 0) 
		{
			printf("Reading error");
			return 0;
		}
		
		if(write(fwp,buf,size) < 0) 
		{
			printf("Writing error");
			return 0;
		}
		
	} else {
		printf("Please, enter device file name and output file name");
	}
	
}
