#ifndef __CONFIG_H__
#define __CONFIG_H__



#define  AUDIO_CAP_DSOUND      0 
#define  AUDIO_CAP_WINMM       1

#define SPEEX_ENABLED     0
#define G711_ENABLED      0

#define SIZE_AUDIO_FRAME 3840

#define HZ_POOR		8000
#define HZ_LOW		11025
#define HZ_NORMAL	22050
#define HZ_HIGH		44100

//#define SAMPLEPSEC   44100
#define MAXRECBUFFER	8

#if SPEEX_ENABLED
#define BUFCOUNT          6400
#else
#define BUFCOUNT          5012
#endif

#define  SAMPLINGPERIOD   16 //采样周期,单位ms

extern DWORD dwSample;
extern WORD  wChannels;

extern int capture_size; //一次采样字节数
#endif
