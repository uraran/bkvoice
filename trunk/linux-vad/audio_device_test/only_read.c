#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#define SAVE_RECORD_FILE         1
#define FRAME_LENGTH_MS         20 //一次采集毫秒数

int main( int argc, char* argv[] )
{
    int soundfd = 0;
    int format = AFMT_S16_LE;  
    int channels = 1;//采通道
    int setting=64;
    int API_fs_Hz = 24000;
    int counter;
    int nbytes;
    int readfrom;
    char ch;
    char *in;
#if SAVE_RECORD_FILE
    FILE *fp_record = fopen("record.pcm", "wb");
#endif
  
    nbytes = API_fs_Hz/1000*format/8*channels*FRAME_LENGTH_MS;
    soundfd = open("/dev/dsp", O_RDONLY);/*只读方式打开文件音频设备-lpthread文件*/
    if(soundfd<0)
    {
       perror("音频设备open");
       //return -1;
    }
    ioctl(soundfd, SNDCTL_DSP_SPEED, &API_fs_Hz);//采样频率
    ioctl(soundfd, SNDCTL_DSP_SETFMT, &format);//音频设备位宽
    ioctl(soundfd, SNDCTL_DSP_CHANNELS, &channels);//音频设备通道
    ioctl(soundfd, SNDCTL_DSP_SETFRAGMENT, &setting);//采样缓冲区

    in = (char*)malloc(nbytes);
    while(1)
    {
/*
        ch = getchar();
        if(ch == 'Q')
        {
            break;
        }
*/
        counter = read(soundfd, in,  nbytes);
        if(counter != nbytes)
        {
            printf("读入数据长度与预设不符, nbytes=%d,counter=%d\n", nbytes, counter);
        }
        else
        {
            printf("读入counter=%d字节\n", counter);
        }
#if SAVE_RECORD_FILE
        fwrite(in, 1, nbytes, fp_record);
#endif  
    }
  
    free(in);
    close(soundfd);
#if SAVE_RECORD_FILE
    fclose(fp_record);
#endif

}
