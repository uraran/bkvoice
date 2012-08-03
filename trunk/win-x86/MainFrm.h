//---------------------------------------------------------------------------

#ifndef MainFrmH
#define MainFrmH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "config.h"

typedef struct tagAudioBuf AUDIOBUF, *pAUDIOBUF;

//音频采集播放结构
struct tagAudioBuf
{
	//char valid;
	char recordvalid;//录音有效
	pAUDIOBUF pNext;
	int count;//有效数据数量
	//short data[SIZE_AUDIO_FRAME/2];
        signed char *data;
#if SPEEX_ENABLED	
	char speexencodevalid;//speex编码后的数据是否还有效
	char speexdecodevalid;//speex编码后的数据是否还有效
	char speexdata[SIZE_AUDIO_FRAME];//speex编码后数据
	short datadecode[SIZE_AUDIO_FRAME];//speex解码后数据
#endif
};



//---------------------------------------------------------------------------
class TMainForm : public TForm
{
__published:	// IDE-managed Components
        TEdit *edtToAddr;
        TLabel *Label1;
        TButton *Button1;
        TButton *Button2;
        void __fastcall Button1Click(TObject *Sender);
        void __fastcall Button2Click(TObject *Sender);
private:	// User declarations
        //void init_audio_buffer();
        HANDLE hRecord, hPlay, hUDPSend;
	HANDLE eventRecord, eventPlay, eventUDPSend;
	DWORD threadRecord, threadPlay, threadUDPSend;

        static DWORD WINAPI voice_record_thread_runner(LPVOID lpParam);
        static DWORD WINAPI voice_udpsend_thread_runner(LPVOID lpParam);
        void init_audio_buffer();//初始化音频缓冲区
public:		// User declarations
        __fastcall TMainForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
