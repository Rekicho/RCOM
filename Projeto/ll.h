#ifndef LL_H
#define LL_H

#define FALSE 0
#define TRUE 1

#define TRANSMITTER 0
#define RECEIVER 1

#define C_START 0x02
#define C_END 0x03
#define DATA_C 0x01
#define PACKET_SIZE 260

int llopen(char *port, int flag);
int llwrite(int fd, char *buffer, int length);
int llread(int fd, char *buffer);
int llclose(int fd, int flag);

#endif