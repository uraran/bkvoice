#include <stdio.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <semaphore.h>
#include <sys/time.h>
#include "config.h"

#if (SOUND_INTERFACE == SOUND_OSS)
#include <sys/soundcard.h>
#elif (SOUND_INTERFACE == SOUND_ALSA)
#include <alsa/asoundlib.h>
#endif

AUDIOBUFFER audiobuffer[BUFFERNODECOUNT];
AUDIOBUFFER *pWriteHeader = NULL;
AUDIOBUFFER *pReadHeader = NULL;
int n = 0;//可用包数

#if (SOUND_INTERFACE == SOUND_OSS)
int Frequency = SAMPLERATE;
int format = AFMT_S16_LE;
#endif
int channels = CHANNELS;
int setting = 64;//0x00040009;

int flag_capture_audio = 0;
int flag_play_audio    = 0;
int flag_network_send  = 0;
int flag_network_recv  = 0;
int programrun = 1;     //退出程序
#if (SOUND_INTERFACE == SOUND_OSS)
#elif (SOUND_INTERFACE == SOUND_ALSA)
/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
long loops;
int rc;
int size;
snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
unsigned int val;
int dir;
snd_pcm_uframes_t frames;
#if 0
char *buffer;
#endif
#endif

pthread_t thread_capture_audio, thread_play_audio;
pthread_t thread_network_send,  thread_network_recv;
pthread_mutex_t mutex_lock;

sem_t sem_recv;    //接收端用的信号量
sem_t sem_capture; //发送端用的信号量

char serverip[15];
int  serverport;

int FrameNO = 0; //包序号

int fdsocket;
struct sockaddr_in dest_addr;
struct sockaddr_in local_addr;

#if (SOUND_INTERFACE == SOUND_ALSA)
int xrun_recovery(snd_pcm_t *handle, int err)
{
   if (err == -EPIPE) {    /* under-run */
      err = snd_pcm_prepare(handle);
   if (err < 0)
      printf("Can't recovery from underrun, prepare failed: %s\n",
         snd_strerror(err));
      return 0;
   } else if (err == -ESTRPIPE) {
      while ((err = snd_pcm_resume(handle)) == -EAGAIN)
         sleep(1);       /* wait until the suspend flag is released */
         if (err < 0) {
            err = snd_pcm_prepare(handle);
         if (err < 0)
            printf("Can't recovery from suspend, prepare failed: %s\n",
              snd_strerror(err));
      }
      return 0;
   }
   return err;
}
#endif

