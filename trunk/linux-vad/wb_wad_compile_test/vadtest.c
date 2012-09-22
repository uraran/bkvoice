#include "stdio.h"   
#include "wb_vad.h"   
#include "cmnMemory.h"

void main()   
{      
        int i,frame=0,temp,vad;    
        //float indata[FRAME_LEN];   
        short indata[FRAME_LEN];   
        VadVars *vadstate;                     
        FILE *fp1;   
	    VO_MEM_OPERATOR voMemoprator;

		voMemoprator.Alloc = cmnMemAlloc;
		voMemoprator.Copy = cmnMemCopy;
		voMemoprator.Free = cmnMemFree;
		voMemoprator.Set = cmnMemSet;
		voMemoprator.Check = cmnMemCheck;

        printf("main: 11111111111111\n");
        fp1=fopen("test1.wav","rb");   
        printf("main: 22222222\n");
        wb_vad_init(&(vadstate), &voMemoprator);           //vad初始化   
        printf("main: 333333333333\n");
        while(!feof(fp1))   
        {      
            frame++;   
            for(i=0;i<FRAME_LEN;i++)     //读取语音文件   
            {      
                indata[i]=0;   
                temp=0;   
                fread(&temp,2,1,fp1);   
                indata[i] = temp;   
                if(indata[i]>65535/2)   
                indata[i]=indata[i]-65536;   
            }   
            vad=wb_vad(vadstate,indata);    //进行vad检测   
            printf("%d,",vad);   
        }   
        printf("ok!");   
        fcloseall();   
        getchar();   
}   
