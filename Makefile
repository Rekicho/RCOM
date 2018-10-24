application: application.c ll.c
				gcc application.c ll.c -o application -Wall -lm

clean:
		rm -f application
