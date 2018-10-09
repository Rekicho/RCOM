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

#define SET_FLAG 0x7E
#define SET_ADDRESS 0x03
#define SET_CONTROL 0x03

#define UA_SIZE 5
#define UA_FLAG 0x7E
#define UA_ADDRESS 0x03
#define UA_CONTROL 0x07

volatile int STOP = FALSE;

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

        if (i == 5)
            recebido = TRUE;
    }

    int enviado = FALSE;

    while (!enviado)
    {
        res = write(fd, ua, UA_SIZE);

        if(res == UA_SIZE)
            enviado = TRUE;
    }
}

int main(int argc, char **argv)
{
    int fd = setup(argc, argv);
    llopen(fd);
}