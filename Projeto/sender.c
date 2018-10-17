#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>

#define C_START 0x02
#define C_END 0x03


int sendStart(FILE* fd, char* name, int size) {
	 
	int package_size = 5;
	int size_length = 1;
	int name_length = strlen(name);
	
	int byte_size = ceil(log2((double)size+1.0)/8);
	printf("%d\n",size);

	package_size += byte_size;
	package_size += name_length;


	char *package;
	package = (char *)malloc(package_size);
	//CONFIRMAR TAMANHO

	printf("%d\n",package_size);
	package[0] = C_START;
	package[1] = 0x00;
	package[2] = byte_size;
	int z=0;
		
	int i=byte_size+2;
	for(; i>2;i--)
	{	
		
		package[i]= (size >> (8*(z))) & 0xFF;
		z++;
	}
	i=3+byte_size;
	package[i]=0x01;
	package[i+1]=name_length;

	int j;	
	for(j=0;j<name_length;j++)
	{
		package[i+j+2]= name[j];
	}	

	
	for(z=0;z<package_size;z++)

	{	if(z<i+2 )
			printf("%d\n",package[z]);
		else
			printf("%d\n",package[z]);
	}

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
	size = ftell(fd)-1;

	sendStart(fd, file, size);
	


    return 0;
}
