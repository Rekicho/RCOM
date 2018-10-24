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

int setup(char* port)
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

void llopenTransmitter(int fd)
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

void llopenReceiver(int fd)
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

int llopen(char *port, int flag)
{
	int fd = setup(port);

	if (flag == TRANSMITTER)
		llopenTransmitter(fd);

	else if (flag == RECEIVER)
		llopenReceiver(fd);

	return fd;
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

int llcloseTransmitter(int fd)
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

int llcloseReceiver(int fd)
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

int llclose(int fd, int flag)
{
	if(flag == TRANSMITTER)
		return llcloseTransmitter(fd);

	if(flag == RECEIVER)
		return llcloseReceiver(fd);

	return -1;
}
