#include <speex/speex.h>
#include <stdio.h>
#include <alsa/asoundlib.h>

#define SOUND_OSS                1
#define SOUND_ALSA               2
#define SOUND_INTERFACE          SOUND_ALSA

#define SAMPLERATE           32000
#define CHANNELS                 1

#if (SOUND_INTERFACE == SOUND_OSS)
#include <sys/soundcard.h>
#endif


#if (SOUND_INTERFACE == SOUND_OSS)

int Frequency = SAMPLERATE;
int format = AFMT_S16_LE;
int setting = 64;//0x00040009;
int fdsoundplay = 0;
#endif

int channels = CHANNELS;//立体声







#if (SOUND_INTERFACE == SOUND_ALSA)
#define ALSA_PCM_NEW_HW_PARAMS_API

int rc; 
snd_pcm_t *handle; 		
snd_pcm_uframes_t frames; 
int size; 		
char *buffer; 
int rate=32000;//采样率


/****************************************************************************************
*   函数名: init_alsa_play
*   输  入: 无
*   输  出: 无
*   功能说明：alsa音频播放器初始化
*
*       
******************************************************************************************/
void init_alsa_play()
{
		snd_pcm_hw_params_t *params; 
		unsigned int val; 
		int dir; 
				
		/* Open PCM device for playback. */
		rc=snd_pcm_open(&handle,"plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
		if (rc< 0) 
		{ 
			fprintf(stderr,  "unable to open pcm device: %s\n",    snd_strerror(rc));
			exit(1); 
		} 
		 
		/* Allocate a hardware parameters object. */
		 snd_pcm_hw_params_alloca(&params); 
		/* Fill it in with default values. */
		 snd_pcm_hw_params_any(handle, params);
		 /* Set the desired hardware parameters. */ 

		/* Interleaved mode */ 
		snd_pcm_hw_params_set_access(handle,params, SND_PCM_ACCESS_RW_INTERLEAVED); 

		/* Signed 16-bit little-endian format 格式*/ 
		snd_pcm_hw_params_set_format(handle,params, SND_PCM_FORMAT_S16_LE); 


		/* Two channels (stereo) 声道*/  
		snd_pcm_hw_params_set_channels(handle, params, channels); 
		/* 44100 bits/second sampling rate (CD quality) 采样率设置*/ 
		snd_pcm_hw_params_set_rate_near(handle,params, &rate, &dir);  /* Set period size to 32 frames. */ 
		frames = 640; 
		snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
		/* Write the parameters to the driver */
		 rc = snd_pcm_hw_params(handle, params);
		 if (rc < 0) 
		 {   
				fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));   
				exit(1); 
		}

		/*Use a buffer large enough to hold one period*/
		snd_pcm_hw_params_get_period_size(params, &frames,  &dir); //32个采样点算一帧
		size = frames * (channels);
		/* 2 bytes/sample, 2 channels */ 
		buffer = (char *) malloc(size); 
		/* We want to loop for 5 seconds */
		snd_pcm_hw_params_get_period_time(params, &val, &dir); 
}

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
#elif (SOUND_INTERFACE == SOUND_OSS)
void init_oss_play()
{
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
}
#endif


/*The frame size in hardcoded for this sample code but it doesn't have to be*/
#define FRAME_SIZE 640
int main(int argc, char **argv)
{
   char *outFile;
   char *inFile[1];
// FILE *fout;
   FILE *fin[1];
   /*Holds the audio that will be written to file (16 bits per sample)*/
   short out[FRAME_SIZE];

    //int i;
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

   char cbits[200];
   int nbBytes;
   /*Holds the state of the decoder*/
   //void *state;
   /*Holds bits so they can be read and written to by the Speex routines*/
   //SpeexBits bits;
   int i, tmp;
   int rc;

   printf("%s: start\n", __func__);


#if (SOUND_INTERFACE == SOUND_OSS)
	  init_oss_play();
#elif (SOUND_INTERFACE == SOUND_ALSA)
	 init_alsa_play();
#endif
   /*Create a new decoder state in narrowband mode*/
   stateDecode = speex_decoder_init(&speex_uwb_mode);

   /*Set the perceptual enhancement on*/
   tmp=1;
   speex_decoder_ctl(stateDecode, SPEEX_SET_ENH, &tmp);
   //printf("%s: 22222222\n", __func__);
	 inFile[0]	= argv[1];
	 //inFile[1]	= argv[2];	 
//   outFile = argv[1];
   //fout = fopen(outFile, "w");
   fin[0] = fopen(inFile[0], "r");//打开输入的spx文件
   
   if(fin[0] ==NULL)
   {
     perror("打开文件错误");
     exit(1);
   }
   
   //printf("%s: 3333333\n", __func__);
   /*Initialization of the structure that holds the bits*/
   speex_bits_init(&bitsDecode);
   //printf("%s: 4444444\n", __func__);   
   while (1)
   {   //printf("%s: 55555\n", __func__);
      //printf("%s: 1111111\n", __func__);
speex_bits_reset(&bitsDecode); 
      /*Read the size encoded by sampleenc, this part will likely be 
        different in your application*/
      fread(&nbBytes, sizeof(int), 1, fin[0]);
      fprintf (stderr, "nbBytes: %d\n", nbBytes);
      if (feof(fin[0]))
         break;
         //printf("%s: 666\n", __func__);
      /*Read the "packet" encoded by sampleenc*/
      fread(cbits, 1, nbBytes, fin[0]);
      /*Copy the data into the bit-stream struct*/
      speex_bits_read_from(&bitsDecode, cbits, nbBytes);

      /*Decode the data*/
      speex_decode_int(stateDecode, &bitsDecode, out);
#if 0
      /*Copy from float to short (16 bits) for output*/
      for (i=0;i<FRAME_SIZE;i++)
         out[i]=output[i];
   //printf("%s: 88888\n", __func__);
      /*Write the decoded audio to file*/
      //fwrite(out, sizeof(short), FRAME_SIZE, fout);
#endif 


#if (SOUND_INTERFACE == SOUND_OSS)
      rc = write(fdsoundplay, out, 640*sizeof(short));
      if(rc != 640*sizeof(short))
      {
          printf("写入数据长度与预期不符合\n");
      }
#elif (SOUND_INTERFACE == SOUND_ALSA)
      rc = snd_pcm_writei(handle, out, 640); 
         //printf("%s: 99999\n", __func__);

            if (rc == -EPIPE) 
            {
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
            else if (rc != (int)640) 
            {
                fprintf(stderr, "short write, write %d frames\n", rc);
            }
#endif
   }
   
printf("end\n");
   /*Destroy the decoder state*/
   speex_decoder_destroy(stateDecode);
   /*Destroy the bit-stream truct*/
   speex_bits_destroy(&bitsDecode);
   fclose(fin[0]);
   return 0;
}
