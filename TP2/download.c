#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> 
#include <netdb.h> 
#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/inet.h>

#define SERVER_PORT 21

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

void parseArguments(char *arg, char *user, char *password, char *host, char *url_path)
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
	server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);	/*server TCP port must be network byte ordered */

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

int main(int argc, char **argv)
{
	if (argc != 2)
		errorUsage();

	char *user = (char*) malloc(100);
	char *password = (char*) malloc(100);
	char *host = (char*) malloc(100);
	char *url_path = (char*) malloc(100);

	parseArguments(argv[1], user, password, host, url_path);

	if(password[0] == '\0')
	{
		printf("User: ");
		scanf("%s", user);
	
		getchar();

		printf("Password: ");
		size_t size = 100;
		getpass (&password, &size, stdin);
		password[strlen(password)-1] = '\0';
		printf("\n");
	}

	char *ip = getIPaddress(host);
	int socket = createSocket(ip, SERVER_PORT);

	printf("%d",socket);

	close(socket);
	free(user);
	free(password);
	free(host);
	free(url_path);

	return 0;
}
