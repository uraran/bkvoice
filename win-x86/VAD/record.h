#ifndef _RECORD_H_
#define _RECORD_H_

#include <mmsystem.h>

extern int openMicAndStartRecording(DWORD);

extern HWAVEIN getRecordHandler(void);

extern WAVEFORMATEX getDefaultFormat(void);

extern void DeleteWaveHeader(void);
#endif