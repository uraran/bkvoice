CROSS_COMPILE ?=
GCC=$(CROSS_COMPILE)gcc

app:speaker.o wb_vad.o 
	$(GCC) -o speaker speaker.o amr_wb_vad/wb_vad.o -lpthread -lasound -I./amr_wb_vad
	cp speaker /tftpboot
	
testplay:testplay.o
	$(GCC) -o testplay testplay.o -lpthread
	cp testplay /tftpboot

testplay.o:testplay.c config.h
	$(GCC) -c -o testplay.o testplay.c

speaker.o:speaker.c config.h
	$(GCC) -c -o speaker.o speaker.c -I./amr_wb_vad

wb_vad.o:amr_wb_vad/wb_vad.c
	$(GCC) -c -o amr_wb_vad/wb_vad.o amr_wb_vad/wb_vad.c

clean:
	rm -rf *.o speaker /tftpboot/speaker testplay /tftpboot/testplay
