CROSS_COMPILE ?=
GCC=$(CROSS_COMPILE)gcc

app:speaker.o
	$(GCC) -o speaker speaker.o -lpthread
	cp speaker /tftpboot
	
testplay:testplay.o
	$(GCC) -o testplay testplay.o -lpthread
	cp testplay /tftpboot

testplay.o:testplay.c config.h
	$(GCC) -c -o testplay.o testplay.c

speaker.o:speaker.c config.h
	$(GCC) -c -o speaker.o speaker.c

clean:
	rm -rf *.o speaker /tftpboot/speaker testplay /tftpboot/testplay
