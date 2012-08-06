#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>
#include "config.h"


// =====Definitions =========

#define M_CARD 0  // 打开第一个声卡,  WAVE_MAPPER 指所有声音


// =====Global Variables ====

WAVEFORMATEX m_waveFormat;
HWAVEIN g_recordHandler;
LPWAVEHDR rechead[MAXRECBUFFER];

DWORD dwSample;
WORD  wChannels;
int   capture_size;
// =====Functions============
WAVEFORMATEX getDefaultFormat(void)
{
	WAVEFORMATEX m_WaveFormatEx;
	memset(&m_WaveFormatEx, 0x00, sizeof(m_WaveFormatEx));
	m_WaveFormatEx.wFormatTag		=	WAVE_FORMAT_PCM;
	m_WaveFormatEx.nChannels		=	wChannels;
	m_WaveFormatEx.wBitsPerSample	        =	16;
	m_WaveFormatEx.cbSize			=	0;
	m_WaveFormatEx.nSamplesPerSec	        =	dwSample;  //采样频率
	m_WaveFormatEx.nBlockAlign		=	m_WaveFormatEx.wBitsPerSample/8;
	m_WaveFormatEx.nAvgBytesPerSec	        =	m_WaveFormatEx.nBlockAlign * m_WaveFormatEx.nSamplesPerSec;

	return m_WaveFormatEx;
}

extern int size_audio_frame;

LPWAVEHDR  CreateWaveHeader(void)
{
        int capture_size = 0;
	LPWAVEHDR lpHdr = (LPWAVEHDR)malloc(sizeof(WAVEHDR));
	char* lpByte = NULL;

	if(lpHdr==NULL)	return NULL;
	memset(lpHdr, 0, sizeof(WAVEHDR));

#if SPEEX_ENABLED
	lpByte = (char*)malloc(size_audio_frame*2);
#else
        capture_size = dwSample/1000*SAMPLINGPERIOD*2*wChannels;
	lpByte = (char*)malloc(capture_size);
#endif
	if(lpByte==NULL)	return NULL;

	lpHdr->lpData =  lpByte;
#if SPEEX_ENABLED
	lpHdr->dwBufferLength =size_audio_frame*2;
#else
	lpHdr->dwBufferLength =capture_size;
#endif
	return lpHdr;
}

void DeleteWaveHeader(void)
{
	int i;

	for(i=0;i<MAXRECBUFFER;i++)
	{
		if(rechead[i])
		{
			if(rechead[i]->lpData )
			{
				free(rechead[i]->lpData);
				rechead[i]->lpData = NULL;
			}
			free(rechead[i]);
			rechead[i] = NULL;
		}
	}
}

int openMicAndStartRecording(DWORD threadId)
{
	int i;

	WAVEFORMATEX m_WaveFormatEx = getDefaultFormat();

	if( MMSYSERR_NOERROR != waveInOpen(&g_recordHandler,WAVE_MAPPER,&m_WaveFormatEx,threadId,0,CALLBACK_THREAD)){
		printf("ER1!\n");
		return -1;
	}

	for(i=0; i<MAXRECBUFFER; i++){
		rechead[i]=CreateWaveHeader();
		if(MMSYSERR_NOERROR != waveInPrepareHeader(g_recordHandler,rechead[i], sizeof(WAVEHDR)) ){
			printf("ER2!   i=%d\n", i);
			return -1;
		}

		if(MMSYSERR_NOERROR != waveInAddBuffer(g_recordHandler, rechead[i], sizeof(WAVEHDR))){
			printf("ER3!\n");
			return -1;
		}

		if(MMSYSERR_NOERROR != waveInStart(g_recordHandler)){
			printf("ER4!\n");	
			return -1;
		}
	}

	printf("Start recording OK!\n");
	return 0;
}

HWAVEIN getRecordHandler(void)
{
	return g_recordHandler;
}