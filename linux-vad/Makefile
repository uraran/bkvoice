#make CROSS_COMPILE=arm-linux

CROSS_COMPILE=
CXX = $(CROSS_COMPILE)-g++
CC = $(CROSS_COMPILE)-gcc
AS = $(CROSS_COMPILE)-as
AR = $(CROSS_COMPILE)-ar
NM = $(CROSS_COMPILE)-nm
LD = $(CROSS_COMPILE)-ld
OBJDUMP = $(CROSS_COMPILE)-objdump
OBJCOPY = $(CROSS_COMPILE)-objcopy
RANLIB = $(CROSS_COMPILE)-ranlib
STRIP = $(CROSS_COMPILE)-strip

CFLAGS       = -c -Wall -D_REENTRANT -DACE_HAS_AIO_CALLS -D_GNU_SOURCE -DACE_HAS_EXCEPTIONS -D__ACE_INLINE__  
LDFLAGS      = -L. -L/etc/ACE_wrappers/lib 
CPPFLAGS     += -I/etc/ACE_wrappers -I/etc/ACE_wrappers/ace 
LDLIBS       = -lACE

vad:main.o
	$(CC) -o vad main.o
	cp vad /tftpboot
	
main.o:
	$(CC) -o main.o -c main.c
	
clean:
	@rm -f vad *.o
