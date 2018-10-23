#include "ll.h"

int receive(char *port)
{
    int serial = llopen(port, RECEIVER);

    if (serial < 0)
    {
        printf("Couldn't open serial port %s\n", port);
        return -1;
    }

    char buffer[PACKET_SIZE];

    int res = llread(serial, buffer);

    int file = interpretPacket(buffer, res);

    int end = FALSE;

    while(!end)
    {
        res = llread(serial, buffer);
        end = interpretPacket(buffer, res);
    }

    return llclose(port, buffer);
}

int transmit(char *port, char *file)
{
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 4)
    {
        printf("Usage: [transmit/receive] SerialPort [filename]\n");
        return -1;
    }

    if (argc == 3)
    {
        if (argv[1] != "receive")
        {
            printf("Usage: [transmit/receive] SerialPort [filename]\n");
            return -1;
        }

        return receive(argv[2]);
    }

    if (argc == 4)
    {
        if (argv[1] != "transmit")
        {
            printf("Usage: [transmit/receive] SerialPort [filename]\n");
            return -1;
        }

        return transmit(argv[2], argv[3]);
    }

    return 1;
}
