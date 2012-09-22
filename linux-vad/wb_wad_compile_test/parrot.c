/*
 * parrot.c
 * Program to illustrate /dev/dsp device
 * Records several seconds of sound, then echoes it back.
 * Runs until Control-C is pressed.
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/soundcard.h>

#define LENGTH 3    /* how many seconds of speech to store */
#define RATE 8000   /* the sampling rate */
#define SIZE 8      /* sample size: 8 or 16 bits */
#define CHANNELS 1  /* 1 = mono 2 = stereo */

/* this buffer holds the digitized audio */
unsigned char buf[LENGTH*RATE*SIZE*CHANNELS/8];

int main()
{
  int fd;	/* sound device file descriptor */
  int arg;	/* argument for ioctl calls */
  int status;   /* return status of system calls */

  /* open sound device */
  fd = open("/dev/dsp", O_RDWR);
  if (fd < 0) {
    perror("open of /dev/dsp failed");
    exit(1);
  }

  /* set sampling parameters */
  arg = SIZE;	   /* sample size */
  status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
  if (status == -1)
    perror("SOUND_PCM_WRITE_BITS ioctl failed");
  if (arg != SIZE)
    perror("unable to set sample size");

  arg = CHANNELS;  /* mono or stereo */
  status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
  if (status == -1)
    perror("SOUND_PCM_WRITE_CHANNELS ioctl failed");
  if (arg != CHANNELS)
    perror("unable to set number of channels");

  arg = RATE;	   /* sampling rate */
  status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
  if (status == -1)
    perror("SOUND_PCM_WRITE_WRITE ioctl failed");

  while (1) { /* loop until Control-C */
    printf("Say something:\n");
    status = read(fd, buf, sizeof(buf)); /* record some sound */
    if (status != sizeof(buf))
      perror("read wrong number of bytes");
    printf("You said:\n");
    status = write(fd, buf, sizeof(buf)); /* play it back */
    if (status != sizeof(buf))
      perror("wrote wrong number of bytes");
    /* wait for playback to complete before recording again */
    status = ioctl(fd, SOUND_PCM_SYNC, 0); 
  if (status == -1)
    perror("SOUND_PCM_SYNC ioctl failed");
  }
}

