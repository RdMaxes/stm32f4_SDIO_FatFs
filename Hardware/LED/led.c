#include <stm32f4xx.h>
#include "led.h"


//Delay for a while
//time: delay time
static void LED_delay(int32_t time)
{
	while(time--);
}
 
//LED configuration 
void LED_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	//LED GREEN
	RCC_AHB1PeriphClockCmd(LED_GREEN_CLK,ENABLE);
	GPIO_InitStruct.GPIO_Pin = LED_GREEN_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_GREEN_PORT, &GPIO_InitStruct);

	//LED ORANGE
	RCC_AHB1PeriphClockCmd(LED_ORANGE_CLK,ENABLE);
	GPIO_InitStruct.GPIO_Pin = LED_ORANGE_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_ORANGE_PORT, &GPIO_InitStruct);

	//LED RED
	RCC_AHB1PeriphClockCmd(LED_RED_CLK,ENABLE);
	GPIO_InitStruct.GPIO_Pin = LED_RED_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_RED_PORT, &GPIO_InitStruct);

	//LED BLUE
	RCC_AHB1PeriphClockCmd(LED_BLUE_CLK,ENABLE);
	GPIO_InitStruct.GPIO_Pin = LED_BLUE_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_BLUE_PORT, &GPIO_InitStruct);		
}

//make led on/off loop
void LED_loop(void)
{	
	LED_GREEN_ON();LED_delay(1000000);LED_GREEN_OFF();LED_delay(1000000);
	LED_BLUE_ON();LED_delay(1000000);LED_BLUE_OFF();LED_delay(1000000);
	LED_RED_ON();LED_delay(1000000);LED_RED_OFF();LED_delay(1000000);
	LED_ORANGE_ON();LED_delay(1000000);LED_ORANGE_OFF();LED_delay(1000000);
}
