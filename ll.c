#include "common.h"
#include "constants.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <signal.h>

int trama = 0;
int flag_alarme = 0;
int conta_alarme = 0;

#ifdef EFI_BAUDRATE
int indice = 0;
int baudrate[8];

#endif

#ifdef LOG
FILE *llLog;

void openLLLogFile()
{
	llLog = fopen("llLog.txt", "a");
}

void closeLLLogFile()
{
	fclose(llLog);
}
#endif

void atende_alarme()
{
	flag_alarme = 1;
	conta_alarme++;

#ifdef LOG
	fprintf(llLog, "Alarme %d\n", conta_alarme);
#endif
}

void desativa_alarme()
{
	flag_alarme = 0;
	alarm(0);
}

int setup(char *port)
{
	int fd;
	struct termios oldtio, newtio;

	if (((strcmp("/dev/ttyS0", port) != 0) && (strcmp("/dev/ttyS1", port) != 0)))
	{
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		exit(1);
	}

	/*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.

  */

	fd = open(port, O_RDWR | O_NOCTTY);
	if (fd < 0)
	{
		perror(port);
		exit(-1);
	}

	if (tcgetattr(fd, &oldtio) == -1)
	{ /* save current port settings */
		perror("tcgetattr");
		exit(-1);
	}

	bzero(&newtio, sizeof(newtio));

	#ifdef EFI_BAUDRATE
	if(indice == 0)
	{
		baudrate[0] = B38400;
		baudrate[1] = B19200;
		baudrate[2] = B9600;
		baudrate[3] = B4800;
		baudrate[4] = B2400;
		baudrate[5] = B1200;
		baudrate[6] = B600;
		baudrate[7] = B300;
	}
	newtio.c_cflag = baudrate[indice] | CS8 | CLOCAL | CREAD;

	indice++;

	#else
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	#endif

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

#ifdef LOG
	openLLLogFile();

	fprintf(llLog, "New termios structure set\n");
#endif

	struct sigaction action;
	action.sa_handler = atende_alarme;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);

	return fd;
}

