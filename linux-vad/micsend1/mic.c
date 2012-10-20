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



#if SPEEX_AUDIO_CODEC
#include <speex/speex.h>
#include <speex/speex_stereo.h>
#include <speex/speex_callbacks.h>
#include <speex/speex_preprocess.h>
#endif


#if SILK_AUDIO_CODEC //SILK启用
#include <SILK/interface/SKP_Silk_SDK_API.h>
/* Define codec specific settings */
#define MAX_BYTES_PER_FRAME     250 // Equals peak bitrate of 100 kbps 
#define MAX_INPUT_FRAMES        5
#define MAX_LBRR_DELAY          2
#define MAX_FRAME_LENGTH        480
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
SKP_int32 k, args, totPackets, totActPackets, ret;
//SKP_int16 nBytes;
double    sumBytes, sumActBytes, avg_rate, act_rate, nrg;
SKP_uint8 payload[ MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES ];
SKP_int16 in[ FRAME_LENGTH_MS * MAX_API_FS_KHZ * MAX_INPUT_FRAMES ];
char      speechInFileName[ 150 ], bitOutFileName[ 150 ];
FILE      *bitOutFile, *speechInFile;
SKP_int32 encSizeBytes;
void      *psEnc;
/* default settings */
SKP_int32 API_fs_Hz = 8000;
SKP_int32 max_internal_fs_Hz = 0;
SKP_int32 targetRate_bps = 25000;
SKP_int32 packetSize_ms = 20;
SKP_int32 frameSizeReadFromFile_ms = 20;
SKP_int32 packetLoss_perc = 0, complexity_mode = 0, smplsSinceLastPacket;
SKP_int32 INBandFEC_enabled = 0, DTX_enabled = 0, quiet = 0;
SKP_SILK_SDK_EncControlStruct encControl; // Struct for input to encoder

void init_silk_encoder()
{    
    /* If no max internal is specified, set to minimum of API fs and 24 kHz */
    if( max_internal_fs_Hz == 0 ) {
        max_internal_fs_Hz = 8000;
        if( API_fs_Hz < max_internal_fs_Hz ) {
            max_internal_fs_Hz = API_fs_Hz;
        }
    }


    /* Print options */
    if( !quiet ) {
        printf("******************* Silk Encoder v %s ****************\n", SKP_Silk_SDK_get_version());
        printf("******************* Compiled for %d bit cpu ********* \n", (int)sizeof(void*) * 8 );
        printf( "Input:                          %s\n",     speechInFileName );
        printf( "Output:                         %s\n",     bitOutFileName );
        printf( "API sampling rate:              %d Hz\n",  API_fs_Hz );
        printf( "Maximum internal sampling rate: %d Hz\n",  max_internal_fs_Hz );
        printf( "Packet interval:                %d ms\n",  packetSize_ms );
        printf( "Inband FEC used:                %d\n",     INBandFEC_enabled );
        printf( "DTX used:                       %d\n",     DTX_enabled );
        printf( "Complexity:                     %d\n",     complexity_mode );
        printf( "Target bitrate:                 %d bps\n", targetRate_bps );
    }

    /* Create Encoder */
    ret = SKP_Silk_SDK_Get_Encoder_Size( &encSizeBytes );
    if( ret ) {
        printf( "\nSKP_Silk_create_encoder returned %d", ret );
    }

    printf("encSizeBytes: %d\n", encSizeBytes);
    psEnc = malloc(encSizeBytes);

    /* Reset Encoder */
    ret = SKP_Silk_SDK_InitEncoder( psEnc, &encControl );
    if(ret) 
    {
        printf( "\nSKP_Silk_reset_encoder returned %d", ret );
    }
    
    //printf("111111111111\n");
    /* Set Encoder parameters */
    encControl.API_sampleRate        = API_fs_Hz;
    encControl.maxInternalSampleRate = max_internal_fs_Hz;
    encControl.packetSize            = ( packetSize_ms * API_fs_Hz ) / 1000;
    encControl.packetLossPercentage  = packetLoss_perc;
    encControl.useInBandFEC          = INBandFEC_enabled;
    encControl.useDTX                = DTX_enabled;
    encControl.complexity            = complexity_mode;
    encControl.bitRate               = ( targetRate_bps > 0 ? targetRate_bps : 0 );

    if( API_fs_Hz > MAX_API_FS_KHZ * 1000 || API_fs_Hz < 0 ) {
        printf( "\nError: API sampling rate = %d out of range, valid range 8000 - 48000 \n \n", API_fs_Hz );
        exit( 0 );
    }

    //printf("22222222\n");
    totPackets           = 0;
    totActPackets        = 0;
    smplsSinceLastPacket = 0;
    sumBytes             = 0.0;
    sumActBytes          = 0.0;


}  
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
#if SPEEX_AUDIO_CODEC
    char cbits[500];
    //int nbBytes;
    void *state;
    SpeexBits bits;
    int tmp;
    SpeexPreprocessState *st;
    float f;
    //int vad;
