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
//#include "wb_vad.h"
#if (SOUND_INTERFACE == SOUND_OSS)
#include <sys/soundcard.h>
#elif (SOUND_INTERFACE == SOUND_ALSA)
#include <alsa/asoundlib.h>
#endif


#if SILK_AUDIO_CODEC
#include <SILK/interface/SKP_Silk_SDK_API.h>
#elif SPEEX_AUDIO_CODEC
#include <speex/speex.h>
#include <speex/speex_stereo.h>
#include <speex/speex_callbacks.h>
#include <speex/speex_preprocess.h>
#endif


#if SILK_AUDIO_CODEC
/* Define codec specific settings should be moved to h file */
#define MAX_BYTES_PER_FRAME     1024
#define MAX_INPUT_FRAMES        5
#define MAX_FRAME_LENGTH        480
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
#define MAX_LBRR_DELAY          2
#endif

struct Setting app_setting;

#define printf_debug(fmt, ...)    if(app_setting.debug_printf_enabled)   printf(fmt, __VA_ARGS__)
#define printf_notice(fmt, ...)   if(app_setting.notice_printf_enabled)  printf(fmt, __VA_ARGS__)
#define printf_error(fmt, ...)    if(app_setting.error_printf_enabled)   printf(fmt, __VA_ARGS__)

AUDIOBUFFER audiobuffer[BUFFERNODECOUNT];
AUDIOBUFFER *p_recv_header = NULL;//接收链表首指针
AUDIOBUFFER *p_decode_header = NULL;//解码链表首指针
AUDIOBUFFER *p_play_header = NULL;//播放链表首指针
int n_recv   = 0;//接收缓冲区包数
int n_decode = 0;//解码缓冲区包数

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
int flag_decode_audio  = 0;
int flag_play_audio    = 0;
int flag_network_send  = 0;
int flag_network_recv  = 0;
int programrun = 1;     //退出程序

pthread_t thread_t_capture_audio, pthread_t_play_audio;
pthread_t pthread_t_network_send,  pthread_t_network_recv, pthread_t_decode_audio;
pthread_mutex_t mutex_lock;

sem_t sem_recv;    //接收端用的信号量
sem_t sem_decode;
sem_t sem_capture; //发送端用的信号量

char serverip[15];
int  serverport;

int FrameNO = 0; //包序号

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

#if RECORD_RECV_FILE
    FILE* fp_recv_codec = fopen("recv.bits", "wb");
#endif

#if READFILE_SIMULATE_RCV
    FILE *fptest = fopen("play.pcm", "rb");
#endif

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
    local_addr.sin_port = htons(SERVER_PORT);          /* 远程连接端口, 注意网络字节顺序*/
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
#if READFILE_SIMULATE_RCV
            if(feof(fptest))
            {
                fseek(fptest, 0L, SEEK_SET);
            }
            p_recv_header->FrameNO++;
            fread(p_recv_header->buffer_recv, sizeof(p_recv_header->buffer_recv), 1, fptest);
            result = 520;
#else


#if SILK_AUDIO_CODEC || SPEEX_AUDIO_CODEC
            printf_debug("n_recv=%d, n_decode=%d\n", n_recv, n_decode);
            if(p_recv_header->received == 0)
            {
                result = recvfrom(fdsocket, &(p_recv_header->FrameNO), sizeof(p_recv_header->buffer_recv)+sizeof(int)*3+sizeof(time_t), 0, (struct sockaddr*)&remote_addr, &socklen);
                p_recv_header->count_recv = result;//实际字节数

#if RECORD_RECV_FILE
                fwrite(&(p_recv_header->count_recv), sizeof(int),1,  fp_recv_codec);
                fwrite(p_recv_header->buffer_recv, p_recv_header->count_recv, 1, fp_recv_codec);
#endif

                //printf("p_recv_header->count_recv=%d, p_recv_header->No=%d, p_recv_header->FrameN0=%d, p_recv_header->vad=%d, p_recv_header->count_encode=%d\n", p_recv_header->count_recv, p_recv_header->No, p_recv_header->FrameNO, p_recv_header->vad, p_recv_header->count_encode);
            }
            else
            {
                printf_notice("p_recv_header->received != 0\n", "");
            }
