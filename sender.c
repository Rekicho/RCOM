#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#define C_START 0x02
#define C_END 0x03
#define DATA_C 0x01
#define PACKET_SIZE 256

int setup(int argc, char **argv);
void llopen(int fd);
int llwrite(int fd, char *buffer, int length);

int setDataPackage(char* buf, int ds, int n) {

	char data[ds];
 	strcpy(data, buf);
	
	int data_size = ds;
	int package_size = ds + 4;

	buf = (char *)malloc(package_size);
	int l1;
	int l2;

	l1 = data_size % 256; 
	l2 = data_size / 256;

	buf[0] = DATA_C;
	buf[1] = n % 256;
	buf[2] = l1;
	buf[3] = l2;

	int i;

	for (i = 0; i < ds; i++) {
		buf[i+4] = data[i];
	}
	
	return 0;	
}

int sendControl(FILE* fd, char* name, int size, int option, int porta) {
	 
	int package_size = 5;
	int size_length = 1;
	int name_length = strlen(name) + 1;
	
	int byte_size = ceil(log2((double)size+1.0)/8);
	printf("%d\n",size);

	package_size += byte_size;
	package_size += name_length;


	char *package;
	package = (char *)malloc(package_size);
	//CONFIRMAR TAMANHO

	printf("%d\n",package_size);
	package[0] = option;
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

	llwrite(porta, package, package_size);

	return 0;
}

int main(int argc, char *argv[])
{
    int porta = setup(argc, argv);
	llopen(porta);

	char *file = "pinguim.gif";
	int size;	

	FILE* fd = fopen(file, "r");
	int ficheiro = open("pinguim.gif",O_RDONLY);
	
	if (fd == 0) {
		printf("Error: Not a file\n");
		return -1;
    }
    
	fseek(fd, 0L, SEEK_END);
	size = ftell(fd)-1;

	sendControl(fd, file, size, C_START, porta);

	int res = 0;
	char buf[PACKET_SIZE + 4];
	int n = 0;
	do
	{
		res = read(ficheiro, buf, PACKET_SIZE);
		//setDataPackage(buf, res, n);
		llwrite(porta, buf, res);
		n++;
	} while(res != 0);

	sendControl(fd, file, size, C_END, porta);
	
    return 0;
}
