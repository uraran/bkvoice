CROSS_COMPILE ?=
GCC=$(CROSS_COMPILE)gcc

micsend:mic.o
	$(GCC) -o micsend mic.o -lpthread 
	cp micsend /tftpboot
	
mic.o:mic.c config.h
	$(GCC) -c -o mic.o mic.c

clean:
	rm -rf *.o micsend /tftpboot/micsend
