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
#define UA_ADDRESS_SENDER 0x03
#define UA_ADDRESS_RECEIVER 0x01
#define UA_CONTROL 0x07

#define INF_INIT_SIZE 4
#define INF_FLAG 0x7E
#define INF_ADDRESS 0x03
#define INF_CONTROL0 0x00
#define INF_CONTROL1 0x40
#define INF_ESCAPE 0x7D
#define INF_XOR_FLAG 0x5E
#define INF_XOR_ESCAPE 0x5D

#define RR_SIZE 5
#define RR_FLAG 0x7E
#define RR_ADDRESS 0x03
#define RR_CONTROL0 0x05
#define RR_CONTROL1 0x85
#define REJ_CONTROL0 0x01
#define REJ_CONTROL1 0x81

#define DISC_SIZE 5
#define DISC_FLAG 0x7E
#define DISC_ADDRESS_SENDER 0x03
#define DISC_ADDRESS_RECEIVER 0x01
#define DISC_CONTROL 0x0B

#define PACKET_SIZE 256

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
    ua[1] = UA_ADDRESS_SENDER;
    ua[2] = UA_CONTROL;
    ua[3] = UA_ADDRESS_SENDER ^ UA_CONTROL;
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
    char inf[4];

    int recebido = FALSE;
    int i = 0, res = 0;
	int temp_trama = -1;

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
			if (inf[i] == INF_CONTROL0)
					temp_trama = 0;
			else if (inf[i] == INF_CONTROL1)
					temp_trama = 1;
            else
            {
                if (inf[i] != INF_FLAG)
                    i = 0;
                else
                    i = 1;
                continue;
            }
            break;
        case 3:
            if (!((inf[i] == (INF_ADDRESS ^ INF_CONTROL0) && temp_trama == 0) || (inf[i] == (INF_ADDRESS ^ INF_CONTROL1) && temp_trama == 1)))
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

	return temp_trama;
}

void sendRR(int fd, int rej){
	char rr[5];

    rr[0] = RR_FLAG;
    rr[1] = RR_ADDRESS;

	if(trama == 0)
	{
		if(rej)
		{
			rr[2] = REJ_CONTROL0;
    		rr[3] = RR_ADDRESS ^ REJ_CONTROL0;
		}

		else
		{
			rr[2] = RR_CONTROL0;
    		rr[3] = RR_ADDRESS ^ RR_CONTROL0;
		}
	}

	else 
	{
		if(rej)
		{
			rr[2] = REJ_CONTROL1;
    		rr[3] = RR_ADDRESS ^ REJ_CONTROL1;
		}

		else
		{
			rr[2] = RR_CONTROL1;
    		rr[3] = RR_ADDRESS ^ RR_CONTROL1;
		}
	}

    rr[4] = RR_FLAG;

	int enviado = FALSE;
	int res;

    while (!enviado)
    {
        res = write(fd, rr, RR_SIZE);

        if (res == RR_SIZE)
            enviado = TRUE;
    }
}

int llread(int fd, char *buffer)
{
	int certo = FALSE;
	int rej;
	int temp_trama;
	char data;
	char bcc;
	int recebido;
	int i;
	int destuffing;
	int res;
	
	while(!certo)
	{
		temp_trama = check_initials(fd);

		if (temp_trama < 0)
		    return temp_trama;

		rej = FALSE;
		recebido = FALSE;
		i = 0;
		destuffing = FALSE;
		res = 0;


		while(!recebido)
		{
		    res = read(fd,&data,1);

		    if (res <= 0)
		        continue;

			if (destuffing)
			{
				destuffing = FALSE;
			
				if(data == INF_XOR_FLAG)
					data = INF_FLAG;

				else if(data == INF_XOR_ESCAPE)
					data = INF_ESCAPE;

				else return -1;
			}

		    else if (data == INF_FLAG)
		    {
		        if(i == 0)
		            return -1;

		        recebido = TRUE;
		        break;
		    }

		    //DE-STUFFING
			else if (data == INF_ESCAPE)
			{
				destuffing = TRUE;
				continue;
			}
			

		    if (i != 0)
		        buffer[i-1] = bcc;

		    bcc = data;
		    i++;
		}

		i--; //BCC doesn't count

		//i has num char read to buffer

		char check = 0;
		int j = 0;

		for(; j < i; j++)
			check ^= buffer[j];

		if (check != bcc)
		    rej = TRUE;

		printf("Trama %d recebida!\n", temp_trama);

		if(rej && temp_trama == trama)
		{
			sendRR(fd,rej);	
			printf("REJ%d enviado!\n", trama);
			continue;
		}

		if(temp_trama != trama)
		{
			sendRR(fd,rej);
			printf("RR%d re-enviado!\n", trama);
			continue;
		}	

		certo = TRUE;
	
		if(trama == 0)
			trama = 1;

		else trama = 0;

		sendRR(fd,rej);
		printf("RR%d enviado!\n", trama);
	}

    return i;
}

int llclose(int fd)
{
    char disc_sender[5];

    char disc_receiver[5];

    disc_receiver[0] = DISC_FLAG;
    disc_receiver[1] = DISC_ADDRESS_RECEIVER;
    disc_receiver[2] = DISC_CONTROL;
    disc_receiver[3] = DISC_ADDRESS_RECEIVER ^ DISC_CONTROL;
    disc_receiver[4] = DISC_FLAG;

    int recebido = FALSE;
    int i = 0, res = 0;

    while (!recebido)
    {
        res = read(fd, disc_sender + i, 1);

        if (res <= 0)
            continue;

        switch (i)
        {
        case 0:
            if (disc_sender[i] != DISC_FLAG)
                continue;
            break;
        case 1:
            if (disc_sender[i] != DISC_ADDRESS_SENDER)
            {
                if (disc_sender[i] != DISC_FLAG)
                    i = 0;
                continue;
            }
            break;
        case 2:
            if (disc_sender[i] != DISC_CONTROL)
            {
                if (disc_sender[i] != DISC_FLAG)
                    i = 0;
                else
                    i = 1;
                continue;
            }
            break;
        case 3:
            if (disc_sender[i] != (DISC_ADDRESS_SENDER ^ DISC_CONTROL))
            {
                if (disc_sender[i] != DISC_FLAG)
                    i = 0;
                else
                    i = 1;
                continue;
            }
            break;
        case 4:
            if (disc_sender[i] != DISC_FLAG)
            {
                i = 0;
                continue;
            }
            break;
        default:
            break;
        }

        i++;

        if (i == DISC_SIZE)
        {
            recebido = TRUE;
            printf("DISC recebido!\n");
        }
    }

    int enviado = FALSE;

    while (!enviado)
    {
        res = write(fd, disc_receiver, DISC_SIZE);
        printf("DISC enviado!\n");

        if (res == DISC_SIZE)
            enviado = TRUE;
    }

	return 1;
}

int main(int argc, char **argv)
{
    int fd = setup(argc, argv);
    llopen(fd);

    FILE* fp = fopen("pinguim.gif", "ab+");

    int ficheiro = open("pinguim.gif", O_WRONLY);

	int res;
char first = 0;

    char data[PACKET_SIZE + 4];
    if ((res = llread(fd, data)) < 0)
	{
		printf("Error reading message\n");
		return -1;
	}

   
while(1)
{
	res = llread(fd, data);
	first = data[0];

	if(first == 3)
	 return 0;
	write(ficheiro,data,res);
}


 
}
