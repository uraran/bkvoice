CROSS_COMPILE ?=
GCC=$(CROSS_COMPILE)gcc

all:speaker testplay_oss testplay_alsa

speaker:speaker.o wb_vad.o 
	$(GCC) -o speaker speaker.o amr_wb_vad/wb_vad.o -lpthread -lasound -I./amr_wb_vad \
    -L$(PWD)/../../libavcodec/install_sdk/x86/lib/ -lSKP_SILK_SDK  -g
	cp speaker /tftpboot




testplay_alsa:testplay_alsa.o
	$(GCC) -o testplay_alsa testplay_alsa.o -lpthread -lasound
	cp testplay_alsa /tftpboot

testplay_alsa.o:testplay_alsa.c config.h
	$(GCC) -c -o testplay_alsa.o testplay_alsa.c



	
testplay_oss:testplay_oss.o
	$(GCC) -o testplay_oss testplay_oss.o -lpthread
	cp testplay_oss /tftpboot

testplay_oss.o:testplay_oss.c config.h
	$(GCC) -c -o testplay_oss.o testplay_oss.c

speaker.o:speaker.c config.h
	$(GCC) -c -o speaker.o speaker.c -I./amr_wb_vad -I$(PWD)/../../libavcodec/install_sdk/x86/include/ \
    -L$(PWD)/../../libavcodec/install_sdk/x86/lib/ -lSKP_SILK_SDK 

wb_vad.o:amr_wb_vad/wb_vad.c
	$(GCC) -c -o amr_wb_vad/wb_vad.o amr_wb_vad/wb_vad.c

clean:
	rm -rf *.o speaker /tftpboot/speaker testplay_oss /tftpboot/testplay_oss testplay_alsa /tftpboot/testplay_alsa
