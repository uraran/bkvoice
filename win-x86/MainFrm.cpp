//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MainFrm.h"

#include "config.h"
#include "record.h"
#include <stdio.h>

#include   <winsock.h>
#include "play.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TMainForm *MainForm;

AUDIOBUF buffers[BUFCOUNT];
pAUDIOBUF pHeaderPut;//生产者链表头
pAUDIOBUF pHeaderGet;//消费者链表头
        
//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
        : TForm(Owner)
{
}

#define RECORD_FILE_ENABLED   1
#define MAX_SEM_COUNT 10
HANDLE ghSemaphore;//信号量
//声音采集线程
DWORD WINAPI TMainForm::voice_record_thread_runner(LPVOID lpParam)
{
	HANDLE eventRecord = (HANDLE)(lpParam);
	MSG   msg;
	LPWAVEHDR lpHdr;

        int i,frame=0,temp,vad;
        float indata[2056];
		//VadVars *vadstate;

    if (ghSemaphore == NULL) 
    {
        printf("CreateSemaphore error: %d/n", GetLastError());
        return -1;
    }
                    
#if RECORD_FILE_ENABLED
        TMemoryStream * stream = new TMemoryStream();
#endif                
		//wb_vad_init(&(vadstate));			//vad初始化
	if(openMicAndStartRecording(GetCurrentThreadId()) < 0) return -1;

	while(GetMessage(&msg, 0, 0, 0))
	{
		if(WaitForSingleObject(eventRecord, 1)==WAIT_OBJECT_0)
		{
			break;
		}

		switch(msg.message)
                {
			case MM_WIM_DATA:				
				lpHdr = (LPWAVEHDR)msg.lParam;
				waveInUnprepareHeader(getRecordHandler(), lpHdr, sizeof(WAVEHDR));

				if(lpHdr->lpData!=NULL )
				{

                                /*
                                        for(int i=0;i<dwSample/1000*SAMPLINGPERIOD*wChannels;i++)
                                        {
                                                indata[i] = (float)(((short*)(lpHdr->lpData))[i]);
                                        }
                                */
                                        /*
                                        if(wb_vad(vadstate,indata) == 1)
                                        {
                                        //memcpy(&(pHeaderPut->data[0]), (short*)(lpHdr->lpData), dwSample/1000*SAMPLINGPERIOD*2*wChannels);

                                        }
                                        */

#if RECORD_FILE_ENABLED
                                        stream->Write((short*)(lpHdr->lpData), dwSample/1000*SAMPLINGPERIOD*2*wChannels);
#endif
                                        memcpy(&(pHeaderPut->data[0]), (short*)(lpHdr->lpData), dwSample/1000*SAMPLINGPERIOD*2*wChannels);
                                        pHeaderPut->recordvalid = TRUE;
                                        pHeaderPut = pHeaderPut->pNext;

                                        if (!ReleaseSemaphore( 
                                            ghSemaphore,  // handle to semaphore - hSemaphore是要增加的信号量句柄
                                            1,            // increase count by one - lReleaseCount是增加的计数
                                            NULL) )       // not interested in previous count - lpPreviousCount是增加前的数值返回
                                        {
                                            printf("ReleaseSemaphore error: %d/n", GetLastError());
                                        }
					
				}

				waveInPrepareHeader(getRecordHandler(),lpHdr, sizeof(WAVEHDR));
				waveInAddBuffer(getRecordHandler(), lpHdr, sizeof(WAVEHDR));
				break;
			default:
				break;
		}
	}
#if RECORD_FILE_ENABLED
    stream->SaveToFile("a.pcm");
    stream->Free();
#endif

    return 0;
}

