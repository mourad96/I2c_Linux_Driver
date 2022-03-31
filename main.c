#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include "adxl345.h"

int main() {
    int16_t read_buf[3];
    //int16_t acclero[3];
	int dev = open("/dev/adxl345-0", O_RDONLY);
	if(dev == -1) {
		printf("Opening was not possible!\n");
		return -1;
	}
	printf("Opening was successfull!\n");
	int fd1 = ioctl(dev, cmd_xyz);
    if (fd1 == -1)
    {
        printf("error ioctl\n");
        return -1;
    }
	for (int i = 0; i < 64; i++)
	{
		read(dev, read_buf, 6);

    	printf("x = %d \n",read_buf[0]);
		printf("y = %d \n",read_buf[1]);
		printf("z = %d \n",read_buf[2]);

	}
	
    
	
	close(dev);
	return 0;
}