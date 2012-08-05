#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>
#include <memory.h>
#include "play.h"
#include "record.h"

// =====Global Variables ====

HWAVEOUT g_playHandler;

// =====Functions============

int startPlaying(DWORD threadId)
{
	WAVEFORMATEX m_WaveFormatEx = getDefaultFormat();
	MMRESULT mmReturn = 0;
	DWORD volume=0xffffffff;

	mmReturn = waveOutOpen(&g_playHandler, WAVE_MAPPER, &m_WaveFormatEx, threadId, 0, CALLBACK_THREAD);
	if(mmReturn != MMSYSERR_NOERROR)
	{
		printf("startPlaying ec=%d\n", mmReturn);
		return -1;
	}

	waveOutSetVolume(g_playHandler,volume);

	return 0;

}

int playWavData(char* pBuff,int bufLen)
{
	WAVEHDR *lpHdr = (WAVEHDR *)malloc(sizeof(WAVEHDR));
	//char* pNewBuf;
	MMRESULT mmResult = 0;

	if( lpHdr == NULL ) return -1;
/*
	pNewBuf = (char*) malloc(bufLen);
	if(pNewBuf == NULL)
	{
		free(lpHdr);
		return -1;
	}
*/	
	memset(lpHdr, 0, sizeof(WAVEHDR));
	//memcpy(pNewBuf, pBuff, bufLen);

	lpHdr->dwBufferLength = bufLen;
	//lpHdr->lpData = pNewBuf;
	lpHdr->lpData = pBuff;

	mmResult = waveOutPrepareHeader(g_playHandler, lpHdr, sizeof(WAVEHDR));
	if(mmResult)
	{
		printf("Fail-1, mmResult=%d\n", mmResult);
		free(lpHdr);
		return -1;
	}
	
	mmResult = waveOutWrite(g_playHandler, lpHdr, sizeof(WAVEHDR));
	if(mmResult)
	{
		printf("Fail-2\n");
		free(lpHdr);
		return -1;
	}

	return 0;
}


