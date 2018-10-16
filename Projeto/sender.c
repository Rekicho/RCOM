#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>

#define C_START 0x02
#define C_END 0x03


int sendStart(FILE* fd, char* name, int size) {
	 
	int package_size = 5;
	int size_length = 1;
	int byte_size = 256;

	while (1) {

		if (size < byte_size) {
			break;
		} else {
			byte_size *= 2;
			size_length++;
		}
	}
	package_size += size_length;
	package_size += strlen(name);

	char package[package_size];
	package[0] = C_START;
	package[1] = 0x00;
	int i = 0;
/*	
	for(i = 0; i < size_length; i++) {
		package[2+i] = ((int)((long)size/pow(256,i))%256);
	}
*/

	return 0;
}

int main(int argc, char *argv[])
{
    char *file = "toy.txt";
	int size;	

	FILE* fd = fopen(file, "r");
	
	if (fd == 0) {
		printf("Error: Not a file\n");
		return -1;
    }
    
	fseek(fd, 0L, SEEK_END);
	size = ftell(fd);

	sendStart(fd, file, size);
	
	printf("%d\n", size);

    return 0;
}