int llopenTransmitter(int fd)
{
	char set[SUP_SIZE];

	set[0] = FLAG;
	set[1] = ADDRESS_SENDER;
	set[2] = SET_CONTROL;
	set[3] = ADDRESS_SENDER ^ SET_CONTROL;
	set[4] = FLAG;

	char ua[SUP_SIZE];

	int recebido = FALSE;
	int i = 0, res = 0;

	while (conta_alarme <= MAX_ALARMS && !recebido)
	{
		desativa_alarme();

		res = write(fd, set, SUP_SIZE);

		if (res != SUP_SIZE)
			continue;

#ifdef LOG
		fprintf(llLog, "SET enviado!\n");
#endif

		alarm(3);

		i = 0;

		while (!flag_alarme && !recebido)
		{
			res = read(fd, ua + i, 1);

			if (res <= 0)
				continue;

			switch (i)
			{
			case 0:
				if (ua[i] != FLAG)
					continue;
				break;
			case 1:
				if (ua[i] != ADDRESS_SENDER)
				{
					if (ua[i] != FLAG)
						i = 0;
					continue;
				}
				break;
			case 2:
				if (ua[i] != UA_CONTROL)
				{
					if (ua[i] != FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 3:
				if (ua[i] != (ADDRESS_SENDER ^ UA_CONTROL))
				{
					if (ua[i] != FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 4:
				if (ua[i] != FLAG)
				{
					i = 0;
					continue;
				}
				break;
			default:
				break;
			}

			i++;

			if (i == SUP_SIZE)
			{
				recebido = TRUE;

#ifdef LOG
				fprintf(llLog, "UA recebido!\n");
#endif

				desativa_alarme();
			}
		}
	}

	if (!recebido)
		return -1;

	conta_alarme = 0;

	return 0;
}

int llopenReceiver(int fd)
{
	char set[SUP_SIZE];

	char ua[SUP_SIZE];

	ua[0] = FLAG;
	ua[1] = ADDRESS_SENDER;
	ua[2] = UA_CONTROL;
	ua[3] = ADDRESS_SENDER ^ UA_CONTROL;
	ua[4] = FLAG;

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
			if (set[i] != FLAG)
				continue;
			break;
		case 1:
			if (set[i] != ADDRESS_SENDER)
			{
				if (set[i] != FLAG)
					i = 0;
				continue;
			}
			break;
		case 2:
			if (set[i] != SET_CONTROL)
			{
				if (set[i] != FLAG)
					i = 0;
				else
					i = 1;
				continue;
			}
			break;
		case 3:
			if (set[i] != (ADDRESS_SENDER ^ SET_CONTROL))
			{
				if (set[i] != FLAG)
					i = 0;
				else
					i = 1;
				continue;
			}
			break;
		case 4:
			if (set[i] != FLAG)
			{
				i = 0;
				continue;
			}
			break;
		default:
			break;
		}

		i++;

		if (i == SUP_SIZE)
		{
			recebido = TRUE;

#ifdef LOG
			fprintf(llLog, "SET recebido!\n");
#endif
		}
	}

	int enviado = FALSE;

	while (!enviado)
	{
		res = write(fd, ua, SUP_SIZE);

#ifdef LOG
		fprintf(llLog, "UA enviado!\n");
#endif

		if (res == SUP_SIZE)
			enviado = TRUE;
	}

	return 0;
}

int llopen(char *port, int flag)
{
	int fd = setup(port);
	int res;

	if (flag == TRANSMITTER)
		res = llopenTransmitter(fd);

	else if (flag == RECEIVER)
		res = llopenReceiver(fd);

	if (res < 0)
		return res;

	return fd;
}

void sendRR(int fd, int rej)
{
	char rr[5];

	rr[0] = FLAG;
	rr[1] = ADDRESS_SENDER;

	if (trama == 0)
	{
		if (rej)
		{
			rr[2] = REJ_CONTROL0;
			rr[3] = ADDRESS_SENDER ^ REJ_CONTROL0;
		}

		else
		{
			rr[2] = RR_CONTROL0;
			rr[3] = ADDRESS_SENDER ^ RR_CONTROL0;
		}
	}

	else
	{
		if (rej)
		{
			rr[2] = REJ_CONTROL1;
			rr[3] = ADDRESS_SENDER ^ REJ_CONTROL1;
		}

		else
		{
			rr[2] = RR_CONTROL1;
			rr[3] = ADDRESS_SENDER ^ RR_CONTROL1;
		}
	}

	rr[4] = FLAG;

	int enviado = FALSE;
	int res;

	while (!enviado)
	{
		res = write(fd, rr, SUP_SIZE);

		if (res == SUP_SIZE)
			enviado = TRUE;
	}
}

int llwrite(int fd, char *buffer, int length)
{
	if (length <= 0 || !(trama == 0 || trama == 1))
		return -1;

	char bcc2 = 0;
	int i = 0;

	for (; i < length; i++)
		bcc2 ^= buffer[i];

	char *buf;

	buf = (char *)malloc((length + 1) * 2 + 5);

	buf[0] = FLAG;
	buf[1] = ADDRESS_SENDER;

	if (trama == 0)
	{
		buf[2] = INF_CONTROL0;
		buf[3] = ADDRESS_SENDER ^ INF_CONTROL0;
	}

	else if (trama == 1)
	{
		buf[2] = INF_CONTROL1;
		buf[3] = ADDRESS_SENDER ^ INF_CONTROL1;
	}

	i = 0;
	int j = 4;

	for (; i < length; i++, j++)
	{
		if (buffer[i] == FLAG)
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

	if (bcc2 == FLAG)
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

	buf[j] = FLAG;

	j++; // j contém numero de chars usados

	char rr[5];
	int res;
	int recebido = FALSE;
	i = 0;
	int temp_trama = -1;
	int rej = FALSE;

	while (conta_alarme <= MAX_ALARMS && !recebido)
	{
		desativa_alarme();

		res = write(fd, buf, j);

		if (res != j)
			continue;

#ifdef LOG
		fprintf(llLog, "Trama I%d enviada!\n", trama);
#endif

		alarm(3);

		i = 0;

		while (!flag_alarme && !recebido)
		{
			res = read(fd, rr + i, 1);

			if (res <= 0)
				continue;

			switch (i)
			{
			case 0:
				if (rr[i] != FLAG)
					continue;
				break;
			case 1:
				if (rr[i] != ADDRESS_SENDER)
				{
					if (rr[i] != FLAG)
						i = 0;
					continue;
				}
				break;
			case 2:
				if (rr[i] == (char)RR_CONTROL0)
					temp_trama = 0;
				else if (rr[i] == (char)RR_CONTROL1)
					temp_trama = 1;
				else if (rr[i] == (char)REJ_CONTROL0)
				{
					rej = TRUE;
					temp_trama = 0;
				}
				else if (rr[i] == (char)REJ_CONTROL1)
				{
					rej = TRUE;
					temp_trama = 1;
				}
				else
				{
					if (rr[i] != FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 3:
				if ((rej && ((rr[i] == (char)(ADDRESS_SENDER ^ REJ_CONTROL0) && temp_trama == 0) || (rr[i] == (char)(ADDRESS_SENDER ^ REJ_CONTROL1) && temp_trama == 1))) || (!rej && ((rr[i] == (char)(ADDRESS_SENDER ^ RR_CONTROL0) && temp_trama == 0) || (rr[i] == (char)(ADDRESS_SENDER ^ RR_CONTROL1) && temp_trama == 1))))
					break;
				else
				{
					if (rr[i] != FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
			case 4:
				if (rr[i] != FLAG)
				{
					i = 0;
					continue;
				}
				break;
			default:
				break;
			}

			i++;

			if (i == SUP_SIZE)
			{
				desativa_alarme();
				recebido = TRUE;

#ifdef LOG
				if (rej)
					fprintf(llLog, "REJ%d recebido!\n", temp_trama);

				else
					fprintf(llLog, "RR%d recebido!\n", temp_trama);
#endif
			}
		}

		if (!recebido)
			continue;

		conta_alarme = 0;

		if (rej || trama == temp_trama)
		{
			temp_trama = -1;
			recebido = FALSE;
			rej = FALSE;

#ifdef LOG
			fprintf(llLog, "Re-sending trama I%d!\n", trama);
#endif

			continue;
		}

		if (trama == 0)
			trama = 1;

		else
			trama = 0;
	}

	if (!recebido)
		return -1;

	free(buf);

	return j;
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
			if (inf[i] != FLAG)
				continue;
			break;
		case 1:
			if (inf[i] != ADDRESS_SENDER)
			{
				if (inf[i] != FLAG)
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
				if (inf[i] != FLAG)
					i = 0;
				else
					i = 1;
				continue;
			}
			break;
		case 3:
			if (!((inf[i] == (ADDRESS_SENDER ^ INF_CONTROL0) && temp_trama == 0) || (inf[i] == (ADDRESS_SENDER ^ INF_CONTROL1) && temp_trama == 1)))
			{
				if (inf[i] != FLAG)
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

		if (i == INF_HEADER_SIZE)
			recebido = TRUE;
	}

	return temp_trama;
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

	while (!certo)
	{
		temp_trama = check_initials(fd);

		if (temp_trama < 0)
			return temp_trama;

		rej = FALSE;
		recebido = FALSE;
		i = 0;
		destuffing = FALSE;
		res = 0;

		while (!recebido)
		{
			res = read(fd, &data, 1);

			if (res <= 0)
				continue;

			if (destuffing)
			{
				destuffing = FALSE;

				if (data == INF_XOR_FLAG)
					data = FLAG;

				else if (data == INF_XOR_ESCAPE)
					data = INF_ESCAPE;

				else
					return -1;
			}

			else if (data == FLAG)
			{
				if (i == 0)
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
				buffer[i - 1] = bcc;

			bcc = data;
			i++;
		}

		i--; //BCC doesn't count

		//i has num char read to buffer

		char check = 0;
		int j = 0;

		for (; j < i; j++)
			check ^= buffer[j];

		if (check != bcc)
			rej = TRUE;

#ifdef LOG
		fprintf(llLog, "Trama %d recebida!\n", temp_trama);
#endif

		if (rej && temp_trama == trama)
		{
			sendRR(fd, rej);

#ifdef LOG
			fprintf(llLog, "REJ%d enviado!\n", trama);
#endif

			continue;
		}

		if (temp_trama != trama)
		{
			sendRR(fd, rej);

#ifdef LOG
			fprintf(llLog, "RR%d re-enviado!\n", trama);
#endif

			continue;
		}

		certo = TRUE;

		if (trama == 0)
			trama = 1;

		else
			trama = 0;

		sendRR(fd, rej);

#ifdef LOG
		fprintf(llLog, "RR%d enviado!\n", trama);
#endif
	}

	return i;
}

int llcloseTransmitter(int fd)
{
	char disc_sender[5];

	disc_sender[0] = FLAG;
	disc_sender[1] = ADDRESS_SENDER;
	disc_sender[2] = DISC_CONTROL;
	disc_sender[3] = ADDRESS_SENDER ^ DISC_CONTROL;
	disc_sender[4] = FLAG;

	char disc_receiver[5];

	int recebido = FALSE;
	int i = 0, res = 0;

	while (conta_alarme <= MAX_ALARMS && !recebido)
	{
		desativa_alarme();

		res = write(fd, disc_sender, SUP_SIZE);

		if (res != SUP_SIZE)
			continue;

#ifdef LOG
		fprintf(llLog, "DISC enviado!\n");
#endif

		alarm(3);

		i = 0;

		while (!flag_alarme && !recebido)
		{
			res = read(fd, disc_receiver + i, 1);

			if (res <= 0)
				continue;

			switch (i)
			{
			case 0:
				if (disc_receiver[i] != FLAG)
					continue;
				break;
			case 1:
				if (disc_receiver[i] != ADDRESS_RECEIVER)
				{
					if (disc_receiver[i] != FLAG)
						i = 0;
					continue;
				}
				break;
			case 2:
				if (disc_receiver[i] != DISC_CONTROL)
				{
					if (disc_receiver[i] != FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 3:
				if (disc_receiver[i] != (ADDRESS_RECEIVER ^ DISC_CONTROL))
				{
					if (disc_receiver[i] != FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 4:
				if (disc_receiver[i] != FLAG)
				{
					i = 0;
					continue;
				}
				break;
			default:
				break;
			}

			i++;

			if (i == SUP_SIZE)
			{
				recebido = TRUE;

#ifdef LOG
				fprintf(llLog, "DISC recebido!\n");
#endif

				desativa_alarme();
			}
		}
	}

	if (!recebido)
		return -1;

	conta_alarme = 0;

	char ua[5];

	ua[0] = FLAG;
	ua[1] = ADDRESS_RECEIVER;
	ua[2] = UA_CONTROL;
	ua[3] = ADDRESS_RECEIVER ^ UA_CONTROL;
	ua[4] = FLAG;

	int enviado = FALSE;

	while (!enviado)
	{
		res = write(fd, ua, SUP_SIZE);

#ifdef LOG
		fprintf(llLog, "UA enviado!\n");
#endif

		if (res == SUP_SIZE)
			enviado = TRUE;
	}

	close(fd);

	return 1;
}

int llcloseReceiver(int fd)
{
	char disc_sender[5];

	char disc_receiver[5];

	disc_receiver[0] = FLAG;
	disc_receiver[1] = ADDRESS_RECEIVER;
	disc_receiver[2] = DISC_CONTROL;
	disc_receiver[3] = ADDRESS_RECEIVER ^ DISC_CONTROL;
	disc_receiver[4] = FLAG;

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
			if (disc_sender[i] != FLAG)
				continue;
			break;
		case 1:
			if (disc_sender[i] != ADDRESS_SENDER)
			{
				if (disc_sender[i] != FLAG)
					i = 0;
				continue;
			}
			break;
		case 2:
			if (disc_sender[i] != DISC_CONTROL)
			{
				if (disc_sender[i] != FLAG)
					i = 0;
				else
					i = 1;
				continue;
			}
			break;
		case 3:
			if (disc_sender[i] != (ADDRESS_SENDER ^ DISC_CONTROL))
			{
				if (disc_sender[i] != FLAG)
					i = 0;
				else
					i = 1;
				continue;
			}
			break;
		case 4:
			if (disc_sender[i] != FLAG)
			{
				i = 0;
				continue;
			}
			break;
		default:
			break;
		}

		i++;

		if (i == SUP_SIZE)
		{
			recebido = TRUE;

#ifdef LOG
			fprintf(llLog, "DISC recebido!\n");
#endif
		}
	}

	int enviado = FALSE;

	while (!enviado)
	{
		res = write(fd, disc_receiver, SUP_SIZE);

#ifdef LOG
		fprintf(llLog, "DISC enviado!\n");
#endif

		if (res == SUP_SIZE)
			enviado = TRUE;
	}

	close(fd);

	return 1;
}

int llclose(int fd, int flag)
{
	if (flag == TRANSMITTER)
	{
		llcloseTransmitter(fd);

#ifdef LOG
		closeLLLogFile();
#endif

		return 0;
	}

	if (flag == RECEIVER)
	{
		llcloseReceiver(fd);

#ifdef LOG
		closeLLLogFile();
#endif

		return 0;
	}

	return -1;
}