//音频采集线程
void * capture_audio_thread(void *para)
{
    int fdsound = 0;
    int readbyte = 0;
#if RECORD_CAPTURE_PCM
    FILE *fp = fopen("capture.pcm", "wb");
#endif


#if (SOUND_INTERFACE == SOUND_OSS)
    fdsound = open("/dev/dsp", O_RDONLY);
    if(fdsound<0)
    {
       perror("以只写方式打开音频设备");
       return;
    }    

    printf("设置读音频设备参数 setup capture audio device parament\n");
    ioctl(fdsound, SNDCTL_DSP_SPEED, &Frequency);//采样频率
    ioctl(fdsound, SNDCTL_DSP_SETFMT, &format);//音频设备位宽
    ioctl(fdsound, SNDCTL_DSP_CHANNELS, &channels);//音频设备通道
    ioctl(fdsound, SNDCTL_DSP_SETFRAGMENT, &setting);//采样缓冲区

    while(flag_capture_audio)
    {
        if((readbyte=read(fdsound, pWriteHeader->buffer, SAMPLERATE/1000*READMSFORONCE*sizeof(short))) < 0)
        {
            perror("读声卡数据");
        }
        else
        {
            sem_post(&sem_capture);
            //printf("readbyte=%d\n", readbyte);
#if RECORD_CAPTURE_PCM
            fwrite(pWriteHeader->buffer, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
            traceprintf("发送信号量 sem_capture\n");
            
            pWriteHeader->FrameNO = FrameNO++;
            printf("cNO:%d, readbyte=%d\n", pWriteHeader->FrameNO, readbyte);
            pthread_mutex_lock(&mutex_lock);
            n++;
            pWriteHeader->Valid = 1;
            pthread_mutex_unlock(&mutex_lock);
            pWriteHeader = pWriteHeader->pNext;
        }
    }
    close(fdsound);
#elif (SOUND_INTERFACE == SOUND_ALSA)
    struct timeval tv;
    struct timezone tz;
    /* Open PCM device for recording (capture). */
    rc = snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) 
    {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }
    snd_pcm_hw_params_alloca(&params);/* Allocate a hardware parameters object. */    
    snd_pcm_hw_params_any(handle, params);/* Fill it in with default values. */

    /* Set the desired hardware parameters. */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);/* Interleaved mode */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);/* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);/* Two channels (stereo), On for mono */
    val = SAMPLERATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);/* SAMPLERATE bits/second sampling rate  */
    frames = SAMPLERATE/1000*READMSFORONCE;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);/* Set period size to SAMPLES_FOR_EACH_TIME frames. */

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) 
    {
        fprintf(stderr, "unable to set hw parameters: %s\n",snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);/* Use a buffer large enough to hold one period */
#if 0    
    size = frames * sizeof(short) * CHANNELS; /* 2 bytes/sample, 2 channels */
    buffer = (char *) malloc(size);
#endif
    snd_pcm_hw_params_get_period_time(params, &val, &dir);/* We want to loop for 5 seconds */
                                         
    while(flag_capture_audio)
    {
        rc = snd_pcm_readi(handle, pWriteHeader->buffer, frames);
        if (rc == -EPIPE) 
        {
            /* EPIPE means overrun */
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(handle);
        } 
        else if (rc < 0) 
        {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        } 
        else if (rc != (int)frames) 
        {
            fprintf(stderr, "short read, read %d frames\n", rc);
        }

#if RECORD_CAPTURE_PCM
            fwrite(pWriteHeader->buffer, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
#if 0
#if DEBUG_SAVE_CAPTURE_PCM
        rc = fwrite(pWriteHeader->buffer, frames*sizeof(short), 1, fp);
        if(rc != 1)
        {
            fprintf(stderr, "write error %d\n", rc);
        }
#endif 
#endif
        sem_post(&sem_capture);                      
        gettimeofday(&tv, &tz);
        ((AUDIOBUFFER*)(pWriteHeader->buffer))->FrameNO = FrameNO++;
        //((AUDIOBUFFER*)(pWriteHeader->buffer))->sec = tv.tv_sec;
        //((AUDIOBUFFER*)(pWriteHeader->buffer))->usec = tv.tv_usec;
        //printf("capture NO=%5d \n", FrameNO);
        pthread_mutex_lock(&mutex_lock);     
        //pWriteHeader = pWriteHeader->pNext;
        pWriteHeader->Valid = 1;
        n++;                
        pthread_mutex_unlock(&mutex_lock);  
        pWriteHeader = pWriteHeader->pNext;
        //traceprintf("发送信号量 sem_capture\n");
    }
                                         
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
#if 0    
    free(buffer);
#endif  
#endif

#if RECORD_CAPTURE_PCM
    fclose(fp);
#endif
    printf("音频采集线程已经关闭 audio capture thread is closed\n");
    return NULL;
}

//清除采集线程
void remove_capture_audio(void)
{
    flag_capture_audio = 0;
    usleep(40*1000);
    pthread_cancel(thread_capture_audio);
}

void * network_send_thread(void *p)
{
#if RECORD_SEND_PCM
    FILE *fp = fopen("send.pcm", "wb");
#endif
    socklen_t socklen;
    printf("数据发送开始\n");
    socklen = sizeof(struct sockaddr);
    while(flag_network_send)
    {
        sem_wait(&sem_capture);//等待采集线程有数据
        if(n > 0)
        {      
                traceprintf("等待 sem_capture 信号量\n");

                if(!pReadHeader->Valid) 
                {
                    printf("忽略%d\n", pReadHeader->FrameNO);
                    continue;
                }
#if TRAN_MODE==UDP_MODE
                if(-1 == sendto(fdsocket, pReadHeader->buffer, sizeof(pReadHeader->buffer)+sizeof(int)+sizeof(int), 0, (struct sockaddr*)&dest_addr, socklen))
#elif TRAN_MODE==TCP_MODE
                if(-1 == send(fdsocket, pReadHeader->buffer, sizeof(pReadHeader->buffer), 0))
#endif
                {
                        perror("sendto");
                }
                else
                {
#if RECORD_SEND_PCM
                        fwrite(pReadHeader->buffer, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
                        traceprintf("n=%d\n", n);
                        printf("    sNO:%d\n", pReadHeader->FrameNO);
                        pthread_mutex_lock(&mutex_lock);
                        pReadHeader->Valid = 0;
                        n--;
                        pthread_mutex_unlock(&mutex_lock);
                        pReadHeader = pReadHeader->pNext;
                }
        }
    }
    
    printf("数据发送线程已经关闭 data send thread is closed\n");
#if TRAN_MODE==UDP_MODE
    close(fdsocket);
#endif

#if RECORD_SEND_PCM
    fclose(fp);
#endif
}

//清除发送线程
void remove_network_send()
{
    flag_network_send = 0;
    usleep(40*1000);
    pthread_cancel(thread_network_send);  
}

void * network_recv_thread(void *p)
{
    struct sockaddr_in remote_addr;
    int result;
    int socklen;
#if RECORD_RECV_PCM 
    FILE * fp = fopen("recv.pcm", "wb");
    if(!fp)
    {
        perror("open file");
    }
#endif
    
    printf("数据接收开始\n");
    socklen = sizeof(struct sockaddr);
    while(flag_network_recv)
    {
        printf("recv\n");
        {      
            result = recvfrom(fdsocket, pWriteHeader->buffer, sizeof(pWriteHeader->buffer)+sizeof(int)+sizeof(int), 
                0, (struct sockaddr*)&remote_addr, &socklen);

            if(result == -1)
            {
                    perror("data recv");
                    return;
            }
            else
            {
#if RECORD_RECV_PCM 
                fwrite(pWriteHeader->buffer, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
                traceprintf("收到数据 %d byte\n", result);
            }
        }
    }
#if RECORD_RECV_PCM 
    fclose(fp);
#endif    
    printf("数据接收线程已经关闭 data recv thread is closed\n");
    close(fdsocket);    
}

//清除接收线程
void remove_network_recv()
{

}

#define RUNMODE_SERVER    0  //接收端
#define RUNMODE_CLIENT    1  //发送端

int InitSocketBuffer()
{
    /* 
     * 先读取缓冲区设置的情况 
     * 获得原始发送缓冲区大小 
     */ 
    int err = -1;        /* 返回值 */ 
    int snd_size = 0;   /* 发送缓冲区大小 */ 
    int rcv_size = 0;    /* 接收缓冲区大小 */ 
    socklen_t optlen;    /* 选项值长度 */ 
    optlen = sizeof(snd_size); 
    err = getsockopt(fdsocket, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen); 

    if(err<0){ 
        printf("获取发送缓冲区大小错误\n"); 
    }   
    else
    {
        printf("send buffer size=%d\n", snd_size);
    } 


    err = getsockopt(fdsocket, SOL_SOCKET, SO_RCVBUF,&snd_size, &optlen); 

    if(err<0){ 
        printf("获取发送缓冲区大小错误\n"); 
    }   
    else
    {
        printf("recv buffer size=%d\n", snd_size);
    } 


    //-------------------------------------------------------------------
    //-------------------------------------------------------------------
    /* 
     * 设置接收缓冲区大小 
     */ 
    rcv_size = 1*1024*1024;    /* 接收缓冲区大小为8K */ 
    optlen = sizeof(rcv_size); 
    err = setsockopt(fdsocket,SOL_SOCKET,SO_RCVBUF, (char *)&rcv_size, optlen); 
    if(err<0){ 
        printf("设置接收缓冲区大小错误\n"); 
    } 
 
    /* 
     * 检查上述缓冲区设置的情况 
     * 获得修改后发送缓冲区大小 
     */ 
    snd_size = 1*1024*1024;    /* 接收缓冲区大小为8K */ 
    optlen = sizeof(snd_size); 
    err = setsockopt(fdsocket, SOL_SOCKET, SO_SNDBUF, (char *)&snd_size, optlen); 
    if(err<0){ 
        printf("获取发送缓冲区大小错误\n"); 
    }   
 
    //-------------------------------------------------------------------
    //-------------------------------------------------------------------

    err = getsockopt(fdsocket, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen); 

    if(err<0){ 
        printf("获取发送缓冲区大小错误\n"); 
    }   
    else
    {
        printf("send buffer size=%d\n", snd_size);
    } 


    err = getsockopt(fdsocket, SOL_SOCKET, SO_RCVBUF,&snd_size, &optlen); 

    if(err<0){ 
        printf("获取发送缓冲区大小错误\n"); 
    }   
    else
    {
        printf("recv buffer size=%d\n", snd_size);
    } 
}

int main(int argc, char **argv)
{
    int runmode;//运行模式
    int cmd;
    FILE *fprecord = NULL;
    int i;
    int  iret1, iret2, result;
    socklen_t socklen;
    
    if(argc <= 1)
    {
        printf("参数个数错误\n");
        return -1;
    }
    
    pthread_mutex_init(&mutex_lock, NULL);

    if(strcmp("capture", argv[1])==0)
    {
#if TRAN_MODE==UDP_MODE
    fdsocket = socket(AF_INET, SOCK_DGRAM, 0);
#elif TRAN_MODE==TCP_MODE
    fdsocket = socket(AF_INET, SOCK_STREAM, 0);
#endif    
    if (fdsocket == -1) 
    {
        perror("socket");
        return;
    }
    
        printf("客户端模式\n");
        runmode = RUNMODE_CLIENT;
        if(argc != 4)
        {
            printf("客户模式参数格式不正确: tape capture 192.168.2.100 20000\n");
            return -1;
        }
        else
        {
            printf("服务器ip  : %s\n", argv[2]);
            printf("服务器port: %s\n", argv[3]);
            serverport = atoi(argv[3]);
            memcpy(serverip, argv[2], strlen(argv[2]));
        }
    }
    else if(strcmp("play", argv[1])==0)
    {
            printf("服务端模式\n");
            runmode = RUNMODE_SERVER;
    }
    else
    {
            printf("未知的模式\n");
            return -1;
    }
    

    InitSocketBuffer();

    /* 设置远程连接的信息*/
    dest_addr.sin_family = AF_INET;                 /* 注意主机字节顺序*/
    dest_addr.sin_port = htons(serverport);          /* 远程连接端口, 注意网络字节顺序*/
    dest_addr.sin_addr.s_addr = inet_addr(serverip); /* 远程 IP 地址, inet_addr() 会返回网络字节顺序*/
    bzero(&(dest_addr.sin_zero), 8);                /* 其余结构须置 0*/    

    //绑定本地端口
    local_addr.sin_family = AF_INET;                 /* 注意主机字节顺序*/
    local_addr.sin_port = htons(serverport);          /* 远程连接端口, 注意网络字节顺序*/
    local_addr.sin_addr.s_addr = INADDR_ANY; /* 远程 IP 地址, inet_addr() 会返回网络字节顺序*/
    bzero(&(local_addr.sin_zero), 8);                /* 其余结构须置 0*/    


    if(bind(fdsocket, (struct sockaddr*)&local_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("绑定错误");
    }
    
    socklen = sizeof(struct sockaddr);
#if TRAN_MODE==TCP_MODE
    if(connect(fdsocket, (struct sockaddr*)&dest_addr, socklen) == -1)
    {
        perror("connect");
        return;
    }
#endif

    for(i=0;i<BUFFERNODECOUNT-1;i++)
    {
        audiobuffer[i].pNext = &audiobuffer[i+1];
    }
    audiobuffer[BUFFERNODECOUNT-1].pNext = &audiobuffer[0];
    pWriteHeader = &audiobuffer[0];
    pReadHeader  = &audiobuffer[0];

    //fprecord = fopen("r.pcm", "wb");

    //fclose(fprecord);
    printf("SAMPLERATE/1000*READMSFORONCE*sizeof(short)=%d\n", SAMPLERATE/1000*READMSFORONCE*sizeof(short));

    flag_capture_audio = 1;
    flag_play_audio    = 1;
    flag_network_send  = 1;
    flag_network_recv  = 1;
    
    if(runmode == RUNMODE_CLIENT)
    {
        result = sem_init(&sem_capture, 0, 0);
        if (result != 0)
        {
            perror("Semaphore capture initialization failed");
        }

        printf("创建发送与采集线程\n");
            iret1 = pthread_create(&thread_capture_audio, NULL, capture_audio_thread, (void*) NULL);
            iret1 = pthread_create(&thread_network_send, NULL, network_send_thread, (void*) NULL);
            
            iret1 = pthread_create(&thread_network_recv, NULL, network_recv_thread, (void*) NULL);
        //pthread_join(thread_capture_audio, NULL);
        //pthread_join(thread_network_send, NULL);
    }
#if 0
    else if(runmode == RUNMODE_SERVER)
    {
        result = sem_init(&sem_recv, 0, 0);
        if (result != 0)
        {
            perror("Semaphore recv initialization failed");
        }

        printf("创建播放与接收线程\n");
            iret2 = pthread_create(&thread_play_audio, NULL, play_audio_thread, (void*) NULL);  
            iret2 = pthread_create(&thread_network_recv, NULL, network_recv_thread, (void*) NULL);    
        //pthread_join(thread_play_audio, NULL);
        //pthread_join(thread_network_recv, NULL);
    }
#endif
    while(programrun)
    {
        usleep(1000);
        cmd = getchar();
        switch(cmd)
        {
            case 'A':
                printf("A");
                break;
            case 'Q':
                programrun = 0;
            default:
                break;
        }

    }

    usleep(1000*1000);

    if(runmode == RUNMODE_CLIENT)
    {
            remove_capture_audio();
            remove_network_send();
    }
#if 0
    else if(runmode == RUNMODE_SERVER)
    {
        printf("清除发送线程\n");
        remove_play_audio();
        remove_network_recv();
    }
#endif
}

