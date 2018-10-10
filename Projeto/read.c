#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define SET_SIZE 5
#define SET_FLAG 0x7E
#define SET_ADDRESS 0x03
#define SET_CONTROL 0x03

#define UA_SIZE 5
#define UA_FLAG 0x7E
#define UA_ADDRESS 0x03
#define UA_CONTROL 0x07

#define INF_INIT_SIZE 4
#define INF_FLAG 0x7E
#define INF_ADDRESS 0x03
#define INF_CONTROL0 0x00
#define INF_CONTROL1 0x40
#define INF_ESCAPE 0x7D
#define INF_OR_FLAG 0x5E
#define INF_OR_ESCAPE 0x5D

int trama = 0;

int setup(int argc, char **argv)
{
    int fd;
    struct termios oldtio, newtio;

    if ((argc < 2) ||
        ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
         (strcmp("/dev/ttyS1", argv[1]) != 0)))
    {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.

  */

    fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(argv[1]);
        exit(-1);
    }

    if (tcgetattr(fd, &oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 1; /* inter-character timer unused */
    newtio.c_cc[VMIN] = 0;  /* blocking read until 5 chars received */

    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    return fd;
}

void llopen(int fd)
{
    char set[5];

    char ua[5];

    ua[0] = UA_FLAG;
    ua[1] = UA_ADDRESS;
    ua[2] = UA_CONTROL;
    ua[3] = UA_ADDRESS ^ UA_CONTROL;
    ua[4] = UA_FLAG;

    int recebido = FALSE;
    int i = 0, res = 0;

    while (!recebido)
    {
        res = read(fd, set + i, 1);

        if (res <= 0)
            continue;

        switch (i)
        {
        case 0:
            if (set[i] != SET_FLAG)
                continue;
            break;
        case 1:
            if (set[i] != SET_ADDRESS)
            {
                if (set[i] != SET_FLAG)
                    i = 0;
                continue;
            }
            break;
        case 2:
            if (set[i] != SET_CONTROL)
            {
                if (set[i] != SET_FLAG)
                    i = 0;
                else
                    i = 1;
                continue;
            }
            break;
        case 3:
            if (set[i] != (SET_ADDRESS ^ SET_CONTROL))
            {
                if (set[i] != SET_FLAG)
                    i = 0;
                else
                    i = 1;
                continue;
            }
            break;
        case 4:
            if (set[i] != SET_FLAG)
            {
                i = 0;
                continue;
            }
            break;
        default:
            break;
        }

        i++;

        if (i == SET_SIZE)
        {
            recebido = TRUE;
            printf("SET recebido!\n");
        }
    }

    int enviado = FALSE;

    while (!enviado)
    {
        res = write(fd, ua, UA_SIZE);
        printf("UA enviado!\n");

        if (res == UA_SIZE)
            enviado = TRUE;
    }
}

int check_initials(int fd)
{
    if(!(trama == 0 || trama == 1))
        return -1;

    char inf[4];

    int recebido = FALSE;
    int i = 0, res = 0;

    while (!recebido)
    {
        res = read(fd, inf + i, 1);

        if (res <= 0)
            continue;

        switch (i)
        {
        case 0:
            if (inf[i] != INF_FLAG)
                continue;
            break;
        case 1:
            if (inf[i] != INF_ADDRESS)
            {
                if (inf[i] != INF_FLAG)
                    i = 0;
                continue;
            }
            break;
        case 2:
            if (!((inf[i] == INF_CONTROL0 && trama == 0) || (inf[i] == INF_CONTROL1 && trama == 1)))
            {
                if (inf[i] != INF_FLAG)
                    i = 0;
                else
                    i = 1;
                continue;
            }
            break;
        case 3:
            if (!((inf[i] == (INF_ADDRESS ^ INF_CONTROL0) && trama == 0) || (inf[i] == (INF_ADDRESS ^ INF_CONTROL1) && trama == 1)))
            {
                if (inf[i] != INF_FLAG)
                    i = 0;
                else
                    i = 1;
                continue;
            }
            break;
        default:
            break;
        }

        i++;

        if (i == INF_INIT_SIZE)
            recebido = TRUE;
    }

    return 0;
}

int llread(int fd, char *buffer)
{
    int res = check_initials(fd);

    if (res < 0)
        return res;

    char data[1];
    char bcc;
    int recebido = FALSE;
    int i = 0;

    while(!recebido)
    {
        res = read(fd,data,1);

        if (res <= 0)
            continue;

        if (data[0] == INF_FLAG)
        {
            if(i == 0)
                return -1;

            recebido = TRUE;
            break;
        }

        //DE-STUFFING

        if (i != 0)
            buffer[i-1] = bcc;

        bcc = data[0];
        i++;
    }

    //i has num char read to buffer

    char check;
    int j = 0;

    for(; j < i; j++)
        check ^= buffer[j];

    if (check != bcc)
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    int fd = setup(argc, argv);
    llopen(fd);

    char data[100];
    if (llread(fd, data) < 1)
        printf("Error reading message\n");

    printf("%s", data);
}
