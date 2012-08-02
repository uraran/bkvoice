
#include "stdio.h"
#include "wb_vad.h"
void main1()
{	
		int i,frame=0,temp,vad; 
		float indata[FRAME_LEN];
		VadVars *vadstate;					
		FILE *fp1;
		fp1=fopen("inls1.wav","rb");
		wb_vad_init(&(vadstate));			//vad初始化
		while(!feof(fp1))
		{	
			frame++;
			for(i=0;i<FRAME_LEN;i++)		//读取语音文件
			{	
				indata[i]=0;
				temp=0;
				fread(&temp,2,1,fp1);
				indata[i]=(float)temp;
				if(indata[i]>65535/2)
				indata[i]=indata[i]-65536;
			}
			vad=wb_vad(vadstate,indata);	//进行vad检测
			printf("%d \n",vad);
		}
		printf("ok!");
		fcloseall();
		getchar();
}
