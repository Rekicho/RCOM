application: application.c ll.c
				gcc application.c ll.c -o application -Wall -lm -D LOG -D PROGRESS

efi_size: application.c ll.c
			gcc application.c ll.c -o application -Wall -lm -D EFI_SIZE -D TIME

efi_baudrate: application.c ll.c
			gcc application.c ll.c -o application -Wall -lm -D EFI_BAUDRATE -D TIME

efi_delay: application.c ll.c
			gcc application.c ll.c -o application -Wall -lm -D EFI_DELAY -D TIME

efi_error: application.c ll.c
			gcc application.c ll.c -o application -Wall -lm -D EFI_ERROR -D TIME

clean:
		rm -f application *Log.txt