#else
            result = recvfrom(fdsocket, p_recv_header->buffer_recv, sizeof(p_recv_header->buffer_recv)+sizeof(int)+sizeof(int), 0, (struct sockaddr*)&remote_addr, &socklen);
#endif


#endif
#elif TRAN_MODE==TCP_MODE
            result = recv(connectsocket, p_recv_header->buffer_recv, sizeof(p_recv_header->buffer_recv), 0);
#endif
            if(result == -1)
            {
                    perror("data recv");
                    return;
            }
#if 0
            else if (result != sizeof(p_recv_header->buffer_recv)+sizeof(int))
            {
                    printf("不足%d字节\n", sizeof(p_recv_header->buffer_recv)+sizeof(int));
            }
#endif
            else
            {
#if RECORD_RECV_PCM 
                fwrite(p_recv_header->buffer_recv, SAMPLERATE/1000*READMSFORONCE*sizeof(short), 1, fp);
#endif


                sem_post(&sem_decode);
                sem_post(&sem_decode);
                //sem_post(&sem_recv);
                //sem_post(&sem_recv);
#if SILK_AUDIO_CODEC
                pthread_mutex_lock(&mutex_lock);
                p_recv_header->received = 1;
                n_recv++;
                pthread_mutex_unlock(&mutex_lock);
                p_recv_header = p_recv_header->pNext;
                //sem_post(&sem_recv);
                //sem_post(&sem_recv);
                sem_post(&sem_recv);
#else
                printf_debug("收到数据 %d byte\n", result);
                //printf("rNO=%d\n", p_recv_header->FrameNO);
                p_recv_header->count_recv = SAMPLERATE/1000*READMSFORONCE*sizeof(short);
                pthread_mutex_lock(&mutex_lock);
                p_recv_header->received = 1;
                n_recv++;
                pthread_mutex_unlock(&mutex_lock);
                p_recv_header = p_recv_header->pNext;
                sem_post(&sem_recv);
                //sem_post(&sem_recv);
#endif




#if READFILE_SIMULATE_RCV
                //usleep(14*1000);
#endif
                //sem_post(&sem_recv);
                //printf("收到%d字节\n", result);
            }
        }
    }
#if RECORD_RECV_PCM 
    fclose(fp);
#endif    

#if RECORD_RECV_FILE
    fclose(fp_recv_codec);
#endif

    printf_notice("数据接收线程已经关闭 data recv thread is closed\n", "");
    close(fdsocket);    
}

//清除接收线程
void remove_network_recv()
{
}

