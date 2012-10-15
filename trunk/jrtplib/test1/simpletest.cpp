#include "rtpsession.h"  
#include "rtpsessionparams.h"  
#include "rtpipv4address.h"  
#include "rtppacket.h"  
#include "rtpudpv4transmitter.h"  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <unistd.h>  
#include <stdio.h>  
#include <sys/soundcard.h>  
#include <sys/ioctl.h>  
#include <fcntl.h>  
#include <iostream>  
  
#define PACKSIZE 130  
  
int main(void)  
{  
    int status,i;  
    RTPSession s;  
    RTPSessionParams sessparams;  
    RTPUDPv4TransmissionParams transparams;  
    char blaai[100];  
      
    transparams.SetPortbase(10000);  
    sessparams.SetOwnTimestampUnit(1.0/8000.0);  
    sessparams.SetUsePollThread(true);  
    sessparams.SetMaximumPacketSize(10000);  
    status = s.Create(sessparams,&transparams);  
    s.SetLocalName((const uint8_t *)"Jori Liesenborgs",16);  
    s.SetLocalEMail((const uint8_t *)"jori@lumumba.luc.ac.be",20);  
    s.SetLocalNote((const uint8_t *)"Blaai",5);  
    s.SetNameInterval(3);  
    s.SetEMailInterval(5);  
    s.SetNoteInterval(2);  
  
    status = s.AddDestination(RTPIPv4Address(ntohl(inet_addr("127.0.0.1")),5000));  
    status = s.AddDestination(RTPIPv4Address(ntohl(inet_addr("127.0.0.1")),7000));  
  
    int snd = open("/dev/dsp",O_RDWR);  
    int val;  
  
    val = 0;  
    status = ioctl(snd,SNDCTL_DSP_STEREO,&val);  
    val = 8;  
    status = ioctl(snd,SNDCTL_DSP_SAMPLESIZE,&val);  
    val = 8000;  
    status = ioctl(snd,SNDCTL_DSP_SPEED,&val);  
    val = 7 | (128<<16);  
    ioctl(snd,SNDCTL_DSP_SETFRAGMENT,&val);  
      
    i = 0;  
    while (i++ < 40000)  
    {  
        if (i == 1000)  
        {  
            std::cout <<"Disabling note" << std::endl;  
            s.SetNoteInterval(0);  
        }  
        uint8_t data[PACKSIZE];   
        RTPTime t1 = RTPTime::CurrentTime();  
        status = read(snd,data,PACKSIZE);  
        RTPTime t2 = RTPTime::CurrentTime();  
        t2 -= t1;  
        printf("%d.%06d\n",t2.GetSeconds(),t2.GetMicroSeconds());  
        status = s.SendPacket(data,PACKSIZE,1,false,PACKSIZE);  
    }  
      
    close(snd);  
    printf("Destroying...\n");  
    s.BYEDestroy(RTPTime(10,0),(const uint8_t *)"Leaving session",16);  
    return 0;  
}  
