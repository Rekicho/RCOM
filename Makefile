application: application.c ll.c
				gcc application.c ll.c -o application -Wall -lm -D LOG -D PROGRESS

efi_size: application.c ll.c
			gcc application.c ll.c -o application -Wall -lm -D EFI_SIZE -D TIME

clean:
		rm -f application *Log.txt
