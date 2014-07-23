//Includes 
#include <stm32f4xx.h>
#include "led.h"
#include "usart2.h"
#include "myprintf.h"
#include "sdio_sdcard.h"
#include "diskio.h"
#include "ff.h"

//Global Variables for FatFs
FATFS fs;			//work area (file system object) for logical drive
FIL fsrc, fdst;	//data stream object
FRESULT res;		//FatFs function common result
UINT br, bw;		//file R/W counter
DIR dir;			//directory 
FILINFO FileInfo;	//file information 

const char rootdir[] = "0:/"; //root directory
const char opnfile[] = "firmware.bin"; //file name 

//Delay for a while
//time: delay time
static void delay(int32_t time)
{
	while(time--);
}

int main(void)
{	
	LED_Init();
	Usart2_Init(115200);
	Myprintf_Init(0x00,myputc);
	while(SD_Init()!=SD_OK)
		{
			LED_loop();
		}
	//print SD card size
	my_printf("SD Card Size: %d Bytes\r\n",(uint32_t)SDCardInfo.CardCapacity);
	//mount logical disk 0
		f_mount(&fs,0,1);
		res = f_opendir(&dir,rootdir); //open assigned directory
		if(res==FR_OK)
		{
			res = f_open(&fsrc,opnfile,FA_READ); //open assigned file
		}

		f_mount(0x00,0,1);	
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