//解码线程
void* decode_audio_thread(void *p)
{
#if RECORD_DECODE_PCM
    FILE* fp_decode = fopen("decode.pcm", "wb");
#endif

#if RECORD_BEFORE_DECODE_FILE
    FILE* fp_before_decode = fopen("before_decode.bits", "wb");
#endif

#if SILK_AUDIO_CODEC
    size_t    counter;
    SKP_int32 args, totPackets, i, k;
    SKP_int16 result, length, tot_len;
    SKP_int16 nBytes;
    SKP_uint8 payload[    MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES * ( MAX_LBRR_DELAY + 1 ) ];
    SKP_uint8 *payloadEnd = NULL, *payloadToDec = NULL;
    SKP_uint8 FECpayload[ MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES ], *payloadPtr;
    SKP_int16 nBytesFEC;
    SKP_int16 nBytesPerPacket[ MAX_LBRR_DELAY + 1 ], totBytes;
    SKP_int16 out[ ( ( FRAME_LENGTH_MS * MAX_API_FS_KHZ ) << 1 ) * MAX_INPUT_FRAMES ], *outPtr;
    char      speechOutFileName[ 150 ], bitInFileName[ 150 ];
    FILE      *bitInFile, *speechOutFile;
    SKP_int32 API_Fs_Hz = 8000;
    SKP_int32 decSizeBytes;
    void      *psDec;
    float     loss_prob;
    SKP_int32 frames, lost, quiet;
    SKP_SILK_SDK_DecControlStruct DecControl;

#if RECORD_RECV_SILK_FILE
    FILE * fp_recv = fopen("recv.silk", "wb");

    static const char Silk_header[] = "#!SILK_V3";
    fwrite(Silk_header, sizeof( char ), strlen( Silk_header ), fp_recv);
#endif
    /* default settings */
    quiet     = 0;
    loss_prob = 0.0f;

    if( !quiet ) {
        //printf("******************* Silk Decoder v %s ****************\n", SKP_Silk_SDK_get_version());
        //printf("******************* Compiled for %d bit cpu ********* \\n", (int)sizeof(void*) * 8 );
        //printf( "Input:                       %s\n", bitInFileName );
        //printf( "Output:                      %s\n", speechOutFileName );
    }

    /* Set the samplingrate that is requested for the output */
    if( API_Fs_Hz == 0 ) {
        DecControl.API_sampleRate = 8000;
    } else {
        DecControl.API_sampleRate = API_Fs_Hz;
    }

    /* Initialize to one frame per packet, for proper concealment before first packet arrives */
    DecControl.framesPerPacket = 1;

    /* Create decoder */
    result = SKP_Silk_SDK_Get_Decoder_Size( &decSizeBytes );
    if( result ) {
        printf( "\nSKP_Silk_SDK_Get_Decoder_Size returned %d", result );
    }
    psDec = malloc( decSizeBytes );

    /* Reset decoder */
    result = SKP_Silk_SDK_InitDecoder( psDec );
    if( result ) {
        printf( "\nSKP_Silk_InitDecoder returned %d", result );
    }

    totPackets = 0;
    payloadEnd = payload;
#elif SPEEX_AUDIO_CODEC
    int i;
    int result, length, tot_len;
    /*保存编码的状态*/         
    static void *stateDecode; 
   /*保存字节因此他们可以被speex常规读写*/
    static SpeexBits bitsDecode;
   //模式寄存器
    static const SpeexMode *mode=NULL;
   //解码器的采样频率
    static int speexFrequency = SAMPLERATE; //编码器的采样率
   /*得到的缓冲区的大小*/  
    static spx_int32_t frame_size; 

#if HAVE_FPU
    float output[SAMPLERATE/1000*READMSFORONCE];
#endif

   /*得到的缓冲区的大小*/
   //static int channe = CHANNELS;
    /*得到是立体声*/
    //static SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT; //单声到 立体声

   /* 初始话IP端口结构*/
#if (SAMPLERATE == 8000)
    mode = speex_lib_get_mode (SPEEX_MODEID_NB); //宽带编码
#elif (SAMPLERATE == 16000)
    mode = speex_lib_get_mode (SPEEX_MODEID_WB); //宽带编码
#elif (SAMPLERATE == 32000)
    mode = speex_lib_get_mode (SPEEX_MODEID_UWB); //宽带编码
#endif
    //mode = speex_lib_get_mode (SPEEX_MODEID_UWB); //在宽带模式解码


    stateDecode = speex_decoder_init(mode);      //新建一个解码器 

    speex_encoder_ctl(stateDecode, SPEEX_GET_FRAME_SIZE, &frame_size); //得到缓冲区大小

    speex_decoder_ctl(stateDecode, SPEEX_SET_SAMPLING_RATE, &speexFrequency); //设置解码器的采样频率

    speex_bits_init(&bitsDecode); //初始解码器
#endif

    while(flag_decode_audio)
    {
        sem_wait(&sem_recv);//等待信号量
        //printf("等到 sem_recv\n");


        if((n_recv > 0) && (p_decode_header->received ==1))
        {
#if SILK_AUDIO_CODEC
            /* Decode 20 ms */
            result = SKP_Silk_SDK_Decode(psDec, &DecControl, 0, p_decode_header->buffer_recv, p_decode_header->count_recv, (p_decode_header->buffer_decode), &length);
            p_decode_header->count_decode = length * sizeof(short);//解码后字节数量
#elif SPEEX_AUDIO_CODEC


#if RECORD_BEFORE_DECODE_FILE
                //把解码前的数据录成文件
                fwrite(&(p_decode_header->count_recv), sizeof(int),1,  fp_before_decode);
                fwrite(p_decode_header->buffer_recv, p_decode_header->count_recv, 1, fp_before_decode);
#endif

            //接收的到的数据
            speex_bits_reset(&bitsDecode); //复位一个位状态变量
            //将编码数据如读入bits   
            speex_bits_read_from(&bitsDecode, p_decode_header->buffer_recv, p_decode_header->count_encode); 

#if HAVE_FPU
            result = speex_decode(stateDecode, &bitsDecode, output);
            for(i=0;i<SAMPLERATE/1000*READMSFORONCE;i++)
            {
                ((short*)(&(p_decode_header->buffer_decode)))[i] = output[i];
            } 
#else
            //对帧进行解码   
            result = speex_decode_int(stateDecode, &bitsDecode, p_decode_header->buffer_decode); //result  (0 for no error, -1 for end of stream, -2 corrupt stream)
#endif

            p_decode_header->count_decode = (SAMPLERATE/1000*READMSFORONCE*sizeof(short));



#endif




            if( result ) 
            {
                printf_error( "解码错误 returned result=%d,p_decode_header->count_recv= %d,No=%d\n", result, p_decode_header->count_recv, p_decode_header->No);
            }
            else
            {
#if RECORD_DECODE_PCM
                //FILE* fp_decode = fopen("decode.pcm", "wb");
                fwrite(p_decode_header->buffer_decode, p_decode_header->count_decode, 1, fp_decode);
#endif
                //printf("解码正常, 输出%d字节,returned result=%d,p_decode_header->count_decode= %d,No=%d,n_recv=%d,n_decode=%d\n", length, result, p_decode_header->count_decode, p_decode_header->No, n_recv, n_decode);
            }

            //sem_post(&sem_decode);
            //sem_post(&sem_decode);

            pthread_mutex_lock(&mutex_lock);
            n_recv--;
            p_decode_header->received = 0;
            p_decode_header->decoded = 1;
            n_decode++;
            pthread_mutex_unlock(&mutex_lock);
            p_decode_header = p_decode_header->pNext;
            sem_post(&sem_decode);

#if 0
            if(n_decode < 1)
            {
                p_decode_header->received = 0;
                p_decode_header->decoded = 1;
                n_decode++;
                result = speex_decode_int(stateDecode, &bitsDecode, p_decode_header->buffer_decode); 
                p_decode_header->count_decode = (SAMPLERATE/1000*READMSFORONCE*sizeof(short));
                sem_post(&sem_decode);

                p_decode_header = p_decode_header->pNext;
            }
#endif


            //sem_post(&sem_decode);
            //sem_post(&sem_decode);
            //sem_post(&sem_decode);
            //sem_post(&sem_decode);
        }





        //sem_post(&sem_decode);//投递信号量
    }

#if RECORD_DECODE_PCM
    fclose(fp_decode);
#endif

#if RECORD_BEFORE_DECODE_FILE
    fclose(fp_before_decode);
#endif
         
}

