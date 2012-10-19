#ifndef _CONFIG_H
#define _CONFIG_H

#define DEBUGTRACE        0  //是否输出调试信息

#if DEBUGTRACE
#define traceprintf     printf
#else
#define traceprintf
#endif

#define BUFFER_COUNT      10   //接收端缓冲区有该数量才允许播放
#define SAMPLERATE     8000 //定义采样率
#define READMSFORONCE     20 //采样周期(ms)

#define UDP_MODE      0
#define TCP_MODE      1
#define TRAN_MODE     UDP_MODE
#define BUFFERNODECOUNT       128 //链表节点个数

#define RECORD_CAPTURE_PCM         0
#define RECORD_SEND_PCM            0
#define RECORD_RECV_PCM            0
#define RECORD_DECODE_PCM          1
#define RECORD_PLAY_PCM            0

#define MAX_SEND_NO                6
//char buffer[SAMPLERATE/1000*READMSFORONCE*sizeof(short)];
struct AudioBuffer;
typedef struct AudioBuffer
{
    unsigned char buffer_decode[SAMPLERATE/1000*READMSFORONCE*sizeof(short)];//解码缓冲区
    int No;//在环形缓冲区中序号
    char received;//是否接收成功
    char decoded;//是否已经解码成功
	  //int time;//timestampe
	  char SendNO;//
    //char Valid;//包是否有效
    char reserver0;//
    char reserver1;//
    char reserver2;//
    char reserver3;//
    char reserver4;//
    char reserver5;//
    struct AudioBuffer  *pPrior;
    struct AudioBuffer  *pNext;
    int count_recv;//接收数据数量
    int count_decode;//解码后数据数量

    int FrameNO;//包序号
    time_t time;
    int  vad;//1表示有语音，为0表示静音或者噪音
    int  count_encode;//压缩后数据长度
    unsigned char buffer_recv[SAMPLERATE/1000*READMSFORONCE*sizeof(short)];//接收缓冲区
} __attribute__ ((packed)) AUDIOBUFFER;


#define SOUND_OSS                1
#define SOUND_ALSA               2
#define SOUND_INTERFACE          SOUND_OSS

#define CHANNELS                 1


#define VAD_ENABLED              0


#define READFILE_SIMULATE_RCV    0 //读文件 模拟成网络数据接收，用于测试ALSA播放是否正常

#define SILK_AUDIO_CODEC         1
#define SPEEX_AUDIO_CODEC        1
#define SERVER_PORT           9000
#endif
