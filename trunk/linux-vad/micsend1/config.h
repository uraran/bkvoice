#ifndef _CONFIG_H
#define _CONFIG_H

#define DEBUGTRACE        0  //是否输出调试信息

#if DEBUGTRACE
#define traceprintf     printf
#else
#define traceprintf
#endif

#define BUFFER_COUNT      3   //接收端缓冲区有该数量才允许播放
#define SAMPLERATE     16000 //定义采样率
#define READMSFORONCE    16 //采样周期(ms)

#define UDP_MODE      0
#define TCP_MODE      1
#define TRAN_MODE     UDP_MODE
#define BUFFERNODECOUNT       1024 //链表节点个数

#define RECORD_CAPTURE_PCM         1
#define RECORD_SEND_PCM            1
#define RECORD_RECV_PCM            0
#define RECORD_PLAY_PCM            0

#define MAX_SEND_NO                6
//char buffer[SAMPLERATE/1000*READMSFORONCE*sizeof(short)];
struct AudioBuffer;
typedef struct AudioBuffer
{
    unsigned char buffer[SAMPLERATE/1000*READMSFORONCE*sizeof(short)];
    int FrameNO;//包序号
	  int time;//timestampe
	  char SendNO;//
    char Valid;//包是否有效
    char reserver0;//
    char reserver1;//
    char reserver2;//
    char reserver3;//
    char reserver4;//
    char reserver5;//
    struct AudioBuffer  *pNext;
    int count;//实际有效字节数量
} __attribute__ ((packed)) AUDIOBUFFER;


#define SOUND_OSS                1
#define SOUND_ALSA               2
#define SOUND_INTERFACE          SOUND_OSS

#endif
