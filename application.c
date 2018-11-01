#include "common.h"
#include "ll.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

FILE* appLog;

void openAppLogFile()
{
	appLog = fopen("appLog.txt","w");
}

void closeAppLogFile()
{
	fclose(appLog);
}

void print_progress(int progress, int max)
{
	printf("\rProgress[");

	int i = 0;
	for(; i < 10; i++)
	{
		if(i < (progress * 10.0 / max))
			printf("#");

		else printf(" ");
	}

	printf("] %d%%", (int)(progress * 100.0 / max));
	fflush(stdout);
}

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

	memcpy(package + 3, &size, byte_size);

	int i=3+byte_size;

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
	buf[2] = data_size / 256;
	buf[3] = data_size % 256;

	int i;

	for (i = 0; i < data_size; i++)
	{
		buf[i+4] = data[i];
	}

	free(data);
}

int transmit(char *port, char *file, int packet_size)
{
	int serial = llopen(port, TRANSMITTER);
	fprintf(appLog, "Called llopen().\n");

    if (serial < 0)
        return -1;

	FILE* ficheiro = fopen(file, "r");

	if (ficheiro == 0) {
		printf("Error: %s is not a file.\n", file);
		return -1;
    }

	fseek(ficheiro, 0L, SEEK_END);
	int size = ftell(ficheiro);
	fclose(ficheiro);

	int n = 0; //PACKET n = 0 será START, a partir de n = 1 dados

	sendControl(serial, file, size, C_START);
	fprintf(appLog, "Sent START Control Packet.\n");

	n++;

	int fd = open(file,O_RDONLY);

	int res;
	char* buf = (char*) malloc(packet_size);
	int progress = 0;

	while(progress != size)
	{
		print_progress(progress, size);
		res = read(fd, buf, packet_size - 4);

		if(res == 0)
			break;

		setDataPackage(buf, res, n);
		if (llwrite(serial, buf, res + 4) < 0)
			return -1;

		fprintf(appLog, "Sent Data Packet n = %d.\n",n);
		progress += res;

		n++;
	}

	free(buf);

	print_progress(progress, size);
	printf("\n"); //To fix console after progress bar

	sendControl(serial, file, size, C_END);
	fprintf(appLog, "Sent END Control Packet.\n");

	close(fd);

	llclose(serial, TRANSMITTER);

	fprintf(appLog, "llclose() called.\n");

	return 0;
}

int interpretPacket(char* buf, int res, int* file, int n, int* size)
{
	if(buf[0]==C_START)
	{
		memcpy(size, buf + 3, buf[2]);

		int i = 5 + buf[2];
		int j=0;
		char* file_name = (char *)malloc(res-i);

		for(;i<res;i++, j++)
		{
			file_name[j]= buf[i];
		}

		int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC , 0644);
		*file = fd;

		free(file_name);
		fprintf(appLog,"Received START Control Packet.\n");
		return FALSE;
	}

	if(buf[0]==C_END)
	{
		fprintf(appLog,"Received END Control Packet.\n");
		return TRUE;
	}

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
		fprintf(appLog,"Received Data Packet n = %d.\n", buf[1]);
		return FALSE;
	}

	return FALSE;
}

int receive(char *port, int packet_size)
{
    int serial = llopen(port, RECEIVER);

    if (serial < 0)
        return -1;

    char buffer[PACKET_SIZE];

    int res = llread(serial, buffer);

    int file;

	int n = 0;
	int size = 0;

	interpretPacket(buffer, res, &file, n, &size); //PACKET n = 0 será START, a partir de n = 1 dados
	n++;

	char* buf = (char*) malloc(packet_size);

    int end = FALSE;

	int progress = 0;
    while(!end)
    {
		print_progress(progress, size);
        res = llread(serial, buf);
		if(res < 0)
			return res;

        end = interpretPacket(buf, res, &file, n, &size);
		n++;

		progress += res - 4;
    }

	free(buf);

	printf("\n"); //To fix console after progress bar

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

	openAppLogFile();

	#ifdef EFI_SIZE
		int i = 0;
		for(;i < 10; i++)
			transmit(argv[2], argv[3], 10*(i+1));
	#else
		transmit(argv[2], argv[3], PACKET_SIZE);
	#endif

	closeAppLogFile();

		return 0;
    }

	if (argc == 3)
    {
        if (strcmp(argv[1],"receive"))
        {
            printf("Usage: [transmit/receive] SerialPort [filename]\n");
            return -1;
        }

	openAppLogFile();

	#ifdef EFI_SIZE
		int i = 0;
		for(; i < 10; i++)
			receive(argv[2], 10*(i+1));
	#else
		receive(argv[2], PACKET_SIZE);
	#endif

	closeAppLogFile();

        return 0;
    }

	printf("Usage: [transmit/receive] SerialPort [filename]\n");
    return 1;
}