//音频播放线程
void* play_audio_thread(void *para)
{
    int fdsoundplay = 0;
    struct timeval tv;
    struct timezone tz;
    time_t timep;
    struct tm *p;
    int result;


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


    printf_debug("设置写音频设备参数 setup play audio device parament\n", "");
    ioctl(fdsoundplay, SNDCTL_DSP_SPEED, &Frequency);//采样频率
    ioctl(fdsoundplay, SNDCTL_DSP_SETFMT, &format);//音频设备位宽
    ioctl(fdsoundplay, SNDCTL_DSP_CHANNELS, &channels);//音频设备通道
    ioctl(fdsoundplay, SNDCTL_DSP_SETFRAGMENT, &setting);//采样缓冲区

    while(flag_play_audio)
    {
        sem_wait(&sem_decode);

        int buffer_count;
        if(n_decode >= BUFFER_COUNT)
        {
            buffer_count = 0;
        }
        else if (n_decode == 0)
        {
            buffer_count = BUFFER_COUNT;
        }/*
        else
        {
            buffer_count = 0;
        }
*/
        //printf("等到sem_decode信号量\n");
        if(n_decode > buffer_count)
        {
            if(!p_play_header->decoded)
            { 
                printf_debug("忽略%d\n", p_play_header->FrameNO);
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
            //result = ioctl(fdsoundplay, SOUND_PCM_SYNC, 0);//采样缓冲区
            //usleep(1*1000);
            if((result=write(fdsoundplay, p_play_header->buffer_decode, p_play_header->count_decode)) < 0)
            {    
                perror("音频设备写错误\n");
            }
            else
            {
                //printf("result=%d\n", result);
                if(result != p_play_header->count_decode)
                {   
                    printf_debug("result=%d\n", result);
                }

                pthread_mutex_lock(&mutex_lock);
                p_play_header->received = 0;//数据已播放，不再有效
                p_play_header->decoded = 0;//数据已播放，不再有效
                n_decode--;
                pthread_mutex_unlock(&mutex_lock);

#if RECORD_PLAY_PCM 
                result = fwrite(p_play_header->buffer_decode, p_play_header->count_decode, 1, fp);
                if(result != p_play_header->count_decode)
                {
                    printf_error("写入数据长度与预期不符\n");
                }
#endif
                p_play_header = p_play_header->pNext;
            }
        }
        else
        {
            printf_debug("n_decode=%d <= buffer_count=%d,n_recv=%d,不写声卡\n", n_decode, buffer_count, n_recv);
        }
        //fwrite(buffer, sizeof(buffer), 1, fprecord);
    }


    close(fdsoundplay);
#elif (SOUND_INTERFACE == SOUND_ALSA)
    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);

    if (rc < 0) 
    {
        fprintf(stderr,  "unable to open pcm device: %s\n",
            snd_strerror(rc));
        exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);

    /* 44100 bits/second sampling rate (CD quality) */
    val = SAMPLERATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    /* Set period size to 32 frames. */
    frames = SAMPLERATE/1000*READMSFORONCE;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) 
    {
        fprintf(stderr,  "unable to set hw parameters: %s\n",
            snd_strerror(rc));
        exit(1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
#if 0                                    
    size = frames * sizeof(short) * channels; /* 2 bytes/sample, 2 channels */
    buffer = (char *) malloc(size);
#endif
    /* We want to loop for 5 seconds */
    snd_pcm_hw_params_get_period_time(params, &val, &dir); 
                                    
    while(flag_play_audio)
    {
        sem_wait(&sem_decode);

        int buffer_count;
        if(n_decode >= BUFFER_COUNT)
        {
            buffer_count = 0;
        }
        else if (n_decode < 2)
        {
            buffer_count = BUFFER_COUNT;
        }

        //traceprintf("播放\n");
        if((n_decode >= buffer_count) && (p_play_header->decoded==1))
        {
            //printf("play: p_play_header->No=%d, n_decode=%d,buffer_count=%d\n", p_play_header->No, n_decode, buffer_count);
            rc = snd_pcm_writei(handle, p_play_header->buffer_decode, frames);
            if (rc == -EPIPE) 
            {
                /* EPIPE means underrun */
                fprintf(stderr, "underrun occurred buffer_count=%d, n_decode=%d\n", buffer_count, n_decode);
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
                fprintf(stderr, "short write, write %d frames\n", rc);
            }




#if VAD_ENABLED
			      if((vad==0) && (nZeroPackageCount > 50))
			      {
				      //printf("z=%d\n", nZeroPackageCount);
				      nZeroPackageCount = 0;
				      while(p_play_header->pNext != p_recv_header)
				      {
				          printf("略过数据包 %d,n=%d\n", p_play_header->FrameNO, n);
					        p_play_header->Valid = 0;//因为是跳过的数据包//数据已播放，不再有效
					        
                  pthread_mutex_lock(&mutex_lock);
                  n--;
					        p_play_header = p_play_header->pNext;
                  pthread_mutex_unlock(&mutex_lock);
				      }
			      }
			      else
#endif
            {
              //printf("          pNO=%d\n", p_play_header->FrameNO);
              pthread_mutex_lock(&mutex_lock);
              n_decode--;
              p_play_header->received = 0;//数据已播放，不再有效
              p_play_header->decoded  = 0;//数据已播放，不再有效
              pthread_mutex_unlock(&mutex_lock);

#if RECORD_PLAY_PCM 
              //printf("p_play_header->No=%d, p_play_header->count_decode=%d\n", p_play_header->No, p_play_header->count_decode);
              fwrite(p_play_header->buffer_decode, p_play_header->count_decode, 1, fp);
#endif
              p_play_header = p_play_header->pNext;
            }
        } //end n > BUFFERCOUNT
    }//end while                                    
#endif


#if RECORD_PLAY_PCM 
    fclose(fp);
#endif

    printf("音频播放线程已经关闭 audio play thread is closed\n");
    return NULL;
}

//清除音频播放线程
void remove_play_audio(void)
{
    sem_post(&sem_recv);
    flag_play_audio = 0;
    usleep(40*1000);
    pthread_cancel(pthread_t_play_audio);
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

    pthread_mutex_init(&mutex_lock, NULL);

#if 0
    if(argc <= 1)
    {
        printf("参数个数错误\n");
        return -1;
    }

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
#endif
    runmode = RUNMODE_SERVER;
    
    for(i=1;i<BUFFERNODECOUNT-1;i++)
    {
    		audiobuffer[i].pPrior = &(audiobuffer[i-1]);
        audiobuffer[i].pNext  = &(audiobuffer[i+1]);
        audiobuffer[i].No     = i;
    }
	  audiobuffer[0].pPrior =  &(audiobuffer[BUFFERNODECOUNT-1]);
	  audiobuffer[0].pNext  =  &(audiobuffer[1]);
    audiobuffer[0].No     = 0;

	  audiobuffer[BUFFERNODECOUNT-1].pPrior =  &(audiobuffer[BUFFERNODECOUNT-2]);
    audiobuffer[BUFFERNODECOUNT-1].pNext = &audiobuffer[0];
    audiobuffer[BUFFERNODECOUNT-1].No    = BUFFERNODECOUNT-1;

    p_recv_header    = &audiobuffer[0];
    p_decode_header  = &audiobuffer[0];
    p_play_header    = &audiobuffer[0];
    //fprecord = fopen("r.pcm", "wb");

    //fclose(fprecord);
    printf_debug("SAMPLERATE/1000*READMSFORONCE*sizeof(short)=%d\n", SAMPLERATE/1000*READMSFORONCE*sizeof(short));

    flag_capture_audio = 1;
    flag_play_audio    = 1;
    flag_decode_audio  = 1;
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

        result = sem_init(&sem_decode, 0, 0);
        if (result != 0)
        {
            perror("Semaphore recv initialization failed");
        }

        printf_debug("创建播放与接收线程\n", "");
        iret2 = pthread_create(&pthread_t_play_audio, NULL, play_audio_thread, (void*) NULL);  
        iret2 = pthread_create(&pthread_t_network_recv, NULL, decode_audio_thread, (void*) NULL);    
        iret2 = pthread_create(&pthread_t_decode_audio, NULL, network_recv_thread, (void*) NULL);    
        //pthread_join(thread_play_audio, NULL);
        //pthread_join(thread_network_recv, NULL);
    }

    while(programrun)
    {
        usleep(1000);
        cmd = getchar();
        switch(cmd)
        {
            case 'd':
            case 'D':
                app_setting.debug_printf_enabled = !app_setting.debug_printf_enabled;
                break;

            case 'n':
            case 'N':
                app_setting.notice_printf_enabled = !app_setting.notice_printf_enabled;
                break;

            case 'e':
            case 'E':
                app_setting.error_printf_enabled = !app_setting.error_printf_enabled;
                break;

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
        printf_debug("清除发送线程\n", "");
        remove_play_audio();
        remove_network_recv();
    }
}

