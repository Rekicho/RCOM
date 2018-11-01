application: application.c ll.c
				gcc application.c ll.c -o application -Wall -lm

efi_size: application.c ll.c
			gcc application.c ll.c -o application -Wall -lm -D EFI_SIZE

clean:
		rm -f application *Log.txt
