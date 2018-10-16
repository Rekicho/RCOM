#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

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

#define INF_FLAG 0x7E
#define INF_ADDRESS 0x03
#define INF_CONTROL0 0x00
#define INF_CONTROL1 0x40
#define INF_ESCAPE 0x7D
#define INF_XOR_FLAG 0x5E
#define INF_XOR_ESCAPE 0x5D

#define MAX_ALARMS 3

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

int flag_alarme = 0, conta_alarme = 0;

void atende_alarme()
{
	flag_alarme = 1;
	conta_alarme++;
	printf("Alarme %d\n", conta_alarme);
}

void desativa_alarme()
{
	flag_alarme = 0;
	alarm(0);
}

void llopen(int fd)
{
	char set[5];

	set[0] = SET_FLAG;
	set[1] = SET_ADDRESS;
	set[2] = SET_CONTROL;
	set[3] = SET_ADDRESS ^ SET_CONTROL;
	set[4] = SET_FLAG;

	char ua[5];

	(void)signal(SIGALRM, atende_alarme);

	int recebido = FALSE;
	int i = 0, res = 0;

	while (conta_alarme <= MAX_ALARMS && !recebido)
	{
		desativa_alarme();

		res = write(fd, set, SET_SIZE);
		printf("SET enviado!\n");

		if (res <= 0)
			continue;

		alarm(3);

		while (!flag_alarme && !recebido)
		{
			res = read(fd, ua + i, 1);

			if (res <= 0)
				continue;

			switch (i)
			{
			case 0:
				if (ua[i] != UA_FLAG)
					continue;
				break;
			case 1:
				if (ua[i] != UA_ADDRESS)
				{
					if (ua[i] != UA_FLAG)
						i = 0;
					continue;
				}
				break;
			case 2:
				if (ua[i] != UA_CONTROL)
				{
					if (ua[i] != UA_FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 3:
				if (ua[i] != (UA_ADDRESS ^ UA_CONTROL))
				{
					if (ua[i] != UA_FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 4:
				if (ua[i] != UA_FLAG)
				{
					i = 0;
					continue;
				}
				break;
			default:
				break;
			}

			i++;

			if (i == UA_SIZE)
			{
				recebido = TRUE;
				printf("UA recebido!\n");
				desativa_alarme();
			}
		}
	}
}

int llwrite(int fd, char *buffer, int length)
{
	if (length <= 0)
		return -1;

	char bcc2 = 0;
	int i = 0;

	for (; i < length; i++)
		bcc2 ^= buffer[i];

	char *buf;

	buf = (char *)malloc((length + 1) * 2 + 5);

	buf[0] = INF_FLAG;
	buf[1] = INF_ADDRESS;

	if (trama == 0)
	{
		buf[2] = INF_CONTROL0;
		buf[3] = INF_ADDRESS ^ INF_CONTROL0;
	}

	else if (trama == 1)
	{
		buf[2] = INF_CONTROL1;
		buf[3] = INF_ADDRESS ^ INF_CONTROL1;
	}

	else
		return -1;

	i = 0;
	int j = 4;

	for (; i < length; i++, j++)
	{
		if (buffer[i] == INF_FLAG)
		{
			buf[j] = INF_ESCAPE;
			j++;
			buf[j] = INF_XOR_FLAG;
		}

		else if (buffer[i] == INF_ESCAPE)
		{
			buf[j] = INF_ESCAPE;
			j++;
			buf[j] = INF_XOR_ESCAPE;
		}

		else
			buf[j] = buffer[i];
	}

	if (bcc2 == INF_FLAG)
	{
		buf[j] = INF_ESCAPE;
		j++;
		buf[j] = INF_XOR_FLAG;
	}

	else if (bcc2 == INF_ESCAPE)
	{
		buf[j] = INF_ESCAPE;
		j++;
		buf[j] = INF_XOR_ESCAPE;
	}

	else
		buf[j] = bcc2;

	j++;

	buf[j] = INF_FLAG;

	j++; // j contém numero de chars usados

	int res = write(fd, buf, j);

	free(buf);

	if (res == j)
		printf("Trama I%d enviada!\n", trama);

	else
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	int fd = setup(argc, argv);
	llopen(fd);
	if (llwrite(fd, "}~]^", 5) < 0)
		printf("Error sending trama");
}
