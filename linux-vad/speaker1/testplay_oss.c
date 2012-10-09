#include <stdio.h>
#include <linux/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include "config.h"

int Frequency = SAMPLERATE;
int format = AFMT_S16_LE;
int channels = 1;
int setting = 64;//0x00040009;

int main(void)
{
    char buffer[512];
    int fdsoundplay = 0;
    FILE* fppcm = NULL;
    int status;   /* return status of system calls */

    printf("start\n");

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

    fppcm = fopen("play.pcm", "rb");
    if(fppcm == NULL)
    {
        printf("打开文件错误\n");
    }

    int recing = 1;

    while(!feof(fppcm))
    {
        fread(buffer, 512, 1, fppcm);
        status = write(fdsoundplay, buffer, 512);
        printf("status=%d\n", status);
        /* wait for playback to complete before recording again */
        //status = ioctl(fdsoundplay, SOUND_PCM_SYNC, 0); 
        if (status == -1)
          perror("SOUND_PCM_SYNC ioctl failed");
        //usleep(5*1000);
        
    }
}
