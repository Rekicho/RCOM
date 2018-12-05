#include <termios.h>
#include <stdio.h>
#include <stdlib.h>

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

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: download ftp://[<user>:<password>@]<host>/<url-path>\n");
		return -1;
	}

	size_t size = 100;
	char *password = (char*) malloc(100);

	getpass (&password, &size, stdin);

	printf("%d : %s", size, password);

	return 0;
}
