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
#include "wb_vad.h"
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
int channels = 1;
int setting = 64;//0x00040009;
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

#if 0
//音频采集线程
void * capture_audio_thread(void *para)
{
    int fdsound = 0;
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
        if(read(fdsound, pWriteHeader->buffer, SAMPLERATE/1000*READMSFORONCE) < 0)
        {
            perror("读声卡数据");
        }
        else
        {
            sem_post(&sem_capture);

#if RECORD_CAPTURE_PCM
            fwrite(pWriteHeader->buffer, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
            traceprintf("发送信号量 sem_capture\n");
            
            pWriteHeader->FrameNO = FrameNO++;
            printf("cNO:%d\n", pWriteHeader->FrameNO);
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
    struct sockaddr_in dest_addr;
    socklen_t socklen;
#if TRAN_MODE==UDP_MODE
    int fdsocket = socket(AF_INET, SOCK_DGRAM, 0);
#elif TRAN_MODE==TCP_MODE
    int fdsocket = socket(AF_INET, SOCK_STREAM, 0);
#endif    
    if (fdsocket == -1) 
    {
        perror("socket");
        return;
    }
    
    /* 设置远程连接的信息*/
    dest_addr.sin_family = AF_INET;                 /* 注意主机字节顺序*/
    dest_addr.sin_port = htons(serverport);          /* 远程连接端口, 注意网络字节顺序*/
    dest_addr.sin_addr.s_addr = inet_addr(serverip); /* 远程 IP 地址, inet_addr() 会返回网络字节顺序*/
    bzero(&(dest_addr.sin_zero), 8);                /* 其余结构须置 0*/    

    socklen = sizeof(struct sockaddr);
#if TRAN_MODE==TCP_MODE
    if(connect(fdsocket, (struct sockaddr*)&dest_addr, socklen) == -1)
    {
        perror("connect");
        return;
    }
#endif
    printf("数据发送开始\n");
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
                if(-1 == sendto(fdsocket, pReadHeader->buffer, sizeof(pReadHeader->buffer)+sizeof(int), 0, (struct sockaddr*)&dest_addr, socklen))
#elif TRAN_MODE==TCP_MODE
                if(-1 == send(fdsocket, pReadHeader->buffer, sizeof(pReadHeader->buffer), 0))
#endif
                {
                        perror("sendto");
                }
                else
                {
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
}

//清除发送线程
void remove_network_send()
{
    flag_network_send = 0;
    usleep(40*1000);
    pthread_cancel(thread_network_send);  
}
#endif

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

void * network_recv_thread(void *p)
{
    struct sockaddr_in local_addr, remote_addr;
    int result;
    int socklen;
    int fdsocket;
#if RECORD_RECV_PCM 
    FILE * fp = fopen("recv.pcm", "wb");
    if(!fp)
    {
        perror("open file");
    }
#endif
    
#if TRAN_MODE==UDP_MODE
    fdsocket = socket(AF_INET, SOCK_DGRAM, 0);
#elif TRAN_MODE==TCP_MODE
    fdsocket = socket(AF_INET, SOCK_STREAM, 0);//建立可靠tcp socket
#endif
    if (fdsocket == -1) 
    {
        perror("socket");
        return;
    }
    
    /* 设置远程连接的信息*/
    local_addr.sin_family = AF_INET;                 /* 注意主机字节顺序*/
    local_addr.sin_port = htons(8302);          /* 远程连接端口, 注意网络字节顺序*/
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* 远程 IP 地址, inet_addr() 会返回网络字节顺序*/
    //bzero(&(local_addr.sin_zero), 8);                /* 其余结构须置 0*/    
    
    if(bind(fdsocket, (struct sockaddr*)&local_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("绑定错误");
    }
    
#if TRAN_MODE==UDP_MODE
#elif TRAN_MODE==TCP_MODE
    struct sockaddr_in their_addr;/*connector'saddressinformation*/
    int sin_size;
    sin_size=sizeof(struct sockaddr_in);

    if(listen(fdsocket, 5)==-1)
    {
        perror("listen");
        return;
    }

    int connectsocket;
    if((connectsocket = accept(fdsocket, (struct sockaddr *)&their_addr, &sin_size)) == -1)
    {
        perror("accept");
        return;
    }


#endif

    printf("数据接收开始\n");
    socklen = sizeof(struct sockaddr);
    while(flag_network_recv)
    {
        {      
#if TRAN_MODE==UDP_MODE
            //if(-1 == sendto(fdsocket, pReadHeader->buffer, sizeof(pReadHeader->buffer), 0, (struct sockaddr*)&dest_addr, sizeof(struct sockaddr)))
            result = recvfrom(fdsocket, pWriteHeader->buffer, sizeof(pWriteHeader->buffer)+sizeof(int)+sizeof(int), 0, (struct sockaddr*)&remote_addr, &socklen);
#elif TRAN_MODE==TCP_MODE
            result = recv(connectsocket, pWriteHeader->buffer, sizeof(pWriteHeader->buffer), 0);
#endif
            if(result == -1)
            {
                    perror("data recv");
                    return;
            }
#if 0
            else if (result != sizeof(pWriteHeader->buffer)+sizeof(int))
            {
                    printf("不足%d字节\n", sizeof(pWriteHeader->buffer)+sizeof(int));
            }
#endif
            else
            {
#if RECORD_RECV_PCM 
                fwrite(pWriteHeader->buffer, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
                traceprintf("收到数据 %d byte\n", result);
                printf("rNO=%d\n", pWriteHeader->FrameNO);
                pthread_mutex_lock(&mutex_lock);
                pWriteHeader->Valid = 1;
                pWriteHeader->count = SAMPLERATE/1000*READMSFORONCE*sizeof(short);
                n++;
                pthread_mutex_unlock(&mutex_lock);
                pWriteHeader = pWriteHeader->pNext;
                sem_post(&sem_recv);
                //printf("收到%d字节\n", result);
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

//音频播放线程
void * play_audio_thread(void *para)
{
    int fdsoundplay = 0;
    struct timeval tv;
    struct timezone tz;
    time_t timep;
    struct tm *p;
#if VAD_ENABLED
  	VadVars *vadstate;
  	int vad;
  	int nZeroPackageCount;
    float indata[256];
    int i;
#endif


#if VAD_ENABLED
  	wb_vad_init(&(vadstate));
#endif

#if RECORD_PLAY_PCM 
    FILE * fp = fopen("play.pcm", "wb");
    if(!fp)
    {
        perror("open file");
    }
#endif
    unsigned int t_ms;
    
#if (SOUND_INTERFACE == SOUND_OSS)
    fdsoundplay = open("/dev/dsp", O_WRONLY);/*只写方式打开设备*/
    if(fdsoundplay<0)
    {
       perror("以只写方式打开音频设备");
       return;
    }


    printf("设置写音频设备参数 setup play audio device parament\n");
    ioctl(fdsoundplay, SNDCTL_DSP_SPEED, &Frequency);//采样频率
    ioctl(fdsoundplay, SNDCTL_DSP_SETFMT, &format);//音频设备位宽
    ioctl(fdsoundplay, SNDCTL_DSP_CHANNELS, &channels);//音频设备通道
    ioctl(fdsoundplay, SNDCTL_DSP_SETFRAGMENT, &setting);//采样缓冲区

    while(flag_play_audio)
    {
        sem_wait(&sem_recv);
        if(n > BUFFER_COUNT)
        {
            if(!pReadHeader->Valid)
            { 
                printf("忽略%d\n", pReadHeader->FrameNO);
                continue;
            }
#if 0
            time(&timep);
            p=localtime(&timep);   //get server's time

            printf("%d\n", p->tm_msec);
#endif

#if 0
            gettimeofday(&tv, &tz);
            t_ms = tv.tv_sec*1000 + tv.tv_usec/1000;
            printf("%d,%d,", tv.tv_sec, tv.tv_usec);
#endif

            if(write(fdsoundplay, pReadHeader->buffer, pReadHeader->count) < 0)
            {    
                perror("音频设备写错误\n");
            }
            else
            {
                pthread_mutex_lock(&mutex_lock);
                pReadHeader->Valid = 0;//数据已播放，不再有效
                n--;
                pthread_mutex_unlock(&mutex_lock);

#if RECORD_PLAY_PCM 
                fwrite(pReadHeader->buffer, pReadHeader->count, 1, fp);
#endif
                pReadHeader = pReadHeader->pNext;
            }
        }
        //fwrite(buffer, sizeof(buffer), 1, fprecord);
    }

    close(fdsoundplay);
#elif (SOUND_INTERFACE == SOUND_ALSA)
    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, "plughw:0,0", 
                    SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
    fprintf(stderr, 
            "unable to open pcm device: %s\n",
            snd_strerror(rc));
    exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params,
                      SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);

    /* 44100 bits/second sampling rate (CD quality) */
    val = SAMPLERATE;
    snd_pcm_hw_params_set_rate_near(handle, params, 
                                  &val, &dir);

    /* Set period size to 32 frames. */
    frames = SAMPLERATE/1000*READMSFORONCE;
    snd_pcm_hw_params_set_period_size_near(handle,
                              params, &frames, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
    fprintf(stderr, 
            "unable to set hw parameters: %s\n",
            snd_strerror(rc));
    exit(1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames,
                                    &dir);
#if 0                                    
    size = frames * sizeof(short) * channels; /* 2 bytes/sample, 2 channels */
    buffer = (char *) malloc(size);
#endif
    /* We want to loop for 5 seconds */
    snd_pcm_hw_params_get_period_time(params, &val, &dir); 
                                    
    while(flag_play_audio)
    {
        sem_wait(&sem_recv);
#if 0        
        rc = read(0, buffer, size);
        if (rc == 0) {
          fprintf(stderr, "end of file on input\n");
          break;
        } else if (rc != size) {
          fprintf(stderr,
                  "short read: read %d bytes\n", rc);
        }
#endif        
        //traceprintf("播放\n");
        if(n > BUFFER_COUNT)
        {
            rc = snd_pcm_writei(handle, pReadHeader->buffer, frames);
            if (rc == -EPIPE) {
              /* EPIPE means underrun */
              fprintf(stderr, "underrun occurred\n");
              snd_pcm_prepare(handle);
        } 
        else if (rc < 0) 
        {
            fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
                    
            rc = xrun_recovery(handle, rc);             
            if (rc < 0) 
            {
                printf("Write error: %s\n", snd_strerror(rc));
                //return -1;
            }                   
        }  
        else if (rc != (int)frames) 
        {
          fprintf(stderr, 
                  "short write, write %d frames\n", rc);
        }

 



#if VAD_ENABLED
        signed short * precdata = (signed short*)(&(pReadHeader->buffer[0]));

				for(i=0;i<256;i++)		//??????
				{
						indata[i]= (float)(precdata[i]);
				}
  			vad = wb_vad(vadstate,indata);	//??vad??
				//vad =1;//
        printf("vad=%d,nZeroPackageCount=%d,n=%d\n", vad, nZeroPackageCount, n);
				if(vad == 1)
				{
					nZeroPackageCount = 0;
				}
				else
				{
					nZeroPackageCount++;
				}
#endif




#if VAD_ENABLED
			if((vad==0) && (nZeroPackageCount > 50))
			{
				//printf("z=%d\n", nZeroPackageCount);
				nZeroPackageCount = 0;
				while(pReadHeader->pNext != pWriteHeader)
				{
				    printf("略过数据包 %d,n=%d\n", pReadHeader->FrameNO, n);
					  pReadHeader->Valid = 0;//因为是跳过的数据包//数据已播放，不再有效
					  
            pthread_mutex_lock(&mutex_lock);
            n--;
					  pReadHeader = pReadHeader->pNext;
            pthread_mutex_unlock(&mutex_lock);
				}
			}
			else
#endif
      {
        printf("          pNO=%d\n", pReadHeader->FrameNO);
        pthread_mutex_lock(&mutex_lock);
        pReadHeader->Valid = 0;//数据已播放，不再有效
        n--;
        pthread_mutex_unlock(&mutex_lock);

#if RECORD_PLAY_PCM 
        fwrite(pReadHeader->buffer, pReadHeader->count, 1, fp);
#endif
        pReadHeader = pReadHeader->pNext;
      }
                
#if 0
#if DEBUG_SAVE_PLAY_PCM
        fwrite(pReadHeader->buffer, sizeof(pReadHeader->buffer), 1, fp);
#endif
        pReadHeader = pReadHeader->pNext;
        n--;  
#endif
        }             
    }                                    
#endif

    fclose(fp);
    printf("音频播放线程已经关闭 audio play thread is closed\n");
    return NULL;
}

//清除音频播放线程
void remove_play_audio(void)
{
    sem_post(&sem_recv);
    flag_play_audio = 0;
    usleep(40*1000);
    pthread_cancel(thread_play_audio);
}

#define RUNMODE_SERVER    0  //接收端
#define RUNMODE_CLIENT    1  //发送端

int main(int argc, char **argv)
{
    int runmode;//运行模式
    int cmd;
    FILE *fprecord = NULL;
    int i;
    int  iret1, iret2, result;
    
    if(argc <= 1)
    {
        printf("参数个数错误\n");
        return -1;
    }
    
    pthread_mutex_init(&mutex_lock, NULL);

    if(strcmp("capture", argv[1])==0)
    {
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
    
    
    for(i=1;i<BUFFERNODECOUNT-1;i++)
    {
    		audiobuffer[i].pPrior = &(audiobuffer[i-1]);
        audiobuffer[i].pNext  = &(audiobuffer[i+1]);
    }
	  audiobuffer[0].pPrior =  &(audiobuffer[BUFFERNODECOUNT-1]);
	  audiobuffer[0].pNext  =  &(audiobuffer[1]);
	
	  audiobuffer[BUFFERNODECOUNT-1].pPrior =  &(audiobuffer[BUFFERNODECOUNT-2]);
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
#if 0
        result = sem_init(&sem_capture, 0, 0);
        if (result != 0)
        {
            perror("Semaphore capture initialization failed");
        }

        printf("创建发送与采集线程\n");
        iret1 = pthread_create(&thread_capture_audio, NULL, capture_audio_thread, (void*) NULL);
        iret1 = pthread_create(&thread_network_send, NULL, network_send_thread, (void*) NULL);
        //pthread_join(thread_capture_audio, NULL);
        //pthread_join(thread_network_send, NULL);
#endif
    }
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
#if 0
            remove_capture_audio();
            remove_network_send();
#endif
    }
    else if(runmode == RUNMODE_SERVER)
    {
        printf("清除发送线程\n");
        remove_play_audio();
        remove_network_recv();
    }
}

