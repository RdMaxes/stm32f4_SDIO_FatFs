//Includes 
#include <stm32f4xx.h>
#include "led.h"
#include "usart2.h"
#include "myprintf.h"
#include "sdio_sdcard.h"
#include "diskio.h"
#include "ff.h"

//Delay for a while
//time: delay time
static void delay(int32_t time)
{
	while(time--);
}

int main(void)
{	
	uint64_t cardsize = 0;

	LED_Init();
	Usart2_Init(115200);
	Myprintf_Init(0x00,myputc);
	while(SD_Init()!=SD_OK)
		{
			LED_loop();
		}
	cardsize = SDCardInfo.CardCapacity;
	while(1) 
	{
		delay(8000000);
		LED_GREEN_ON();
		delay(8000000);
		LED_GREEN_OFF();		
	}

	return 0;
}
