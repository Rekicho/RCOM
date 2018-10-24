#include "common.h"
#include "ll.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

void sendControl(int porta, char* name, int size, int option) {
	 
	int package_size = 5;
	int name_length = strlen(name) + 1;
	
	int byte_size = ceil(log2((double)size+1.0)/8);

	package_size += byte_size;
	package_size += name_length;


	char *package;
	package = (char *)malloc(package_size);

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

	llwrite(porta, package, package_size);

	free(package);
}

void setDataPackage(char* buf, int data_size, int n) {

	char* data = (char *)malloc(data_size);
 	memcpy(data, buf, data_size);

	buf[0] = C_DATA;
	buf[1] = n % 256;
	buf[2] = data_size % 256;
	buf[3] = data_size / 256;

	int i;

	for (i = 0; i < data_size; i++) 
	{
		buf[i+4] = data[i];
	}	

	free(data);
}

int transmit(char *port, char *file)
{
	int serial = llopen(port, TRANSMITTER);

    if (serial < 0)
        return -1;

	FILE* ficheiro = fopen(file, "r");

	if (ficheiro == 0) {
		printf("Error: %s is not a file.\n", file);
		return -1;
    }

	fseek(ficheiro, 0L, SEEK_END);
	int size = ftell(ficheiro)-1;

	fclose(ficheiro);

	int n = 0; //PACKET n = 0 será START, a partir de n = 1 dados

	sendControl(serial, file, size, C_START);

	n++;

	int fd = open(file,O_RDONLY);

	int res;
	char buf[PACKET_SIZE];

	while(res != 0)
	{
		res = read(fd, buf, PACKET_SIZE - 4);

		if(res == 0)
			break;

		setDataPackage(buf, res, n);
		if (llwrite(serial, buf, res + 4) < 0)
			return -1;

		n++;
	}

	sendControl(serial, file, size, C_END);  

	close(file);

	return llclose(serial, TRANSMITTER);
}

int interpretPacket(char* buf, int res, int* file, int n)
{
	if(buf[0]==C_START)
	{
		int i=5 + buf[2];
		int j=0;
		char* file_name = (char *)malloc(res-i);
		
		for(;i<res;i++, j++)
		{
			file_name[j]=buf[i];
		}

		int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC , 0644);
		*file = fd;

		free(file_name);
		return FALSE;
	}

	if(buf[0]==C_END)	
		return TRUE;

	if(buf[0]!=C_DATA)
		printf("Campo Controlo Packet %d não conhecido, assumindo 1.\n", n);

	if(buf[0]==C_DATA)
	{
		char* data = (char *)malloc(res - 4);

		int i;

		for (i = 0; i < res - 4; i++) 
		{
			data[i] = buf[i+4];
		}

		write(*file, data, res - 4);

		free(data);
		return FALSE;
	}

	return FALSE;	
}

int receive(char *port)
{
    int serial = llopen(port, RECEIVER);

    if (serial < 0)
        return -1;

    char buffer[PACKET_SIZE];

    int res = llread(serial, buffer);

    int file;

	int n = 0;

	interpretPacket(buffer, res, &file, n); //PACKET n = 0 será START, a partir de n = 1 dados

	n++;

    int end = FALSE;

    while(!end)
    {
        res = llread(serial, buffer);
		if(res < 0)
			return res;

        end = interpretPacket(buffer, res, &file, n);
		n++;
    }

	close(file);

    return llclose(serial, RECEIVER);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 4)
    {
        printf("Usage: [transmit/receive] SerialPort [filename]\n");
        return -1;
    }

    if (argc == 4)
    {
        if (strcmp(argv[1],"transmit"))
        {
            printf("Usage: [transmit/receive] SerialPort [filename]\n");
            return -1;
        }

        return transmit(argv[2], argv[3]);
    }

	if (argc == 3)
    {
        if (strcmp(argv[1],"receive"))
        {
            printf("Usage: [transmit/receive] SerialPort [filename]\n");
            return -1;
        }

        return receive(argv[2]);
    }

	printf("Usage: [transmit/receive] SerialPort [filename]\n");
    return 1;
}