//声音发送线程
DWORD WINAPI TMainForm::voice_udpsend_thread_runner(LPVOID lpParam)
{
    int  result;
    SOCKET      m_Socket;
    unsigned long nAddr;
    struct sockaddr_in To;
    WSADATA     wsaData;
    WORD wVersionRequested;
    wVersionRequested = MAKEWORD(1,1);

    if((result = WSAStartup(wVersionRequested,&wsaData))!=0)
    {
   //      Application->MessageBoxA("Socket Initial Error","Error",MB_OK);
         WSACleanup();
         MessageBox(NULL,"Wrong     WinSock     Version","Error",MB_OK);  
         return -1;
    }

    m_Socket = socket(AF_INET,SOCK_DGRAM,0);
    if(m_Socket == INVALID_SOCKET)
    {
  //      Application->MessageBoxA("Socket Open failed","Error",MB_OK);
        WSACleanup();
        MessageBox(NULL,"Wrong     WinSock     Version","Error",MB_OK);

        return -1;
    }

#define LocalPort 8302
    SOCKADDR_IN sockaddr;
    memset(&sockaddr,0,sizeof(sockaddr));
    /* 设置端口号     */
    sockaddr.sin_port=htons(LocalPort);
    sockaddr.sin_family=AF_INET;
    sockaddr.sin_addr.S_un.S_addr=htonl(INADDR_ANY);

/*
    int  nZero=0;
    int SndBufLen=1024*64;   //128K
    int RcvBufLen=1024*64;   //128K
    //int  iLen;
    //iLen=sizeof(nZero);           //  SO_SNDBUF
    nZero=SndBufLen;       //128K
    result=setsockopt(m_Socket,SOL_SOCKET,SO_SNDBUF,(char*)&nZero,sizeof((char*)&nZero));
    nZero=RcvBufLen;       //128K
    result=setsockopt(m_Socket,SOL_SOCKET,SO_RCVBUF,(char*)&nZero,sizeof((char*)&nZero));
*/
   nAddr=inet_addr(MainForm->edtToAddr->Text.c_str());

   To.sin_family=AF_INET;
#define RemotePort 8302
   To.sin_port=htons(RemotePort);
   To.sin_addr.S_un.S_addr=(int)nAddr;
   
   while(1)
   {

        // Try to enter the semaphore gate.
        DWORD dwWaitResult = WaitForSingleObject( 
            ghSemaphore,   // handle to semaphore
            100L);           // zero-second time-out interval

        if(dwWaitResult == WAIT_OBJECT_0)
        {
            //sendto(m_Socket,"dddddddd", 5, 0,(struct sockaddr*)&To,sizeof(struct sockaddr));
            if(pHeaderGet->recordvalid)
            {
                    signed short * precdata = (signed short*)(&(pHeaderGet->data[0]));
                    int nLength = dwSample/1000*SAMPLINGPERIOD*2*wChannels;


                    //if(vad)
                    {
                            sendto(m_Socket, &(pHeaderGet->data[0]), nLength, 0,(struct sockaddr*)&To,sizeof(struct sockaddr));
                    }
                    pHeaderGet->recordvalid = FALSE;
                    pHeaderGet = pHeaderGet->pNext;
            }
        }
   }
}

//声音发送线程
DWORD WINAPI TMainForm::voice_udprecv_thread_runner(LPVOID lpParam)
{
    int  result;
    SOCKET      m_Socket;
    unsigned long nAddr;
    struct sockaddr_in serveraddr;
    WSADATA     wsaData;
    WORD wVersionRequested;
    wVersionRequested = MAKEWORD(1,1);
    TFileStream *fs = new TFileStream("recv.pcm", fmOpenWrite|fmCreate);
    if((result = WSAStartup(wVersionRequested,&wsaData))!=0)
    {
   //      Application->MessageBoxA("Socket Initial Error","Error",MB_OK);
         WSACleanup();
         MessageBox(NULL,"Wrong     WinSock     Version","Error", MB_OK);  
         return -1;
    }

    m_Socket = socket(AF_INET,SOCK_DGRAM,0);
    if(m_Socket == INVALID_SOCKET)
    {
  //      Application->MessageBoxA("Socket Open failed","Error",MB_OK);
        WSACleanup();
        MessageBox(NULL,"Wrong     WinSock     Version","Error",MB_OK);

        return -1;
    }

    serveraddr.sin_family=AF_INET;
    #define RemotePort 8302
    serveraddr.sin_port=htons(RemotePort);
    serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

    if(bind(m_Socket,(struct sockaddr*)&serveraddr,sizeof(serveraddr))<0)
    {
        printf("bind() ???????.\r\n");
        //close(sock);
    }

   while(1)
   {
        //sendto(m_Socket,"dddddddd", 5, 0,(struct sockaddr*)&To,sizeof(struct sockaddr));
        //if(pHeaderGet->recvvalid == FALSE)
        {
                signed short * recvdata = (signed short*)(&(pHeaderGet->data[0]));
                int nLength = dwSample/1000*SAMPLINGPERIOD*2*wChannels;
                int dwSenderSize =sizeof(serveraddr);

                //if(vad)
                {
                        int recvlength = recvfrom(m_Socket, &(pHeaderGet->data[0]), nLength, 0, (struct sockaddr*)&serveraddr, &dwSenderSize);
                        fs->Write(&(pHeaderGet->data[0]), 512);
                        //fs->
                        printf("%d\n", recvlength);
                }
                pHeaderGet->recvvalid = TRUE;
                pHeaderGet = pHeaderGet->pNext;

                if (!ReleaseSemaphore( 
                    ghSemaphore,  // handle to semaphore - hSemaphore是要增加的信号量句柄
                    1,            // increase count by one - lReleaseCount是增加的计数
                    NULL) )       // not interested in previous count - lpPreviousCount是增加前的数值返回
                {
                    printf("ReleaseSemaphore error: %d/n", GetLastError());
                }
        }
   }
}

