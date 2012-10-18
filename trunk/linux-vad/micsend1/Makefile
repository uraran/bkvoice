CROSS_COMPILE ?=
GCC=$(CROSS_COMPILE)gcc

micsend:mic.o
	$(GCC) -o micsend mic.o -lpthread -I$(PWD)/../../libavcodec/install_sdk/arm920t/include/ \
    -L$(PWD)/../../libavcodec/install_sdk/arm920t/lib/ -lSKP_SILK_SDK -lm  -lspeex -lspeexdsp -static -lm

	cp micsend /tftpboot
	
mic.o:mic.c config.h
	$(GCC) -c -o mic.o mic.c -I$(PWD)/../../libavcodec/install_sdk/arm920t/include/ \
    -L$(PWD)/../../libavcodec/install_sdk/arm920t/lib/

clean:
	rm -rf *.o micsend /tftpboot/micsend
