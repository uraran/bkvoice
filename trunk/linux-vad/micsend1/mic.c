#include <stdio.h>
#include <linux/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <semaphore.h>
#include <sys/time.h>
#include "config.h"

AUDIOBUFFER audiobuffer[BUFFERNODECOUNT];
AUDIOBUFFER *pWriteHeader = NULL;
AUDIOBUFFER *pReadHeader = NULL;
int n = 0;//可用包数

int Frequency = SAMPLERATE;
int format = AFMT_S16_LE;
int channels = 1;
int setting = 64;//0x00040009;

int flag_capture_audio = 0;
int flag_play_audio    = 0;
int flag_network_send  = 0;
int flag_network_recv  = 0;
int programrun = 1;     //退出程序

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

//音频采集线程
void * capture_audio_thread(void *para)
{
    int fdsound = 0;
    int readbyte = 0;
#if RECORD_CAPTURE_PCM
    FILE *fp = fopen("capture.pcm", "wb");
#endif

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
            //printf("cNO:%d\n", pWriteHeader->FrameNO);
            pthread_mutex_lock(&mutex_lock);
            n++;
            pWriteHeader->Valid = 1;
            pthread_mutex_unlock(&mutex_lock);
            pWriteHeader = pWriteHeader->pNext;
        }
    }

    close(fdsound);
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
                        traceprintf("n=%d\n", n);
                        //printf("    sNO:%d\n", pReadHeader->FrameNO);
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

