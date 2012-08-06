#ifndef _PLAY_H_
#define _PLAY_H_


extern HWAVEOUT g_playHandler;
int startPlaying(DWORD threadId);
int playWavData(char* pBuff,int bufLen);

#endif