#include "stdio.h"   
#include "wb_vad.h"   
#include "cmnMemory.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/soundcard.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>

#define LENGTH 16    /* how many seconds of speech to store */
#define RATE 16000   /* the sampling rate */
#define SIZE 16      /* sample size: 8 or 16 bits */
#define CHANNELS 1  /* 1 = mono 2 = stereo */

/* this buffer holds the digitized audio */
unsigned char buf[LENGTH*RATE*SIZE*CHANNELS/8/1000 + 8];
#define SERVERIP      "192.168.2.6"
#define SERVERPORT    8302 
int main()   
{      
    int fd;	/* sound device file descriptor */
    int arg;	/* argument for ioctl calls */
    int status;   /* return status of system calls */
    int i,frame=0,temp,vad;    
    //float indata[FRAME_LEN];   
    short indata[FRAME_LEN];   
    VadVars *vadstate;                     
    FILE *fp1;   
    VO_MEM_OPERATOR voMemoprator;
    struct sockaddr_in dest_addr;
    unsigned int* PackageNO;//包序号
    unsigned int tmpPackageNO=0;

    int fdsocket = socket(AF_INET, SOCK_DGRAM, 0);    
    if (fdsocket == -1) 
    {
      perror("socket(创建套接字)");
      return -1;
    }

    /* 设置远程连接的信息*/
    dest_addr.sin_family = AF_INET;                 /* 注意主机字节顺序*/
    dest_addr.sin_port = htons(SERVERPORT);          /* 远程连接端口, 注意网络字节顺序*/
    dest_addr.sin_addr.s_addr = inet_addr(SERVERIP); /* 远程 IP 地址, inet_addr() 会返回网络字节顺序*/
    memset(&(dest_addr.sin_zero), 0, 8);                /* 其余结构须置 0*/   

    /* open sound device */
    fd = open("/dev/dsp", O_RDONLY);
    if (fd < 0) {
      perror("open of /dev/dsp failed");
      exit(1);
    }

    printf("xxxxxxxxxxxxxxxxxxxxx\n");
    /* set sampling parameters */
    arg = SIZE;	   /* sample size */
    status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
    if (status == -1)
      perror("SOUND_PCM_WRITE_BITS ioctl failed");
    if (arg != SIZE)
      perror("unable to set sample size");

    arg = CHANNELS;  /* mono or stereo */
    status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
    if (status == -1)
      perror("SOUND_PCM_WRITE_CHANNELS ioctl failed");
    if (arg != CHANNELS)
      perror("unable to set number of channels");

    arg = RATE;	   /* sampling rate */
    status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
    if (status == -1)
      perror("SOUND_PCM_WRITE_WRITE ioctl failed");

    voMemoprator.Alloc = cmnMemAlloc;
    voMemoprator.Copy = cmnMemCopy;
    voMemoprator.Free = cmnMemFree;
    voMemoprator.Set = cmnMemSet;
    voMemoprator.Check = cmnMemCheck;
#if 0
    printf("main: 11111111111111\n");
    fp1=fopen("test1.wav","rb");   
    printf("main: 22222222\n");
#endif
    //wb_vad_init(&(vadstate), &voMemoprator);           //vad初始化   
    printf("main: 333333333333\n");
#if 0
    while(!feof(fp1))   
#endif
    PackageNO = (int*)(&(buf[512]));
    while(1)
    {      
#if 0
        frame++;   
        for(i=0;i<FRAME_LEN;i++)     //读取语音文件   
        {      
            indata[i]=0;   
            temp=0;   
            fread(&temp,2,1,fp1);   
            indatax[i] = temp;   
            if(indata[i]>65535/2)   
            indata[i]=indata[i]-65536;   
        }   
#endif
        //printf("start while\n");
        status = read(fd, buf, sizeof(buf)); /* record some sound */
        //printf("111111111111111111\n");
        if (status != sizeof(buf))
          perror("read wrong number of bytes");

        (*PackageNO) = tmpPackageNO++;
        status = sendto(fdsocket, buf, sizeof(buf), 0, (struct sockaddr*)&dest_addr, sizeof(struct sockaddr));    
        if(status == -1)
        {
            perror("sendto(数据发送)");
        }

        //printf("2222222222222222222\n");
        //vad=wb_vad(vadstate, (short*)buf);    //进行vad检测   
        //printf("%d,",vad);  
  
        //printf("NO=%d,tmpPackageNO=%d\n", *PackageNO, tmpPackageNO); 
        printf("tmpPackageNO=%d\n", tmpPackageNO); 
    }   
    printf("ok!");   
    fcloseall();   
    getchar();  

    return 0; 
}   