//语音播放线程
DWORD WINAPI TMainForm::voice_play_thread_runner(LPVOID   lpParam)   
{
	HANDLE eventPlay = (HANDLE)(lpParam);

	if( 0 != startPlaying(GetCurrentThreadId() )  )
	{
		printf("Start Playing Failed!\n");
		return -1;
	}

	while(1)   
	{

        // Try to enter the semaphore gate.
        DWORD dwWaitResult = WaitForSingleObject(
            ghSemaphore,   // handle to semaphore
            5L);           // zero-second time-out interval

        if(dwWaitResult == WAIT_OBJECT_0)
        {

		//if(pHeaderGet->recvvalid == TRUE)
		{

			if( 0 != playWavData((char*)&(pHeaderGet->data[0]), dwSample/1000*SAMPLINGPERIOD*2*wChannels))

			{
				printf("Playing Wave Data Failed!\n");
			}
			pHeaderGet->recvvalid = FALSE;
			pHeaderGet = pHeaderGet->pNext;			
		}
        }

	}   
	waveOutReset(g_playHandler);
	waveOutClose(g_playHandler);
	return   0;   
}

void TMainForm::init_audio_buffer()
{
	int i;

	for(i=0;i<BUFCOUNT-1;i++)
	{
		buffers[i].pNext = &(buffers[i+1]);
                buffers[i].data = (char*)malloc(dwSample/1000*SAMPLINGPERIOD*2*wChannels);                
		//buffers[i].valid = FALSE;
#if SPEEX_ENABLED
		buffers[i].recordvalid = FALSE;
		buffers[i].speexencodevalid = FALSE;
		buffers[i].speexdecodevalid = FALSE;
#endif
	}
        buffers[BUFCOUNT-1].data = (char*)malloc(dwSample/1000*SAMPLINGPERIOD*2*wChannels);
	buffers[BUFCOUNT-1].pNext = &(buffers[0]);
	//buffers[BUFCOUNT-1].valid = FALSE;
#if SPEEX_ENABLED
	buffers[BUFCOUNT-1].recordvalid = FALSE;
	buffers[BUFCOUNT-1].speexencodevalid = FALSE;
	buffers[BUFCOUNT-1].speexdecodevalid = FALSE;
#endif
	pHeaderPut = &(buffers[0]);
	pHeaderGet = &(buffers[0]);

#if SPEEX_ENABLED
	pHeaderSpeexEncode = &(buffers[0]);
	pHeaderSpeexDecode = &(buffers[0]);
#endif
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::Button1Click(TObject *Sender)
{
    // Create a semaphore with initial and max counts of MAX_SEM_COUNT

    ghSemaphore = CreateSemaphore( 
        NULL,           // default security attributes - lpSemaphoreAttributes是信号量的安全属性
        MAX_SEM_COUNT,  // initial count - lInitialCount是初始化的信号量
        MAX_SEM_COUNT,  // maximum count - lMaximumCount是允许信号量增加到最大值
        NULL);          // unnamed semaphore - lpName是信号量的名称

        
    eventRecord = CreateEvent(NULL,   TRUE,   FALSE,   NULL);
    eventPlay   = CreateEvent(NULL,   TRUE,   FALSE,   NULL);

    dwSample = 16000;
    wChannels = 1;

    init_audio_buffer();

    hRecord = CreateThread((LPSECURITY_ATTRIBUTES)NULL, 0,
          (LPTHREAD_START_ROUTINE)voice_record_thread_runner,
          (LPVOID)eventRecord,0, &threadRecord);

    hUDPSend = CreateThread((LPSECURITY_ATTRIBUTES)NULL, 0,
          (LPTHREAD_START_ROUTINE)voice_udpsend_thread_runner,
          (LPVOID)eventUDPSend,0, &threadUDPSend); 
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::Button2Click(TObject *Sender)
{
                //发送事件通知录音线程与播放线程结束
                SetEvent(eventRecord);

                //等待录音线程结束
                WaitForSingleObject(hRecord, INFINITE);

                if(hRecord != NULL)
                {
                        //释放录音线程资源
                        //CloseHandle(hRecord);
                        //hRecord = NULL;
                }

}
//---------------------------------------------------------------------------


void __fastcall TMainForm::Button3Click(TObject *Sender)
{
//    eventRecord = CreateEvent(NULL,   TRUE,   FALSE,   NULL);
//    eventPlay   = CreateEvent(NULL,   TRUE,   FALSE,   NULL);

    // Create a semaphore with initial and max counts of MAX_SEM_COUNT

    ghSemaphore = CreateSemaphore( 
        NULL,           // default security attributes - lpSemaphoreAttributes是信号量的安全属性
        MAX_SEM_COUNT,  // initial count - lInitialCount是初始化的信号量
        MAX_SEM_COUNT,  // maximum count - lMaximumCount是允许信号量增加到最大值
        NULL);          // unnamed semaphore - lpName是信号量的名称

        
    dwSample = 16000;
    wChannels = 1;

    init_audio_buffer();

    hPlay = CreateThread((LPSECURITY_ATTRIBUTES)NULL, 0,
          (LPTHREAD_START_ROUTINE)voice_play_thread_runner,
          (LPVOID)eventPlay, 0, &threadPlay);

    hUDPRecv = CreateThread((LPSECURITY_ATTRIBUTES)NULL, 0,
          (LPTHREAD_START_ROUTINE)voice_udprecv_thread_runner,
          (LPVOID)eventUDPRecv,0, &threadUDPRecv);
}
