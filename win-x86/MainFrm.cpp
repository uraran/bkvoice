//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MainFrm.h"

#include "config.h"
#include "record.h"
#include <stdio.h>

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TMainForm *MainForm;
//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
        : TForm(Owner)
{
}

#define RECORD_FILE_ENABLED   1
//声音采集线程
DWORD WINAPI TMainForm::voice_record_thread_runner(LPVOID lpParam)
{
	HANDLE eventRecord = (HANDLE)(lpParam);
	MSG   msg;
	LPWAVEHDR lpHdr;


        int i,frame=0,temp,vad; 
        float indata[2056];
		//VadVars *vadstate;
#if RECORD_FILE_ENABLED
        TMemoryStream * stream = new TMemoryStream();
#endif                
		//wb_vad_init(&(vadstate));			//vad初始化


        
	if(openMicAndStartRecording(GetCurrentThreadId()) < 0) return -1;

	while(GetMessage(&msg, 0, 0, 0))
	{
		if(WaitForSingleObject(eventRecord, 1)==WAIT_OBJECT_0)
		{
                        //printf("");                
			break;
		}

		switch(msg.message){
			case MM_WIM_DATA:				
				lpHdr = (LPWAVEHDR)msg.lParam;
				waveInUnprepareHeader(getRecordHandler(), lpHdr, sizeof(WAVEHDR));

				if(lpHdr->lpData!=NULL )
				{

					{

                                                for(int i=0;i<dwSample/1000*SAMPLINGPERIOD*wChannels;i++)
                                                {
                                                        indata[i] = (float)(((short*)(lpHdr->lpData))[i]);
                                                }

                                                /*
                                                if(wb_vad(vadstate,indata) == 1)
                                                {
						//memcpy(&(pHeaderPut->data[0]), (short*)(lpHdr->lpData), dwSample/1000*SAMPLINGPERIOD*2*wChannels);

                                                }
                                                */

#if RECORD_FILE_ENABLED
                                                stream->Write((short*)(lpHdr->lpData), dwSample/1000*SAMPLINGPERIOD*2*wChannels);
#endif


						//pHeaderPut->recordvalid = TRUE;
                                                
						//pHeaderPut = pHeaderPut->pNext;
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
                eventRecord = CreateEvent(NULL,   TRUE,   FALSE,   NULL); 	
                eventPlay   = CreateEvent(NULL,   TRUE,   FALSE,   NULL);

                dwSample = 16000;
                wChannels = 1;

                init_audio_buffer();

                hRecord = CreateThread((LPSECURITY_ATTRIBUTES)NULL, 0,
                        (LPTHREAD_START_ROUTINE)voice_record_thread_runner,
                        (LPVOID)eventRecord,0, &threadRecord);        
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

