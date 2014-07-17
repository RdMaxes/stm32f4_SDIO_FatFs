#include <stm32f4xx.h>
#include "usart2.h"

//Private Function Prototype
static uint32_t Usart2_RxByte(uint8_t *key);

//USART2 get a byte DR
//key: pointer to store data
//return: 
//      0:fail
//      1:success
static uint32_t Usart2_RxByte(uint8_t *key)
{

  if ( USART_GetFlagStatus(USART2, USART_FLAG_RXNE) != RESET)
  {
    *key = (uint8_t)USART2->DR;
    return 1;
  }
  else
  {
    return 0;
  }
}

//USART2 get a byte from HyperTerminal
//return: Rx byte
uint8_t Usart2_GetByte(void)
{
  uint8_t key = 0;

  /* Waiting for user input */
  while (1)
  {
    if (Usart2_RxByte((uint8_t*)&key)) break;
    
  }
  return key;

}
 
//DMA1 for usart2 configuration
static void DMA1_Usart2_Config(void)  
{  
    uint8_t dummy_data = 0;

    DMA_InitTypeDef DMA_InitStructure;
    //enable DMA1 clock
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    //reset 
    DMA_DeInit(DMA1_Stream6); 
    //DMA configuration  
    DMA_InitStructure.DMA_Channel = DMA_Channel_4; 
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR; 
    DMA_InitStructure.DMA_BufferSize = 1;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&dummy_data;      
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;     
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;   
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;   
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;  
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte; 
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal ;     
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;     
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;  
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single; 
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold =DMA_FIFOThreshold_Full; 
                
    DMA_Init(DMA1_Stream6, &DMA_InitStructure);         
    DMA_Cmd(DMA1_Stream6,ENABLE); 
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);  
}

//usart2 send data via DMA
//@ int8_t *buf: data buffer ready to send
//@ int16_t len: data length
void Usart2_DMA_Send(uint8_t *buf, uint16_t len)
{  
    DMA_InitTypeDef DMA_InitStructure;
    //wait unitl last package is sent
    while(DMA_GetFlagStatus(DMA1_Stream6, DMA_FLAG_TCIF6)==RESET);
    DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_TCIF6);
    //DMA configuration  
    DMA_InitStructure.DMA_Channel = DMA_Channel_4;   
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR; 
    DMA_InitStructure.DMA_BufferSize = len;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)buf;      
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;     
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;   
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;   
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;  
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte; 
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal ;     
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;     
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;  
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single; 
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold =DMA_FIFOThreshold_HalfFull; 
                
    DMA_Init(DMA1_Stream6, &DMA_InitStructure);         
    DMA_Cmd(DMA1_Stream6,ENABLE); 
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE); 
}

//usart2 configuration
//default setting is 8,n,1
//@ int baudrate: the desired baudrate
void Usart2_Init(int baudrate)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	USART_InitTypeDef USART_InitStruct;
//	NVIC_InitTypeDef NVIC_InitStruct;

	//enable clock for Tx/Rx pins
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	//hook the pin function
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
	//pin configuration
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	//enable usart2's clock
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	//usart2 configuration
	USART_InitStruct.USART_BaudRate = baudrate;
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStruct.USART_Parity = USART_Parity_No;
	USART_InitStruct.USART_StopBits = USART_StopBits_1;
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART2, &USART_InitStruct);
	USART_Cmd(USART2, ENABLE);
	//enable RX interrupt
//	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	 
	//usart2 NVIC cconfiguration
//	NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;
//	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
//	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
//	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
//	NVIC_Init(&NVIC_InitStruct);

	//Enable usart2 DMA Tx
	DMA1_Usart2_Config();	
}
