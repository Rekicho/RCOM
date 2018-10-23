#include "constants.h"

int trama = 0;
int flag_alarme = 0;
int conta_alarme = 0;

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

		if (res != SET_SIZE)
			continue;
		
		printf("SET enviado!\n");

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
				if (ua[i] != UA_FLAG)
					continue;
				break;
			case 1:
				if (ua[i] != UA_ADDRESS_SENDER)
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
				if (ua[i] != (UA_ADDRESS_SENDER ^ UA_CONTROL))
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

	conta_alarme = 0;
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

	(void)signal(SIGALRM, atende_alarme);

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

		printf("Trama I%d enviada!\n", trama);

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
				if (rr[i] != RR_FLAG)
					continue;
				break;
			case 1:
				if (rr[i] != RR_ADDRESS)
				{
					if (rr[i] != RR_FLAG)
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
					if (rr[i] != RR_FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 3:
				if ((rej && ((rr[i] == (char)(RR_ADDRESS ^ REJ_CONTROL0) && temp_trama == 0) || (rr[i] == (char)(RR_ADDRESS ^ REJ_CONTROL1) && temp_trama == 1))) || (!rej && ((rr[i] == (char)(RR_ADDRESS ^ RR_CONTROL0) && temp_trama == 0) || (rr[i] == (char)(RR_ADDRESS ^ RR_CONTROL1) && temp_trama == 1))))
				break;
				else
				{
					if (rr[i] != RR_FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
			case 4:
				if (rr[i] != RR_FLAG)
				{
					i = 0;
					continue;
				}
				break;
			default:
				break;
			}

			i++;

			if (i == RR_SIZE)
			{
				desativa_alarme();
				recebido = TRUE;
				printf("RR%d recebido!\n", temp_trama);
			}
		}

		if(!recebido)
			continue;

		conta_alarme = 0;

		if(rej || trama == temp_trama)
		{
			temp_trama = -1;
			recebido = FALSE;
			rej = FALSE;
			printf("Re-sending trama I%d!\n",trama);
			continue;
		}

		if (trama == 0)
			trama = 1;

		else trama = 0;
	}

	free(buf);

	return j;
}

int llclose(int fd)
{
	char disc_sender[5];

	disc_sender[0] = DISC_FLAG;
	disc_sender[1] = DISC_ADDRESS_SENDER;
	disc_sender[2] = DISC_CONTROL;
	disc_sender[3] = DISC_ADDRESS_SENDER ^ DISC_CONTROL;
	disc_sender[4] = DISC_FLAG;

	char disc_receiver[5];

	(void)signal(SIGALRM, atende_alarme);

	int recebido = FALSE;
	int i = 0, res = 0;

	while (conta_alarme <= MAX_ALARMS && !recebido)
	{
		desativa_alarme();

		res = write(fd, disc_sender, DISC_SIZE);

		if (res != DISC_SIZE)
			continue;
		
		printf("DISC enviado!\n");

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
				if (disc_receiver[i] != DISC_FLAG)
					continue;
				break;
			case 1:
				if (disc_receiver[i] != DISC_ADDRESS_RECEIVER)
				{
					if (disc_receiver[i] != DISC_FLAG)
						i = 0;
					continue;
				}
				break;
			case 2:
				if (disc_receiver[i] != DISC_CONTROL)
				{
					if (disc_receiver[i] != DISC_FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 3:
				if (disc_receiver[i] != (DISC_ADDRESS_RECEIVER ^ DISC_CONTROL))
				{
					if (disc_receiver[i] != DISC_FLAG)
						i = 0;
					else
						i = 1;
					continue;
				}
				break;
			case 4:
				if (disc_receiver[i] != DISC_FLAG)
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
				desativa_alarme();
			}
		}
	}

	conta_alarme = 0;

	char ua[5];

    ua[0] = UA_FLAG;
    ua[1] = UA_ADDRESS_RECEIVER;
    ua[2] = UA_CONTROL;
    ua[3] = UA_ADDRESS_RECEIVER ^ UA_CONTROL;
    ua[4] = UA_FLAG;

	int enviado = FALSE;

    while (!enviado)
    {
        res = write(fd, ua, UA_SIZE);
        printf("UA enviado!\n");

        if (res == UA_SIZE)
            enviado = TRUE;
    }

	return 1;
}
