//Includes 
#include <stm32f4xx.h>
#include "led.h"
#include "usart2.h"
#include "myprintf.h"
#include "sdio_sdcard.h"
#include "diskio.h"
#include "ff.h"

//definitions for FatFs
#define READ_BUF_LEN 1024 //read out buffer length

//Global Variables for FatFs
FATFS fs;			//work area (file system object) for logical drive
FIL fsrc, fdst;	//data stream object
FRESULT res;		//FatFs function common result
UINT br, bw;		//file R/W counter
DIR dir;			//directory 
FILINFO FileInfo;	//file information 

static const char rootdir[] = "0:/"; //root directory of driver 0
static const char opnfile[] = "test.txt"; //file name 
uint8_t rd_buf[READ_BUF_LEN] = {0};	//read out buffer

//print string via usart2
void print_string(uint8_t *buf)
{
	uint32_t i=0;
	uint8_t* pbuf = buf;
	while(*buf!='\0')
	{
		i++;
		buf++;
	}
	Usart2_DMA_Send(pbuf,i);
}
//Delay for a while
//time: delay time
static void delay(int32_t time)
{
	while(time--);
}

int main(void)
{	
	uint32_t stream_cnt = 0; //data stream counter

	LED_Init();
	Usart2_Init(115200);
	Myprintf_Init(0x00,myputc);
	while(SD_Init()!=SD_OK)
		{
			LED_loop();
		}
	//print SD card size
	stream_cnt = 	SDCardInfo.CardCapacity;
	my_printf("\r\nSD Card Size: %d Bytes\r\n",(uint32_t)SDCardInfo.CardCapacity);
	//mount logical disk 0
		res = f_mount(0,&fs);
		if (res==FR_OK)
		{
			res = f_opendir(&dir,"0:/test"); //open assigned directory
			if(res==FR_OK)
			{
				res = f_open(&fsrc,opnfile,FA_READ); //open assigned file
				if(!res)
				{
					my_printf("Opening file: %s \r\n",opnfile);
					my_printf("File Content:\r\n");
					stream_cnt=0;
					for(;;)
					{
						//clear buffer
						for(stream_cnt=0;stream_cnt<READ_BUF_LEN;stream_cnt++)	rd_buf[stream_cnt] = 0x00;
						//read out data with READ_BUF_LEN length
						res = f_read(&fsrc,rd_buf,READ_BUF_LEN,&br); 
						//print out
						print_string((uint8_t*)rd_buf);
						//if error occur or reach end of file, break
						if(res||(br<READ_BUF_LEN))	break;
					}
				}
				else	my_printf("Fatal, no such file.\r\n");
			}
			else	my_printf("Fatal, no such directory.\r\n");
		}
		else	my_printf("Fatal, driver mounting error.\r\n");

		f_mount(0,0x00);	
	//unmount logical disk 0
	while(1) 
	{
		delay(8000000);
		LED_GREEN_ON();
		delay(8000000);
		LED_GREEN_OFF();		
	}

	return 0;
}
