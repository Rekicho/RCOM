#ifndef LL_H
#define LL_H

#define C_START 0x02
#define C_END 0x03
#define C_DATA 0x01
#define PACKET_SIZE 260

int llopen(char *port, int flag);
int llwrite(int fd, char *buffer, int length);
int llread(int fd, char *buffer);
int llclose(int fd, int flag);

#endif