#endif

#if RECORD_CAPTURE_PCM
    FILE *fp = fopen("capture.pcm", "wb");
#endif

#if SILK_AUDIO_CODEC
    SKP_uint8 encode[MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES];
#endif

#if SILK_AUDIO_CODEC
    init_silk_encoder();//初始化silk codec
    printf("end of init silk encoder\n");
#endif

#if SPEEX_AUDIO_CODEC
   st = speex_preprocess_state_init(SAMPLERATE/1000*READMSFORONCE, SAMPLERATE);
   tmp=1;
   speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &tmp);
    tmp=1;
   speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_VAD, &tmp);

   tmp=0;
   speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC, &tmp);
   tmp=8000;
   speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC_LEVEL, &tmp);
   tmp=0;
   speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DEREVERB, &tmp);
   f=.0;
   speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
   f=.0;
   speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);

    /*Create a new encoder state in narrowband mode*/
    state = speex_encoder_init(&speex_uwb_mode);

    /*Set the quality to 8 (15 kbps)*/
    tmp=8;
    speex_encoder_ctl(state, SPEEX_SET_QUALITY, &tmp);

    //复杂度
    tmp = 10;
    speex_encoder_ctl(state, SPEEX_SET_COMPLEXITY, &tmp); 

    tmp=1;
    speex_encoder_ctl(state, SPEEX_SET_VBR, &tmp); //使能VBR数据质量
    /*Initialization of the structure that holds the bits*/
   speex_bits_init(&bits);
#endif













