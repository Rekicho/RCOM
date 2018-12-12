#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> 
#include <netdb.h> 
#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <fcntl.h>

#define SERVER_PORT 21

#define CONNECTION_OK "220"
#define SEND_PASS "331"
#define LOGGED_IN "230"
#define START_TRANSFER "150"
#define TRANSFER_COMPLETE "226"
#define GOODBYE "221"

#define READ_START 0
#define READ_LINE 1
#define READ_LINES 2
#define READ_END 3 
#define READ_IGNORE 4
#define READ_PORT 5

ssize_t getpass (char **lineptr, size_t *n, FILE *stream)
{
	struct termios old, new;
	int nread;

	/* Turn echoing off and fail if we can’t. */
	if (tcgetattr (fileno (stream), &old) != 0)
		return -1;
	new = old;
	new.c_lflag &= ~ECHO;
	if (tcsetattr (fileno (stream), TCSAFLUSH, &new) != 0)
		return -1;

	/* Read the passphrase */
	nread = getline (lineptr, n, stream);

	/* Restore terminal. */
	(void) tcsetattr (fileno (stream), TCSAFLUSH, &old);

	return nread;
}

void errorUsage()
{
	printf("Usage: download ftp://[<user>:<password>@]<host>/<url-path>\n");
	exit(1);
}

void parseArguments(char *arg, char *user, char *password, char *host, char *url_path, char *file_name)
{
	host[0] = '\0';
	url_path[0] = '\0';

	if(strncmp(arg, "ftp://", 6) != 0)
		errorUsage();

	char *start = arg + (6 * sizeof(char));

	char *div = strchr(start,'@');

	if(div == NULL)
	{
		user[0] = '\0';
		password[0] = '\0';

		div = strchr(start,'/');

		if(div == NULL)
			errorUsage();

		memcpy(host, start, (div - start) / sizeof(char));

		div++;
		memcpy(url_path, div, strlen(div)); 
	}

	else
	{
		div = strchr(start,':');

		if(div == NULL)
			errorUsage();

		memcpy(user, start, (div - start) / sizeof(char));
		div++;
		start = div;

		div = strchr(start,'@');

		if(div == NULL)
			errorUsage();

		memcpy(password, start, (div - start) / sizeof(char));
		div++;
		start = div;

		div = strchr(start,'/');
		
		if(div == NULL)
			errorUsage();

		memcpy(host, start, (div - start) / sizeof(char));
		div++;

		memcpy(url_path, div, strlen(div)); 

		char *lastdiv;

		while(div != NULL)
		{
			lastdiv = div;
			div++;
			div = strchr(div,'/');	
		}

		memcpy(file_name, lastdiv, strlen(lastdiv)); 
	}  
}

char *getIPaddress(char *host)
{
	struct hostent *h;

	if ((h=gethostbyname(host)) == NULL) 
	{  
		herror("gethostbyname");
		exit(1);
	}

	return inet_ntoa(*((struct in_addr *)h->h_addr));	
}

int createSocket(char *ip, int port)
{
	int	sockfd;
	struct	sockaddr_in server_addr;

	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(port);	/*server TCP port must be network byte ordered */

	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) 
	{
		perror("socket()");
		exit(0);
	}
	/*connect to the server*/
	if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("connect()");
		exit(0);
	}

	return sockfd;
}

void getServerReply(int socket, char *server_reply)
{
	char c;
	int i = 0;
	int state = READ_START;

	while (state != READ_END)
	{
		if (read(socket, &c, 1) != 1)
			continue;

		printf("%c", c);

		switch(state)
		{
			case READ_START:
				switch(c)
				{
					case ' ': 
						state = READ_LINE;
						i = 0;
						break;
					case '-':
						state = READ_LINES;
						i = 0;
						break;
					default: 
						if(c >= '0' && c <= '9')
						{
							server_reply[i] = c;
							i++;
						}
						break;
				}
				break;
			case READ_LINE:
				if (c == '\n')
					state = READ_END;
				break;
			case READ_LINES:
				if (c == server_reply[i])
					i++;
				else if (i == 3)
				{
					if (c == ' ')
						state = READ_LINE;

					else if(c == '-')
						i = 0;
				}
				break;
			default: break;
		}
	}
}

int getPort(int socket)
{
	char c;
	int port = 0;
	char port_temp[4];
	port_temp[3] = '\0';
	int i = 0;
	int separators = 0;
	int state = READ_START;

	while (state != READ_END)
	{
		if (read(socket, &c, 1) != 1)
			continue;

		printf("%c", c);

		switch(state)
		{
			case READ_START:
				if (c == ' ')
				{
					if (i != 3)
						exit(1);

					i = 0;
					state = READ_IGNORE;
				}
				else i++;
				break;
			case READ_IGNORE:
				if (c == ',')
					separators++;
				if (separators == 4)
					state = READ_PORT;
				break;
			case READ_PORT:
				if (c == ',')
				{
					port += atoi(port_temp) * 256;
					port_temp[0] = '\0';
					port_temp[1] = '\0';
					port_temp[2] = '\0';
					i = 0;
				}
				else if (c == ')')
				{
					port += atoi(port_temp);
					state = READ_END;
				}
				else
				{
					port_temp[i] = c;
					i++;
				}
				break;
			default: break;
		}
	}

	//Read .
	read(socket, &c, 1);
	printf("%c", c);
	printf("\n");

	return port;	
}

void sendCommand(int socket, char *cmd, char *arg, char *response, int *port)
{
	write(socket, cmd, strlen(cmd));
	if(arg != NULL)
	{
		write(socket, " ", 1);
		write(socket, arg, strlen(arg));
	}
	write(socket, "\n", 1);

    if(strcmp("pasv", cmd) == 0)
		*port = getPort(socket);
	
	else getServerReply(socket, response);
}

void downloadFile(int port_file, char *file_name)
{
    int file = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    char data[256];
    int res;

    while ((res = read(port_file, data, 256)) > 0)
        write(file, data, res);

    close(file);
}

int main(int argc, char **argv)
{
	if (argc != 2)
		errorUsage();

	char *user = (char*) malloc(100);
	char *password = (char*) malloc(100);
	char *host = (char*) malloc(100);
	char *url_path = (char*) malloc(100);
	char *file_name = (char*) malloc(100);

	parseArguments(argv[1], user, password, host, url_path, file_name);

	char *ip = getIPaddress(host);
	int socket = createSocket(ip, SERVER_PORT);
	
	char server_reply[4];
	server_reply[3] = '\0';

	getServerReply(socket, server_reply);

	if (strcmp(CONNECTION_OK,server_reply) == 0)
		printf("Connected to %s\n", host);

	else
	{
		printf("Could not connect to %s\n", host);
		return 1;
	}

	if(user[0] == '\0')
	{
		printf("User: ");
		scanf("%s", user);
		getchar();
	}

	sendCommand(socket,"user",user,server_reply,NULL);

	if(strcmp(SEND_PASS,server_reply) != 0)
		return 1;

	if(password[0] == '\0')
	{
		printf("Password: ");
		size_t size = 100;
		getpass (&password, &size, stdin);
		password[strlen(password)-1] = '\0';
		printf("\n");
	}

	sendCommand(socket,"pass",password,server_reply,NULL);

	if(strcmp(LOGGED_IN, server_reply) != 0)
		return 1;

	int port;
	sendCommand(socket,"pasv",NULL,NULL,&port);

	int port_file = createSocket(ip, port);

	sendCommand(socket,"retr",url_path,server_reply,NULL);

	if(strcmp(START_TRANSFER, server_reply) != 0)
		return 1;

	downloadFile(port_file, file_name);

	getServerReply(socket,server_reply);

	if(strcmp(TRANSFER_COMPLETE, server_reply) != 0)
		return 1;

	sendCommand(socket,"quit",NULL,server_reply,NULL);

	if(strcmp(GOODBYE, server_reply) != 0)
		return 1;

	close(port_file);
	close(socket);
	free(user);
	free(password);
	free(host);
	free(url_path);
	free(file_name);

	return 0;
}