#if (SOUND_INTERFACE == SOUND_OSS)
    fdsound = open("/dev/dsp", O_RDONLY);
    if(fdsound<0)
    {
       perror("以只写方式打开音频设备");
       //return;
    }    

    printf("设置读音频设备参数 setup capture audio device parament\n");
    ioctl(fdsound, SNDCTL_DSP_SPEED, &Frequency);//采样频率
    ioctl(fdsound, SNDCTL_DSP_SETFMT, &format);//音频设备位宽
    ioctl(fdsound, SNDCTL_DSP_CHANNELS, &channels);//音频设备通道
    ioctl(fdsound, SNDCTL_DSP_SETFRAGMENT, &setting);//采样缓冲区

    while(flag_capture_audio)
    {
        if((readbyte=read(fdsound, pWriteHeader->buffer_capture, SAMPLERATE/1000*READMSFORONCE*sizeof(short))) < 0)
        {
            perror("读声卡数据");
        }
        else
        {
            sem_post(&sem_capture);
            //printf("readbyte=%d\n", readbyte);
#if RECORD_CAPTURE_PCM
            fwrite(pWriteHeader->buffer_capture, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
            traceprintf("发送信号量 sem_capture\n");
            
            pWriteHeader->FrameNO = FrameNO++;



#if SPEEX_AUDIO_CODEC

#if 0
            pWriteHeader->vad = speex_preprocess_run(st, pWriteHeader->buffer_capture);
#endif
            speex_bits_reset(&bits);

            /*Encode the frame*/
            speex_encode_int(state, pWriteHeader->buffer_capture, &bits);

            /*Copy the bits to an array of char that can be written*/
            pWriteHeader->count_encode = speex_bits_write(&bits, pWriteHeader->buffer_encode, 200);

            //printf("压缩后大小 %d\n", pWriteHeader->count_encode);
#endif
            time(&(pWriteHeader->time));
            printf("cNO:%d, readbyte=%d,压缩后大小%d,pWriteHeader->vad=%d\n", pWriteHeader->FrameNO, readbyte, pWriteHeader->count_encode, pWriteHeader->vad);
            pthread_mutex_lock(&mutex_lock);
            n++;
            pWriteHeader->Valid = 1;
            pthread_mutex_unlock(&mutex_lock);
            pWriteHeader = pWriteHeader->pNext;
        }
    }
    close(fdsound);
#elif (SOUND_INTERFACE == SOUND_ALSA)
    printf("alsa xxxxxxxxxxxxxxx\n");
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
        rc = snd_pcm_readi(handle, pWriteHeader->buffer_capture, frames);
        printf("read rc=%d\n", rc);
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
            fwrite(pWriteHeader->buffer_capture, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
#if 0
#if DEBUG_SAVE_CAPTURE_PCM
        rc = fwrite(pWriteHeader->buffer_capture, frames*sizeof(short), 1, fp);
        if(rc != 1)
        {
            fprintf(stderr, "write error %d\n", rc);
        }
#endif 
#endif
#if SILK_AUDIO_CODEC
            /* max payload size */
            int nbBytes = MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES;
            /* Silk Encoder */
            ret = SKP_Silk_SDK_Encode(psEnc, &encControl, pWriteHeader->buffer_capture, (SKP_int16)SAMPLERATE/1000*READMSFORONCE, pWriteHeader->buffer_encode, &nbBytes);
            pWriteHeader->count = nbBytes;
            printf("ret=%d, nbBytes=%d\n", ret, nbBytes);
            if(ret) 
            { 
                printf( "SKP_Silk_Encode returned %d, nbBytes=%d\n", ret, nbBytes);
                //break;
            }
#endif


#if SPEEX_AUDIO_CODEC
            speex_bits_reset(&bits);

            /*Encode the frame*/
            speex_encode_int(state, pWriteHeader->buffer_capture, &bits);

            /*Copy the bits to an array of char that can be written*/
            nbBytes = speex_bits_write(&bits, cbits, 200);

            printf("压缩后大小 %d\n", nbBytes);
#endif

        sem_post(&sem_capture); 
        sem_post(&sem_capture);                     
        gettimeofday(&tv, &tz);
        ((AUDIOBUFFER*)(pWriteHeader->buffer_capture))->FrameNO = FrameNO++;
        //((AUDIOBUFFER*)(pWriteHeader->buffer))->sec = tv.tv_sec;
        //((AUDIOBUFFER*)(pWriteHeader->buffer))->usec = tv.tv_usec;
        //printf("capture NO=%5d \n", FrameNO);
        pthread_mutex_lock(&mutex_lock);     
        //pWriteHeader = pWriteHeader->pNext;
        pWriteHeader->Valid = 1;
        n++;                
        pthread_mutex_unlock(&mutex_lock);  
        pWriteHeader = pWriteHeader->pNext;
        sem_post(&sem_capture);                     
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

#if SPEEX_AUDIO_CODEC
speex_preprocess_state_destroy(st);
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
    int result;
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
#if SILK_AUDIO_CODEC || SPEEX_AUDIO_CODEC
                printf("发送%d,字节%d\n", pReadHeader->FrameNO, sizeof(int)*3 + sizeof(time_t) + pReadHeader->count_encode);
                result = sendto(fdsocket, &(pReadHeader->FrameNO), sizeof(int)*3 + sizeof(time_t) + pReadHeader->count_encode, 0, (struct sockaddr*)&dest_addr, socklen);
                printf("实际发送%d字节\n", result);
                if(-1 == result)
#else
                result = sendto(fdsocket, pReadHeader->buffer_capture, sizeof(pReadHeader->buffer_capture)+sizeof(int)+sizeof(int), 0, (struct sockaddr*)&dest_addr, socklen);
                if(-1 == result)
#endif
#elif TRAN_MODE==TCP_MODE
                if(-1 == send(fdsocket, pReadHeader->buffer_capture, sizeof(pReadHeader->buffer_capture), 0))
#endif
                {
                        perror("sendto");
                }
                else
                {
#if RECORD_SEND_PCM
                        fwrite(pReadHeader->buffer_capture, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif
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
            result = recvfrom(fdsocket, pWriteHeader->buffer_capture, sizeof(pWriteHeader->buffer_capture)+sizeof(int)+sizeof(int), 
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
    
#if SPEEX_AUDIO_CODEC
#if 0
    SpeexEncoderInit();
#endif
#endif

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
#if 0
    //绑定本地端口
    local_addr.sin_family = AF_INET;                 /* 注意主机字节顺序*/
    local_addr.sin_port = htons(serverport);          /* 远程连接端口, 注意网络字节顺序*/
    local_addr.sin_addr.s_addr = INADDR_ANY; /* 远程 IP 地址, inet_addr() 会返回网络字节顺序*/
    bzero(&(local_addr.sin_zero), 8);                /* 其余结构须置 0*/    


    if(bind(fdsocket, (struct sockaddr*)&local_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("绑定错误");
    }
#endif
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

