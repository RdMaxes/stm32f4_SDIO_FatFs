/*
//@brief :STM32F40x_41x SD Card R/W via SDIO interface
//@Author:RdMaxes
//@Data  :2014/07/18
//@Note  :
//		 (*)2014/07/18
//          Original sdio_sdcard.c and sdio_sdcard.h modified from ALIENTEK example.
//          www.openedv.com for ATOM@ALIENTEK online support
*/

#include "sdio_sdcard.h"	  													   
									  
static u8 CardType=SDIO_STD_CAPACITY_SD_CARD_V1_1;	//Type of SD card	
static u32 CSD_Tab[4],CID_Tab[4],RCA=0;				//CSD, CIS and RCA data	
static u8 DeviceMode=SD_DMA_MODE;		   			//Working Mode 	
static u8 StopCondition=0; 							//if sending stop bit flag, for DMA W/R	
volatile SD_Error TransferError=SD_OK;				//transmission error flag, for DMA W/R		    
volatile u8 TransferEnd=0;							//transmission ending flag, for DMA W/R		
SD_CardInfo SDCardInfo;								//SD card information

//Global Variables
//SD R/W Disk needs 4byte alignment
u8 SDIO_DATA_BUFFER[512];						  
 

//Initialize SD card
//Calling major function SD_PowerON() and SD_InitializeCards() ..etc
//return: fatal code
SD_Error SD_Init(void)
{
	SD_Error errorstatus=SD_OK;	   
	NVIC_InitTypeDef NVIC_InitStruct;

	//SDIO pin configuration
	RCC->APB2ENR|=1<<4;  //enable PORTC clock  	   	 
	RCC->APB2ENR|=1<<5;  //enable PORTD clock  	
  	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO,ENABLE);	 //enable SDIO clock
  //	RCC->AHBENR|=1<<10;  //enable SDIO clock  		   	 
 	RCC->AHBENR|=1<<1;   //enable DMA2 clock 	

	GPIOC->CRH&=0XFFF00000; 
	GPIOC->CRH|=0X000BBBBB;	//PC.08~PC.12 Alternate-Push-Pull

	GPIOD->CRL&=0XFFFFF0FF; 
	GPIOD->CRL|=0X00000B00;	//PD.02 Alternate-Push-Pull, PD.07 Pull-Up Input
 	
 	//Deinit SDIO peripheral		   
	SDIO->POWER=0x00000000;
	SDIO->CLKCR=0x00000000;
	SDIO->ARG=0x00000000;
	SDIO->CMD=0x00000000;
	SDIO->DTIMER=0x00000000;
	SDIO->DLEN=0x00000000;
	SDIO->DCTRL=0x00000000;
	SDIO->ICR=0x00C007FF;
	SDIO->MASK=0x00000000;	  
	//SDIO NVIC Configuration
	NVIC_InitStruct.NVIC_IRQChannel = SDIO_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 2;
	NVIC_Init(&NVIC_InitStruct);
	//Power ON Step
   	errorstatus=SD_PowerON();			
 	if(errorstatus==SD_OK)errorstatus=SD_InitializeCards();	//SD card initialization				  
  	if(errorstatus==SD_OK)errorstatus=SD_GetCardInfo(&SDCardInfo);	//get SD card information	
 	if(errorstatus==SD_OK)errorstatus=SD_SelectDeselect((u32)(SDCardInfo.RCA<<16)); //Select SD card
   	if(errorstatus==SD_OK)errorstatus=SD_EnableWideBusOperation(1);	//switch to 4bit data width
  	if((errorstatus==SD_OK)||(SDIO_MULTIMEDIA_CARD==CardType))
	{  		    
		SDIO_Clock_Set(SDIO_TRANSFER_CLK_DIV);	//Set clock frequency		
		errorstatus=SD_SetDeviceMode(SD_DMA_MODE); //Set as DMA mode
 	}
	return errorstatus;		 
}

//Set SDIO clock frequency
//clkdiv: clock division
void SDIO_Clock_Set(u8 clkdiv)
{
  	SDIO->CLKCR&=0XFFFFFF00;
 	SDIO->CLKCR|=clkdiv; 
} 

//Send command via SDIO
//cmdindex: command index, LSB 6 bits are used
//waitrsp: response type
//		   00/10 = No Response
//         01    = Short Response
//         11    = Long Response
void SDIO_Send_Cmd(u8 cmdindex,u8 waitrsp,u32 arg)
{						    
	SDIO->ARG=arg;
	SDIO->CMD&=0XFFFFF800;	//clear index and waitrsp	
	SDIO->CMD|=cmdindex&0X3F;	//set new index
	SDIO->CMD|=waitrsp<<6;	//set new waitrsp
	SDIO->CMD|=0<<8; //no wait
  	SDIO->CMD|=1<<10;	//enable SDIO command	
}

//SDIO send data configuration
//datatimeout: timeout of sending data
//datalen: data length, LSB 25 are used
//blksize: size of block, actual size is 2^blksize bytes
//dir: 0 = controller-->card, 1 = card-->controller
void SDIO_Send_Data_Cfg(u32 datatimeout,u32 datalen,u8 blksize,u8 dir)
{
	SDIO->DTIMER=datatimeout;
  	SDIO->DLEN=datalen&0X1FFFFFF;	
	SDIO->DCTRL&=0xFFFFFF08;		
	SDIO->DCTRL|=blksize<<4;		
	SDIO->DCTRL|=0<<2;				
	SDIO->DCTRL|=(dir&0X01)<<1;		
	SDIO->DCTRL|=1<<0;				
}  

//Power On step of initialization
//Check all the devices on SDIO interface, get the voltage level and setting clock
//return: fatal code, 0 = correct
SD_Error SD_PowerON(void)
{
 	u8 i=0;
	SD_Error errorstatus=SD_OK;
	u32 response=0,count=0,validvoltage=0;
	u32 SDType=SD_STD_CAPACITY;
	//Configure CLKCR register
	SDIO->CLKCR=0;		//clear setting				
	SDIO->CLKCR|=0<<9;	//not power-save mode		
	SDIO->CLKCR|=0<<10;	//disable bypass mode		
	SDIO->CLKCR|=0<<11;	//1 bit data width		
	SDIO->CLKCR|=0<<13;	//SDIOCLK generates SDIOCK on rising edge		
	SDIO->CLKCR|=0<<14;	//disable hardware flow control		
	SDIO_Clock_Set(SDIO_INIT_CLK_DIV);	//set clock frequency
 	SDIO->POWER=0X03;	//power on, enable clock
  	SDIO->CLKCR|=1<<8;	//SDIOCK enabled
   	for(i=0;i<74;i++)
	{
		SDIO_Send_Cmd(SD_CMD_GO_IDLE_STATE,0,0);	//entering IDLE STAGE											  
		errorstatus=CmdError();
		if(errorstatus==SD_OK)break;
 	}
 	if(errorstatus)return errorstatus; //return fatal code(error)
	SDIO_Send_Cmd(SDIO_SEND_IF_COND,1,SD_CHECK_PATTERN);//send CMD8, check card interface															
  	errorstatus=CmdResp7Error();//waiting for R7 response						
 	if(errorstatus==SD_OK)	//get R7 response 								
	{
		CardType=SDIO_STD_CAPACITY_SD_CARD_V2_0;	//SD 2.0		
		SDType=SD_HIGH_CAPACITY;	//R7 response normal
	}else 
	{
		SDIO_Send_Cmd(SD_CMD_APP_CMD,1,0);					  
	   	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
	}
	SDIO_Send_Cmd(SD_CMD_APP_CMD,1,0);	//send CMD55				
	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);	//waiting for R1 response		 	  
	if(errorstatus==SD_OK)	//it's SD2.0 or SD1.1 card, otherwise it's MMC card
	{																  
		
		while((!validvoltage)&&(count<SD_MAX_VOLT_TRIAL))
		{	   										   
			SDIO_Send_Cmd(SD_CMD_APP_CMD,1,0);				 
			errorstatus=CmdResp1Error(SD_CMD_APP_CMD); 	 	  
 			if(errorstatus!=SD_OK)return errorstatus;   	
			SDIO_Send_Cmd(SD_CMD_SD_APP_OP_COND,1,SD_VOLTAGE_WINDOW_SD|SDType);
			errorstatus=CmdResp3Error(); 					
 			if(errorstatus!=SD_OK)return errorstatus;   	
			response=SDIO->RESP1;;			   				
			validvoltage=(((response>>31)==1)?1:0);
			count++;
		}
		if(count>=SD_MAX_VOLT_TRIAL)
		{
			errorstatus=SD_INVALID_VOLTRANGE;
			return errorstatus;
		}	 
		if(response&=SD_HIGH_CAPACITY)
		{
			CardType=SDIO_HIGH_CAPACITY_SD_CARD;
		}
 	}
 	else //MMC card 
	{
		CardType=SDIO_MULTIMEDIA_CARD;	  
		
		while((!validvoltage)&&(count<SD_MAX_VOLT_TRIAL))
		{	   										   				   
			SDIO_Send_Cmd(SD_CMD_SEND_OP_COND,1,SD_VOLTAGE_WINDOW_MMC);	 
			errorstatus=CmdResp3Error(); 					  
 			if(errorstatus!=SD_OK)return errorstatus;   	
			response=SDIO->RESP1;;			   				
			validvoltage=(((response>>31)==1)?1:0);
			count++;
		}
		if(count>=SD_MAX_VOLT_TRIAL)
		{
			errorstatus=SD_INVALID_VOLTRANGE;
			return errorstatus;
		}	 			    
  	}  
  	return(errorstatus);		
}


//Power Off SD card
//return: fatal code, 0 = correct
SD_Error SD_PowerOFF(void)
{
  	SDIO->POWER&=~(3<<0);	//power off
	return SD_OK;		  
}   

//Initialize cards
//return: fatal code
SD_Error SD_InitializeCards(void)
{
 	SD_Error errorstatus=SD_OK;
	u16 rca = 0x01;
 	if((SDIO->POWER&0X03)==0)return SD_REQUEST_NOT_APPLICABLE;	//check power, ensure it's power on
 	if(SDIO_SECURE_DIGITAL_IO_CARD!=CardType)			
	{
		SDIO_Send_Cmd(SD_CMD_ALL_SEND_CID,3,0);			
		errorstatus=CmdResp2Error(); 					 
		if(errorstatus!=SD_OK)return errorstatus;   	    
 		CID_Tab[0]=SDIO->RESP1;
		CID_Tab[1]=SDIO->RESP2;
		CID_Tab[2]=SDIO->RESP3;
		CID_Tab[3]=SDIO->RESP4;
	}
	//Determine card type
	if((SDIO_STD_CAPACITY_SD_CARD_V1_1==CardType)||(SDIO_STD_CAPACITY_SD_CARD_V2_0==CardType)||(SDIO_SECURE_DIGITAL_IO_COMBO_CARD==CardType)||(SDIO_HIGH_CAPACITY_SD_CARD==CardType))//判断卡类型
	{
		SDIO_Send_Cmd(SD_CMD_SET_REL_ADDR,1,0);			
		errorstatus=CmdResp6Error(SD_CMD_SET_REL_ADDR, &rca);
		if(errorstatus!=SD_OK)return errorstatus;   	 
	}   
    if (SDIO_MULTIMEDIA_CARD==CardType)
    {
 		SDIO_Send_Cmd(SD_CMD_SET_REL_ADDR,1,(u32)(rca<<16));	   
		errorstatus=CmdResp2Error(); 					
		if(errorstatus!=SD_OK)return errorstatus;   	
    }
	if (SDIO_SECURE_DIGITAL_IO_CARD!=CardType)	//not SECURE_DIGITAL_IO_CARD		
	{
		RCA = rca;
		SDIO_Send_Cmd(SD_CMD_SEND_CSD,3,(u32)(rca<<16));	   
		errorstatus=CmdResp2Error(); 					
		if(errorstatus!=SD_OK)return errorstatus;   		    
  		CSD_Tab[0]=SDIO->RESP1;
		CSD_Tab[1]=SDIO->RESP2;
		CSD_Tab[2]=SDIO->RESP3;						
		CSD_Tab[3]=SDIO->RESP4;					    
	}
	return SD_OK;	//Succecc initialization
} 

//Get card information
//cardinfo: pointer to store card information
//return: fatal code
SD_Error SD_GetCardInfo(SD_CardInfo *cardinfo)
{
 	SD_Error errorstatus=SD_OK;
	u8 tmp=0;	   
	cardinfo->CardType=(u8)CardType; 				
	cardinfo->RCA=(u16)RCA;							
	tmp=(u8)((CSD_Tab[0]&0xFF000000)>>24);
	cardinfo->SD_csd.CSDStruct=(tmp&0xC0)>>6;		
	cardinfo->SD_csd.SysSpecVersion=(tmp&0x3C)>>2;	
	cardinfo->SD_csd.Reserved1=tmp&0x03;			
	tmp=(u8)((CSD_Tab[0]&0x00FF0000)>>16);			
	cardinfo->SD_csd.TAAC=tmp;				   		
	tmp=(u8)((CSD_Tab[0]&0x0000FF00)>>8);	  		
	cardinfo->SD_csd.NSAC=tmp;		  				
	tmp=(u8)(CSD_Tab[0]&0x000000FF);				
	cardinfo->SD_csd.MaxBusClkFrec=tmp;		  		  
	tmp=(u8)((CSD_Tab[1]&0xFF000000)>>24);			
	cardinfo->SD_csd.CardComdClasses=tmp<<4;    	
	tmp=(u8)((CSD_Tab[1]&0x00FF0000)>>16);	 		
	cardinfo->SD_csd.CardComdClasses|=(tmp&0xF0)>>4;
	cardinfo->SD_csd.RdBlockLen=tmp&0x0F;	    	
	tmp=(u8)((CSD_Tab[1]&0x0000FF00)>>8);			
	cardinfo->SD_csd.PartBlockRead=(tmp&0x80)>>7;	
	cardinfo->SD_csd.WrBlockMisalign=(tmp&0x40)>>6;	
	cardinfo->SD_csd.RdBlockMisalign=(tmp&0x20)>>5;	
	cardinfo->SD_csd.DSRImpl=(tmp&0x10)>>4;
	cardinfo->SD_csd.Reserved2=0; 					
 	if((CardType==SDIO_STD_CAPACITY_SD_CARD_V1_1)||(CardType==SDIO_STD_CAPACITY_SD_CARD_V2_0)||(SDIO_MULTIMEDIA_CARD==CardType))//标准1.1/2.0卡/MMC卡
	{
		cardinfo->SD_csd.DeviceSize=(tmp&0x03)<<10;	
	 	tmp=(u8)(CSD_Tab[1]&0x000000FF); 			
		cardinfo->SD_csd.DeviceSize|=(tmp)<<2;
 		tmp=(u8)((CSD_Tab[2]&0xFF000000)>>24);		
		cardinfo->SD_csd.DeviceSize|=(tmp&0xC0)>>6;
 		cardinfo->SD_csd.MaxRdCurrentVDDMin=(tmp&0x38)>>3;
		cardinfo->SD_csd.MaxRdCurrentVDDMax=(tmp&0x07);
 		tmp=(u8)((CSD_Tab[2]&0x00FF0000)>>16);		
		cardinfo->SD_csd.MaxWrCurrentVDDMin=(tmp&0xE0)>>5;
		cardinfo->SD_csd.MaxWrCurrentVDDMax=(tmp&0x1C)>>2;
		cardinfo->SD_csd.DeviceSizeMul=(tmp&0x03)<<1;
 		tmp=(u8)((CSD_Tab[2]&0x0000FF00)>>8);	  	
		cardinfo->SD_csd.DeviceSizeMul|=(tmp&0x80)>>7;
 		cardinfo->CardCapacity=(cardinfo->SD_csd.DeviceSize+1);
		cardinfo->CardCapacity*=(1<<(cardinfo->SD_csd.DeviceSizeMul+2));
		cardinfo->CardBlockSize=1<<(cardinfo->SD_csd.RdBlockLen);
		cardinfo->CardCapacity*=cardinfo->CardBlockSize;
	}else if(CardType==SDIO_HIGH_CAPACITY_SD_CARD)	
	{
 		tmp=(u8)(CSD_Tab[1]&0x000000FF); 			
		cardinfo->SD_csd.DeviceSize=(tmp&0x3F)<<16;
 		tmp=(u8)((CSD_Tab[2]&0xFF000000)>>24); 		
 		cardinfo->SD_csd.DeviceSize|=(tmp<<8);
 		tmp=(u8)((CSD_Tab[2]&0x00FF0000)>>16);		
 		cardinfo->SD_csd.DeviceSize|=(tmp);
 		tmp=(u8)((CSD_Tab[2]&0x0000FF00)>>8); 		
 		cardinfo->CardCapacity=(long long)(cardinfo->SD_csd.DeviceSize+1)*512*1024;
		cardinfo->CardBlockSize=512; 			
	}	  
	cardinfo->SD_csd.EraseGrSize=(tmp&0x40)>>6;
	cardinfo->SD_csd.EraseGrMul=(tmp&0x3F)<<1;	   
	tmp=(u8)(CSD_Tab[2]&0x000000FF);				
	cardinfo->SD_csd.EraseGrMul|=(tmp&0x80)>>7;
	cardinfo->SD_csd.WrProtectGrSize=(tmp&0x7F);
 	tmp=(u8)((CSD_Tab[3]&0xFF000000)>>24);			
	cardinfo->SD_csd.WrProtectGrEnable=(tmp&0x80)>>7;
	cardinfo->SD_csd.ManDeflECC=(tmp&0x60)>>5;
	cardinfo->SD_csd.WrSpeedFact=(tmp&0x1C)>>2;
	cardinfo->SD_csd.MaxWrBlockLen=(tmp&0x03)<<2;	 
	tmp=(u8)((CSD_Tab[3]&0x00FF0000)>>16);		
	cardinfo->SD_csd.MaxWrBlockLen|=(tmp&0xC0)>>6;
	cardinfo->SD_csd.WriteBlockPaPartial=(tmp&0x20)>>5;
	cardinfo->SD_csd.Reserved3=0;
	cardinfo->SD_csd.ContentProtectAppli=(tmp&0x01);  
	tmp=(u8)((CSD_Tab[3]&0x0000FF00)>>8);		
	cardinfo->SD_csd.FileFormatGrouop=(tmp&0x80)>>7;
	cardinfo->SD_csd.CopyFlag=(tmp&0x40)>>6;
	cardinfo->SD_csd.PermWrProtect=(tmp&0x20)>>5;
	cardinfo->SD_csd.TempWrProtect=(tmp&0x10)>>4;
	cardinfo->SD_csd.FileFormat=(tmp&0x0C)>>2;
	cardinfo->SD_csd.ECC=(tmp&0x03);  
	tmp=(u8)(CSD_Tab[3]&0x000000FF);			
	cardinfo->SD_csd.CSD_CRC=(tmp&0xFE)>>1;
	cardinfo->SD_csd.Reserved4=1;		 
	tmp=(u8)((CID_Tab[0]&0xFF000000)>>24);		
	cardinfo->SD_cid.ManufacturerID=tmp;		    
	tmp=(u8)((CID_Tab[0]&0x00FF0000)>>16);		
	cardinfo->SD_cid.OEM_AppliID=tmp<<8;	  
	tmp=(u8)((CID_Tab[0]&0x000000FF00)>>8);		
	cardinfo->SD_cid.OEM_AppliID|=tmp;	    
	tmp=(u8)(CID_Tab[0]&0x000000FF);				
	cardinfo->SD_cid.ProdName1=tmp<<24;				  
	tmp=(u8)((CID_Tab[1]&0xFF000000)>>24); 		
	cardinfo->SD_cid.ProdName1|=tmp<<16;	  
	tmp=(u8)((CID_Tab[1]&0x00FF0000)>>16);	   	
	cardinfo->SD_cid.ProdName1|=tmp<<8;		 
	tmp=(u8)((CID_Tab[1]&0x0000FF00)>>8);		
	cardinfo->SD_cid.ProdName1|=tmp;		   
	tmp=(u8)(CID_Tab[1]&0x000000FF);	  		
	cardinfo->SD_cid.ProdName2=tmp;			  
	tmp=(u8)((CID_Tab[2]&0xFF000000)>>24); 		
	cardinfo->SD_cid.ProdRev=tmp;		 
	tmp=(u8)((CID_Tab[2]&0x00FF0000)>>16);		
	cardinfo->SD_cid.ProdSN=tmp<<24;	   
	tmp=(u8)((CID_Tab[2]&0x0000FF00)>>8); 		
	cardinfo->SD_cid.ProdSN|=tmp<<16;	   
	tmp=(u8)(CID_Tab[2]&0x000000FF);   			
	cardinfo->SD_cid.ProdSN|=tmp<<8;		   
	tmp=(u8)((CID_Tab[3]&0xFF000000)>>24); 		
	cardinfo->SD_cid.ProdSN|=tmp;			     
	tmp=(u8)((CID_Tab[3]&0x00FF0000)>>16);	 	
	cardinfo->SD_cid.Reserved1|=(tmp&0xF0)>>4;
	cardinfo->SD_cid.ManufactDate=(tmp&0x0F)<<8;    
	tmp=(u8)((CID_Tab[3]&0x0000FF00)>>8);		
	cardinfo->SD_cid.ManufactDate|=tmp;		 	  
	tmp=(u8)(CID_Tab[3]&0x000000FF);			
	cardinfo->SD_cid.CID_CRC=(tmp&0xFE)>>1;
	cardinfo->SD_cid.Reserved2=1;	 
	return errorstatus;
}

//Set SDIO data bus width (MMC does not support 4bit mode)
//wmode: data bus width mode, 
//       0 = 1bit, 1 = 4bit, 2 = 8bit
//return: fatal code
SD_Error SD_EnableWideBusOperation(u32 wmode)
{
  	SD_Error errorstatus=SD_OK;
 	if(SDIO_MULTIMEDIA_CARD==CardType)return SD_UNSUPPORTED_FEATURE;
 	else if((SDIO_STD_CAPACITY_SD_CARD_V1_1==CardType)||(SDIO_STD_CAPACITY_SD_CARD_V2_0==CardType)||(SDIO_HIGH_CAPACITY_SD_CARD==CardType))
	{
		if(wmode>=2)return SD_UNSUPPORTED_FEATURE;
 		else   
		{
			errorstatus=SDEnWideBus(wmode);
 			if(SD_OK==errorstatus)
			{
				SDIO->CLKCR&=~(3<<11);		    
				SDIO->CLKCR|=(u16)wmode<<11; 
				SDIO->CLKCR|=0<<14;			 
			}
		}  
	}
	return errorstatus; 
}

//Set SD card working mode
//return: fatal code
SD_Error SD_SetDeviceMode(u32 Mode)
{
	SD_Error errorstatus = SD_OK;
 	if((Mode==SD_DMA_MODE)||(Mode==SD_POLLING_MODE))DeviceMode=Mode;
	else errorstatus=SD_INVALID_PARAMETER;
	return errorstatus;	    
}

//Select Card
//Send out CMD7, selec the card with rca = addr, cancel other cards selection. 
//If addr is 0, choose no cards.
//addr:rca address of the desired card
//return: fatal code
SD_Error SD_SelectDeselect(u32 addr)
{
 	SDIO_Send_Cmd(SD_CMD_SEL_DESEL_CARD,1,addr);		 	   
   	return CmdResp1Error(SD_CMD_SEL_DESEL_CARD);	  
}


//Read one block
//buf: buffer to store data (must be 4bytes alignment)
//addr: read out sector address
//blksize: block size (for SD card is 512bytes usually)
SD_Error SD_ReadBlock(u8 *buf,u32 addr,u16 blksize)
{	  
	SD_Error errorstatus=SD_OK;
	u8 power;
   	u32 count=0,*tempbuff=(u32*)buf; 
	u32 timeout=0;   
   	if(NULL==buf)return SD_INVALID_PARAMETER; 
   	SDIO->DCTRL=0x0;	
	if(CardType==SDIO_HIGH_CAPACITY_SD_CARD)
	{
		blksize=512;
		addr>>=9;
	}   
  	SDIO_Send_Data_Cfg(SD_DATATIMEOUT,0,0,0);	
	if(SDIO->RESP1&SD_CARD_LOCKED)return SD_LOCK_UNLOCK_FAILED;
	if((blksize>0)&&(blksize<=2048)&&((blksize&(blksize-1))==0))
	{
		power=convert_from_bytes_to_power_of_two(blksize);	    	   
		SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,blksize);	 	   
		errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);	   
		if(errorstatus!=SD_OK)return errorstatus;   		 
	}else return SD_INVALID_PARAMETER;	  	  									    
  	SDIO_Send_Data_Cfg(SD_DATATIMEOUT,blksize,power,1);		  
   	SDIO_Send_Cmd(SD_CMD_READ_SINGLE_BLOCK,1,addr);		 	   
	errorstatus=CmdResp1Error(SD_CMD_READ_SINGLE_BLOCK);   
	if(errorstatus!=SD_OK)return errorstatus;   			 
	if(DeviceMode==SD_POLLING_MODE)							 
	{
		while(!(SDIO->STA&((1<<5)|(1<<1)|(1<<3)|(1<<10)|(1<<9))))
		{
			if(SDIO->STA&(1<<15))						
			{
				for(count=0;count<8;count++)			
				{
					*(tempbuff+count)=SDIO->FIFO;	 
				}
				tempbuff+=8;
			}
		} 
		if(SDIO->STA&(1<<3))		
		{										   
	 		SDIO->ICR|=1<<3; 		
			return SD_DATA_TIMEOUT;
	 	}else if(SDIO->STA&(1<<1))	
		{
	 		SDIO->ICR|=1<<1; 		
			return SD_DATA_CRC_FAIL;		   
		}else if(SDIO->STA&(1<<5)) 	
		{
	 		SDIO->ICR|=1<<5; 		
			return SD_RX_OVERRUN;		 
		}else if(SDIO->STA&(1<<9)) 	
		{
	 		SDIO->ICR|=1<<9; 		
			return SD_START_BIT_ERR;		 
		}   
		while(SDIO->STA&(1<<21))	
		{
			*tempbuff=SDIO->FIFO;	
			tempbuff++;
		}
		SDIO->ICR=0X5FF;	 		
	}else if(DeviceMode==SD_DMA_MODE)
	{
 		TransferError=SD_OK;
		StopCondition=0;			
		TransferEnd=0;				
		SDIO->MASK|=(1<<1)|(1<<3)|(1<<8)|(1<<5)|(1<<9);	
	 	SDIO->DCTRL|=1<<3;		 	
 	    SD_DMA_Config((u32*)buf,blksize,0);
		timeout=SDIO_DATATIMEOUT;
 		while(((DMA2->ISR&0X2000)==RESET)&&(TransferEnd==0)&&(TransferError==SD_OK)&&timeout)timeout--;//等待传输完成 
		if(timeout==0)return SD_DATA_TIMEOUT;
		if(TransferError!=SD_OK)errorstatus=TransferError;  
    }   
 	return errorstatus; 
}

//Read multi blocks
//addr: start block address to read
//blksize: block size
//nblks: number of blocks to read
//return: fatal code
SD_Error SD_ReadMultiBlocks(u8 *buf,u32 addr,u16 blksize,u32 nblks)
{
  	SD_Error errorstatus=SD_OK;
	u8 power;
   	u32 count=0,*tempbuff=(u32*)buf;
	u32 timeout=0;  
    SDIO->DCTRL=0x0;		   
	if(CardType==SDIO_HIGH_CAPACITY_SD_CARD)
	{
		blksize=512;
		addr>>=9;
	}  
   	SDIO_Send_Data_Cfg(SD_DATATIMEOUT,0,0,0);	
	if(SDIO->RESP1&SD_CARD_LOCKED)return SD_LOCK_UNLOCK_FAILED;
	if((blksize>0)&&(blksize<=2048)&&((blksize&(blksize-1))==0))
	{
		power=convert_from_bytes_to_power_of_two(blksize);	    
		SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,blksize);	 	   
		errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);	 
		if(errorstatus!=SD_OK)return errorstatus;   		 
	}else return SD_INVALID_PARAMETER;	  
	if(nblks>1)											  
	{									    
 	  	if(nblks*blksize>SD_MAX_DATA_LENGTH)return SD_INVALID_PARAMETER;
		SDIO_Send_Data_Cfg(SD_DATATIMEOUT,nblks*blksize,power,1);  
	  	SDIO_Send_Cmd(SD_CMD_READ_MULT_BLOCK,1,addr);	 	   
		errorstatus=CmdResp1Error(SD_CMD_READ_MULT_BLOCK);   
		if(errorstatus!=SD_OK)return errorstatus;   		  
		if(DeviceMode==SD_POLLING_MODE)
		{
			while(!(SDIO->STA&((1<<5)|(1<<1)|(1<<3)|(1<<8)|(1<<9))))
			{
					if(SDIO->STA&(1<<15))						
					{
						for(count=0;count<8;count++)			
						{
							*(tempbuff+count)=SDIO->FIFO;	 
						}
						tempbuff+=8;
					}
			} 
			if(SDIO->STA&(1<<3))		
			{										   
		 		SDIO->ICR|=1<<3; 		
				return SD_DATA_TIMEOUT;
		 	}else if(SDIO->STA&(1<<1))	
			{
		 		SDIO->ICR|=1<<1; 		
				return SD_DATA_CRC_FAIL;		   
			}else if(SDIO->STA&(1<<5)) 	
			{
		 		SDIO->ICR|=1<<5; 		
				return SD_RX_OVERRUN;		 
			}else if(SDIO->STA&(1<<9)) 	
			{
		 		SDIO->ICR|=1<<9; 		
				return SD_START_BIT_ERR;		 
			}   
			while(SDIO->STA&(1<<21))	
			{
				*tempbuff=SDIO->FIFO;	
				tempbuff++;
			}
	 		if(SDIO->STA&(1<<8))		
			{
				if((SDIO_STD_CAPACITY_SD_CARD_V1_1==CardType)||(SDIO_STD_CAPACITY_SD_CARD_V2_0==CardType)||(SDIO_HIGH_CAPACITY_SD_CARD==CardType))
				{
					SDIO_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);		 	   
					errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);   
					if(errorstatus!=SD_OK)return errorstatus;	 
				}
 			}
	 		SDIO->ICR=0X5FF;	 		 
 		}else if(DeviceMode==SD_DMA_MODE)
		{
	   		TransferError=SD_OK;
			StopCondition=1;			 
			TransferEnd=0;				
			SDIO->MASK|=(1<<1)|(1<<3)|(1<<8)|(1<<5)|(1<<9);	 
		 	SDIO->DCTRL|=1<<3;		 						 
	 	    SD_DMA_Config((u32*)buf,nblks*blksize,0);
			timeout=SDIO_DATATIMEOUT;
	 		while(((DMA2->ISR&0X2000)==RESET)&&timeout)timeout--; 
			if(timeout==0)return SD_DATA_TIMEOUT;
			while((TransferEnd==0)&&(TransferError==SD_OK)); 
			if(TransferError!=SD_OK)errorstatus=TransferError;  	 
		}		 
  	}
	return errorstatus;
}			    																  

//Write one block
//addr: block address to be written
//blksize: block size
//return: fatal code
SD_Error SD_WriteBlock(u8 *buf,u32 addr,  u16 blksize)
{
	SD_Error errorstatus = SD_OK;
	u8  power=0,cardstate=0;
	u32 timeout=0,bytestransferred=0;
	u32 cardstatus=0,count=0,restwords=0;
	u32	tlen=blksize;						
	u32*tempbuff=(u32*)buf;								 
 	if(buf==NULL)return SD_INVALID_PARAMETER;   
  	SDIO->DCTRL=0x0;							
  	SDIO_Send_Data_Cfg(SD_DATATIMEOUT,0,0,0);	
	if(SDIO->RESP1&SD_CARD_LOCKED)return SD_LOCK_UNLOCK_FAILED;
 	if(CardType==SDIO_HIGH_CAPACITY_SD_CARD)	
	{
		blksize=512;
		addr>>=9;
	}    
	if((blksize>0)&&(blksize<=2048)&&((blksize&(blksize-1))==0))
	{
		power=convert_from_bytes_to_power_of_two(blksize);	    
		SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,blksize);	 	   
		errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);	   
		if(errorstatus!=SD_OK)return errorstatus;   		 
	}else return SD_INVALID_PARAMETER;	 
   	SDIO_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16);	 	   
	errorstatus=CmdResp1Error(SD_CMD_SEND_STATUS);		   		   
	if(errorstatus!=SD_OK)return errorstatus;
	cardstatus=SDIO->RESP1;													  
	timeout=SD_DATATIMEOUT;
   	while(((cardstatus&0x00000100)==0)&&(timeout>0)) 	
	{
		timeout--;
	   	SDIO_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16); 	   
		errorstatus=CmdResp1Error(SD_CMD_SEND_STATUS);	   		   
		if(errorstatus!=SD_OK)return errorstatus;				    
		cardstatus=SDIO->RESP1;													  
	}
	if(timeout==0)return SD_ERROR;
   	SDIO_Send_Cmd(SD_CMD_WRITE_SINGLE_BLOCK,1,addr);	 	   
	errorstatus=CmdResp1Error(SD_CMD_WRITE_SINGLE_BLOCK);   		   
	if(errorstatus!=SD_OK)return errorstatus;   	  
	StopCondition=0;									 
 	SDIO_Send_Data_Cfg(SD_DATATIMEOUT,blksize,power,0);		  
	if (DeviceMode == SD_POLLING_MODE)
	{
		while(!(SDIO->STA&((1<<10)|(1<<4)|(1<<1)|(1<<3)|(1<<9))))
		{
			if(SDIO->STA&(1<<14))							
			{
				if((tlen-bytestransferred)<SD_HALFFIFOBYTES)
				{
					restwords=((tlen-bytestransferred)%4==0)?((tlen-bytestransferred)/4):((tlen-bytestransferred)/4+1);
					
					for(count=0;count<restwords;count++,tempbuff++,bytestransferred+=4)
					{
						SDIO->FIFO=*tempbuff;
					}
				}else
				{
					for(count=0;count<8;count++)
					{
						SDIO->FIFO=*(tempbuff+count);
					}
					tempbuff+=8;
					bytestransferred+=32;
				}

			}
		} 
		if(SDIO->STA&(1<<3))		
		{										   
	 		SDIO->ICR|=1<<3; 		
			return SD_DATA_TIMEOUT;
	 	}else if(SDIO->STA&(1<<1))	
		{
	 		SDIO->ICR|=1<<1; 		
			return SD_DATA_CRC_FAIL;		   
		}else if(SDIO->STA&(1<<4)) 	
		{
	 		SDIO->ICR|=1<<4; 		
			return SD_TX_UNDERRUN;		 
		}else if(SDIO->STA&(1<<9)) 	
		{
	 		SDIO->ICR|=1<<9; 		
			return SD_START_BIT_ERR;		 
		}   
		SDIO->ICR=0X5FF;	 			  
	}else if(DeviceMode==SD_DMA_MODE)
	{
   		TransferError=SD_OK;
		StopCondition=0;			 
		TransferEnd=0;				
		SDIO->MASK|=(1<<1)|(1<<3)|(1<<8)|(1<<4)|(1<<9);	
		SD_DMA_Config((u32*)buf,blksize,1);				
 	 	SDIO->DCTRL|=1<<3;								
		timeout=SDIO_DATATIMEOUT;
 		while(((DMA2->ISR&0X2000)==RESET)&&timeout)timeout--; 
		if(timeout==0)
		{
  			SD_Init();	 					
			return SD_DATA_TIMEOUT;				 
 		}
		timeout=SDIO_DATATIMEOUT;
		while((TransferEnd==0)&&(TransferError==SD_OK)&&timeout)timeout--;
 		if(timeout==0)return SD_DATA_TIMEOUT;				 
  		if(TransferError!=SD_OK)return TransferError;
 	}  
 	SDIO->ICR=0X5FF;	 		
 	errorstatus=IsCardProgramming(&cardstate);
 	while((errorstatus==SD_OK)&&((cardstate==SD_CARD_PROGRAMMING)||(cardstate==SD_CARD_RECEIVING)))
	{
		errorstatus=IsCardProgramming(&cardstate);
	}   
	return errorstatus;
}

//Write multi blocks
//addr: start block address to be written
//blksize: block size
//nblks: number of blocks
//return: fatal code										   
SD_Error SD_WriteMultiBlocks(u8 *buf,u32 addr,u16 blksize,u32 nblks)
{
	SD_Error errorstatus = SD_OK;
	u8  power = 0, cardstate = 0;
	u32 timeout=0,bytestransferred=0;
	u32 count = 0, restwords = 0;
	u32 tlen=nblks*blksize;				
	u32 *tempbuff = (u32*)buf;  
  	if(buf==NULL)return SD_INVALID_PARAMETER;   
  	SDIO->DCTRL=0x0;							
  	SDIO_Send_Data_Cfg(SD_DATATIMEOUT,0,0,0);	
	if(SDIO->RESP1&SD_CARD_LOCKED)return SD_LOCK_UNLOCK_FAILED;
 	if(CardType==SDIO_HIGH_CAPACITY_SD_CARD)
	{
		blksize=512;
		addr>>=9;
	}    
	if((blksize>0)&&(blksize<=2048)&&((blksize&(blksize-1))==0))
	{
		power=convert_from_bytes_to_power_of_two(blksize);	    
		SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,blksize);	 	   
		errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);	   
		if(errorstatus!=SD_OK)return errorstatus;   		 
	}else return SD_INVALID_PARAMETER;	 
	if(nblks>1)
	{					  
		if(nblks*blksize>SD_MAX_DATA_LENGTH)return SD_INVALID_PARAMETER;   
     	if((SDIO_STD_CAPACITY_SD_CARD_V1_1==CardType)||(SDIO_STD_CAPACITY_SD_CARD_V2_0==CardType)||(SDIO_HIGH_CAPACITY_SD_CARD==CardType))
    	{
			//提高性能
	 	   	SDIO_Send_Cmd(SD_CMD_APP_CMD,1,(u32)RCA<<16);	 	   
			errorstatus=CmdResp1Error(SD_CMD_APP_CMD);		   		   
			if(errorstatus!=SD_OK)return errorstatus;				    
	 	   	SDIO_Send_Cmd(SD_CMD_SET_BLOCK_COUNT,1,nblks);	 	   
			errorstatus=CmdResp1Error(SD_CMD_SET_BLOCK_COUNT);   		   
			if(errorstatus!=SD_OK)return errorstatus;				    
		} 
		SDIO_Send_Cmd(SD_CMD_WRITE_MULT_BLOCK,1,addr);			   
		errorstatus=CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK);	   		   
		if(errorstatus!=SD_OK)return errorstatus;
 	 	SDIO_Send_Data_Cfg(SD_DATATIMEOUT,nblks*blksize,power,0);	
	    if(DeviceMode==SD_POLLING_MODE)
	    {
			while(!(SDIO->STA&((1<<4)|(1<<1)|(1<<8)|(1<<3)|(1<<9))))
			{
				if(SDIO->STA&(1<<14))							
				{	  
					if((tlen-bytestransferred)<SD_HALFFIFOBYTES)
					{
						restwords=((tlen-bytestransferred)%4==0)?((tlen-bytestransferred)/4):((tlen-bytestransferred)/4+1);
						for(count=0;count<restwords;count++,tempbuff++,bytestransferred+=4)
						{
							SDIO->FIFO=*tempbuff;
						}
					}else 										
					{
						for(count=0;count<SD_HALFFIFO;count++)
						{
							SDIO->FIFO=*(tempbuff+count);
						}
						tempbuff+=SD_HALFFIFO;
						bytestransferred+=SD_HALFFIFOBYTES;
					} 
				}
			} 
			if(SDIO->STA&(1<<3))		
			{										   
		 		SDIO->ICR|=1<<3; 		
				return SD_DATA_TIMEOUT;
		 	}else if(SDIO->STA&(1<<1))	
			{
		 		SDIO->ICR|=1<<1; 		
				return SD_DATA_CRC_FAIL;		   
			}else if(SDIO->STA&(1<<4)) 	
			{
		 		SDIO->ICR|=1<<4; 		
				return SD_TX_UNDERRUN;		 
			}else if(SDIO->STA&(1<<9)) 	
			{
		 		SDIO->ICR|=1<<9; 		
				return SD_START_BIT_ERR;		 
			}   										   
			if(SDIO->STA&(1<<8))		
			{															 
				if((SDIO_STD_CAPACITY_SD_CARD_V1_1==CardType)||(SDIO_STD_CAPACITY_SD_CARD_V2_0==CardType)||(SDIO_HIGH_CAPACITY_SD_CARD==CardType))
				{
					SDIO_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);		 	   
					errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);   
					if(errorstatus!=SD_OK)return errorstatus;	 
				}
			}
	 		SDIO->ICR=0X5FF;	 		 
	    }else if(DeviceMode==SD_DMA_MODE)
		{
	   		TransferError=SD_OK;
			StopCondition=1;			 
			TransferEnd=0;				
			SDIO->MASK|=(1<<1)|(1<<3)|(1<<8)|(1<<4)|(1<<9);	
			SD_DMA_Config((u32*)buf,nblks*blksize,1);		
	 	 	SDIO->DCTRL|=1<<3;								
			timeout=SDIO_DATATIMEOUT;
	 		while(((DMA2->ISR&0X2000)==RESET)&&timeout)timeout--; 
			if(timeout==0)	 								
			{									  
  				SD_Init();	 					
	 			return SD_DATA_TIMEOUT;				 
	 		}
			timeout=SDIO_DATATIMEOUT;
			while((TransferEnd==0)&&(TransferError==SD_OK)&&timeout)timeout--;
	 		if(timeout==0)return SD_DATA_TIMEOUT;				 
	 		if(TransferError!=SD_OK)return TransferError;	 
		}
  	}
 	SDIO->ICR=0X5FF;	 		
 	errorstatus=IsCardProgramming(&cardstate);
 	while((errorstatus==SD_OK)&&((cardstate==SD_CARD_PROGRAMMING)||(cardstate==SD_CARD_RECEIVING)))
	{
		errorstatus=IsCardProgramming(&cardstate);
	}   
	return errorstatus;	   
}

//SDIO IRQHandler	  
void SDIO_IRQHandler(void) 
{											
 	SD_ProcessIRQSrc();
}	 																    

//SDIO interrupt 
//return: fatal code
SD_Error SD_ProcessIRQSrc(void)
{
	if(SDIO->STA&(1<<8))	//Rx finish interrupt
	{	 
		if (StopCondition==1)
		{
			SDIO_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);			   
			TransferError=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);
		}else TransferError = SD_OK;	
 		SDIO->ICR|=1<<8;
		SDIO->MASK&=~((1<<1)|(1<<3)|(1<<8)|(1<<14)|(1<<15)|(1<<4)|(1<<5)|(1<<9));
 		TransferEnd = 1;
		return(TransferError);
	}
 	if(SDIO->STA&(1<<1))	//CRC error interrupt
	{
		SDIO->ICR|=1<<1;
		SDIO->MASK&=~((1<<1)|(1<<3)|(1<<8)|(1<<14)|(1<<15)|(1<<4)|(1<<5)|(1<<9));
	    TransferError = SD_DATA_CRC_FAIL;
	    return(SD_DATA_CRC_FAIL);
	}
 	if(SDIO->STA&(1<<3))	//data timeout interrupt
	{
		SDIO->ICR|=1<<3;
		SDIO->MASK&=~((1<<1)|(1<<3)|(1<<8)|(1<<14)|(1<<15)|(1<<4)|(1<<5)|(1<<9));
	    TransferError = SD_DATA_TIMEOUT;
	    return(SD_DATA_TIMEOUT);
	}
  	if(SDIO->STA&(1<<5))	//FIFO upper-overload interrupt
	{
		SDIO->ICR|=1<<5;
		SDIO->MASK&=~((1<<1)|(1<<3)|(1<<8)|(1<<14)|(1<<15)|(1<<4)|(1<<5)|(1<<9));
	    TransferError = SD_RX_OVERRUN;
	    return(SD_RX_OVERRUN);
	}
   	if(SDIO->STA&(1<<4))	//FIFO downer-overload interrupt
	{
		SDIO->ICR|=1<<4;
		SDIO->MASK&=~((1<<1)|(1<<3)|(1<<8)|(1<<14)|(1<<15)|(1<<4)|(1<<5)|(1<<9));
	    TransferError = SD_TX_UNDERRUN;
	    return(SD_TX_UNDERRUN);
	}
	if(SDIO->STA&(1<<9))	//Srart bit error interrupt
	{
		SDIO->ICR|=1<<9;
		SDIO->MASK&=~((1<<1)|(1<<3)|(1<<8)|(1<<14)|(1<<15)|(1<<4)|(1<<5)|(1<<9));
	    TransferError = SD_START_BIT_ERR;
	    return(SD_START_BIT_ERR);
	}
	return(SD_OK);
}
  
//Check CMD execution
//return: fatal code
SD_Error CmdError(void)
{
	SD_Error errorstatus = SD_OK;
	u32 timeout=SDIO_CMD0TIMEOUT;	   
	while(timeout--)
	{
		if(SDIO->STA&(1<<7))break;		 
	}	    
	if(timeout==0)return SD_CMD_RSP_TIMEOUT;  
	SDIO->ICR=0X5FF;				
	return errorstatus;
}	 

//Check R7 response
//return: fatal code
SD_Error CmdResp7Error(void)
{
	SD_Error errorstatus=SD_OK;
	u32 status;
	u32 timeout=SDIO_CMD0TIMEOUT;
 	while(timeout--)
	{
		status=SDIO->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;	
	}
 	if((timeout==0)||(status&(1<<2)))	
	{																				    
		errorstatus=SD_CMD_RSP_TIMEOUT;	
		SDIO->ICR|=1<<2;				
		return errorstatus;
	}	 
	if(status&1<<6)						
	{								   
		errorstatus=SD_OK;
		SDIO->ICR|=1<<6;				
 	}
	return errorstatus;
}	   

//Check R1 response
//return: fatal code
SD_Error CmdResp1Error(u8 cmd)
{	  
   	u32 status;
	while(1)
	{
		status=SDIO->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;	
	}  
 	if(status&(1<<2))					
	{																				    
 		SDIO->ICR=1<<2;					
		return SD_CMD_RSP_TIMEOUT;
	}	
 	if(status&(1<<0))					
	{																				    
 		SDIO->ICR=1<<0;					
		return SD_CMD_CRC_FAIL;
	}		
	if(SDIO->RESPCMD!=cmd)return SD_ILLEGAL_CMD; 
  	SDIO->ICR=0X5FF;	 				
	return (SD_Error)(SDIO->RESP1&SD_OCR_ERRORBITS);
}

//Check R3 response
//return: fatal code
SD_Error CmdResp3Error(void)
{
	u32 status;						 
 	while(1)
	{
		status=SDIO->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;
	}
 	if(status&(1<<2))					
	{											 
		SDIO->ICR|=1<<2;			
		return SD_CMD_RSP_TIMEOUT;
	}	 
   	SDIO->ICR=0X5FF;	 			
 	return SD_OK;								  
}

//Check R2 response
//return: fatal code
SD_Error CmdResp2Error(void)
{
	SD_Error errorstatus=SD_OK;
	u32 status;
	u32 timeout=SDIO_CMD0TIMEOUT;
 	while(timeout--)
	{
		status=SDIO->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;
	}
  	if((timeout==0)||(status&(1<<2)))	
	{																				    
		errorstatus=SD_CMD_RSP_TIMEOUT; 
		SDIO->ICR|=1<<2;				
		return errorstatus;
	}	 
	if(status&1<<0)						
	{								   
		errorstatus=SD_CMD_CRC_FAIL;
		SDIO->ICR|=1<<0;				
 	}
	SDIO->ICR=0X5FF;	 				
 	return errorstatus;								    		 
} 

//Check R6 response
//return: fatal code
SD_Error CmdResp6Error(u8 cmd,u16*prca)
{
	SD_Error errorstatus=SD_OK;
	u32 status;					    
	u32 rspr1;
 	while(1)
	{
		status=SDIO->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;
	}
	if(status&(1<<2))					
	{																				    
 		SDIO->ICR|=1<<2;				
		return SD_CMD_RSP_TIMEOUT;
	}	 	 
	if(status&1<<0)						
	{								   
		SDIO->ICR|=1<<0;				
 		return SD_CMD_CRC_FAIL;
	}
	if(SDIO->RESPCMD!=cmd)				
	{
 		return SD_ILLEGAL_CMD; 		
	}	    
	SDIO->ICR=0X5FF;	 				
	rspr1=SDIO->RESP1;						 
	if(SD_ALLZERO==(rspr1&(SD_R6_GENERAL_UNKNOWN_ERROR|SD_R6_ILLEGAL_CMD|SD_R6_COM_CRC_FAILED)))
	{
		*prca=(u16)(rspr1>>16);			
		return errorstatus;
	}
   	if(rspr1&SD_R6_GENERAL_UNKNOWN_ERROR)return SD_GENERAL_UNKNOWN_ERROR;
   	if(rspr1&SD_R6_ILLEGAL_CMD)return SD_ILLEGAL_CMD;
   	if(rspr1&SD_R6_COM_CRC_FAILED)return SD_COM_CRC_FAILED;
	return errorstatus;
}

//Enable wide data bus mode
//enx: 0 = disable, 1 = enable
//return: fatal code
SD_Error SDEnWideBus(u8 enx)
{
	SD_Error errorstatus = SD_OK;
 	u32 scr[2]={0,0};
	u8 arg=0X00;
	if(enx)arg=0X02;
	else arg=0X00;
 	if(SDIO->RESP1&SD_CARD_LOCKED)return SD_LOCK_UNLOCK_FAILED;	    
 	errorstatus=FindSCR(RCA,scr);						
 	if(errorstatus!=SD_OK)return errorstatus;
	if((scr[1]&SD_WIDE_BUS_SUPPORT)!=SD_ALLZERO)		
	{
	 	SDIO_Send_Cmd(SD_CMD_APP_CMD,1,(u32)RCA<<16);				  
	 	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
	 	if(errorstatus!=SD_OK)return errorstatus; 
	 	SDIO_Send_Cmd(SD_CMD_APP_SD_SET_BUSWIDTH,1,arg);							  
		errorstatus=CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);
		return errorstatus;
	}else return SD_REQUEST_NOT_APPLICABLE;				 
}												   

//Check if the card is writing
//pstatus: current status
//return: fatal code
SD_Error IsCardProgramming(u8 *pstatus)
{
 	vu32 respR1 = 0, status = 0; 
  	SDIO_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16);			   
  	status=SDIO->STA;
	while(!(status&((1<<0)|(1<<6)|(1<<2))))status=SDIO->STA;
   	if(status&(1<<0))			
	{
		SDIO->ICR|=1<<0;		
		return SD_CMD_CRC_FAIL;
	}
   	if(status&(1<<2))			
	{
		SDIO->ICR|=1<<2;		
		return SD_CMD_RSP_TIMEOUT;
	}
 	if(SDIO->RESPCMD!=SD_CMD_SEND_STATUS)return SD_ILLEGAL_CMD;
	SDIO->ICR=0X5FF;	 		
	respR1=SDIO->RESP1;
	*pstatus=(u8)((respR1>>9)&0x0000000F);
	return SD_OK;
}

//Get register SCR value of SD card
//rca: address of SD card
//pscr: data buffer to store SCR content
//return: fatal code   
SD_Error FindSCR(u16 rca,u32 *pscr)
{ 
	u32 index = 0;
	SD_Error errorstatus = SD_OK;
	u32 tempscr[2]={0,0};  
 	SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,8);			 
 	errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);
 	if(errorstatus!=SD_OK)return errorstatus;	    
  	SDIO_Send_Cmd(SD_CMD_APP_CMD,1,(u32)rca<<16);	 									  
 	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
 	if(errorstatus!=SD_OK)return errorstatus;
	SDIO_Send_Data_Cfg(SD_DATATIMEOUT,8,3,1);		
   	SDIO_Send_Cmd(SD_CMD_SD_APP_SEND_SCR,1,0);						  
 	errorstatus=CmdResp1Error(SD_CMD_SD_APP_SEND_SCR);
 	if(errorstatus!=SD_OK)return errorstatus;							   
 	while(!(SDIO->STA&(SDIO_FLAG_RXOVERR|SDIO_FLAG_DCRCFAIL|SDIO_FLAG_DTIMEOUT|SDIO_FLAG_DBCKEND|SDIO_FLAG_STBITERR)))
	{
		if(SDIO->STA&(1<<21))
		{
			*(tempscr+index)=SDIO->FIFO;	
			index++;
			if(index>=2)break;
		}
	}
 	if(SDIO->STA&(1<<3))		
	{										 
 		SDIO->ICR|=1<<3;		
		return SD_DATA_TIMEOUT;
	}
	else if(SDIO->STA&(1<<1))	
	{
 		SDIO->ICR|=1<<1;		
		return SD_DATA_CRC_FAIL;   
	}
	else if(SDIO->STA&(1<<5))	
	{
 		SDIO->ICR|=1<<5;		
		return SD_RX_OVERRUN;   	   
	}
	else if(SDIO->STA&(1<<9))	
	{
 		SDIO->ICR|=1<<9;		
		return SD_START_BIT_ERR;    
	}
   	SDIO->ICR=0X5FF;	 			 
		
	*(pscr+1)=((tempscr[0]&SD_0TO7BITS)<<24)|((tempscr[0]&SD_8TO15BITS)<<8)|((tempscr[0]&SD_16TO23BITS)>>8)|((tempscr[0]&SD_24TO31BITS)>>24);
	*(pscr)=((tempscr[1]&SD_0TO7BITS)<<24)|((tempscr[1]&SD_8TO15BITS)<<8)|((tempscr[1]&SD_16TO23BITS)>>8)|((tempscr[1]&SD_24TO31BITS)>>24);
 	return errorstatus;
}


u8 convert_from_bytes_to_power_of_two(u16 NumberOfBytes)
{
	u8 count=0;
	while(NumberOfBytes!=1)
	{
		NumberOfBytes>>=1;
		count++;
	}
	return count;
} 	 

//Configure SDIO DMA function
//mbuf: memory address
//bufsize: data transmission sieze
//dir: transmission direction
//	  1 = memory-->SDIO (write)
//    0 = SDIO-->memory (read)
void SD_DMA_Config(u32*mbuf,u32 bufsize,u8 dir)
{				  
 	DMA2->IFCR|=(0XF<<12);				
 	DMA2_Channel4->CCR&=~(1<<0);		
  	DMA2_Channel4->CCR&=~(0X7FF<<4);	
 	DMA2_Channel4->CCR|=dir<<4;  		  
	DMA2_Channel4->CCR|=0<<5;  			
	DMA2_Channel4->CCR|=0<<6; 			
	DMA2_Channel4->CCR|=1<<7;  			
	DMA2_Channel4->CCR|=2<<8;  			
	DMA2_Channel4->CCR|=2<<10; 			
	DMA2_Channel4->CCR|=2<<12; 				  
  	DMA2_Channel4->CNDTR=bufsize/4;   		  
 	DMA2_Channel4->CPAR=(u32)&SDIO->FIFO; 
	DMA2_Channel4->CMAR=(u32)mbuf; 		
 	DMA2_Channel4->CCR|=1<<0; 				
}   

//Read SD card
//buf: buffer to store data
//sector: sector address
//cnt: number of sector
//return: 
//      0 = normal
// others = fatal code			  				 
u8 SD_ReadDisk(u8*buf,u32 sector,u8 cnt)
{
	u8 sta=SD_OK;
	u8 n;
	if(CardType!=SDIO_STD_CAPACITY_SD_CARD_V1_1)sector<<=9;
	if((u32)buf%4!=0)
	{
	 	for(n=0;n<cnt;n++)
		{
		 	sta=SD_ReadBlock(SDIO_DATA_BUFFER,sector,512);    	
			memcpy(buf,SDIO_DATA_BUFFER,512);
			buf+=512;
		} 
	}else
	{
		if(cnt==1)sta=SD_ReadBlock(buf,sector,512);    	
		else sta=SD_ReadMultiBlocks(buf,sector,512,cnt);  
	}
	return sta;
}

//Write SD card
//buf: data buffer to write
//sector: sector address
//cnt: number of sector
//return: 
//      0 = normal
// others = fatal code	
u8 SD_WriteDisk(u8*buf,u32 sector,u8 cnt)
{
	u8 sta=SD_OK;
	u8 n;
	if(CardType!=SDIO_STD_CAPACITY_SD_CARD_V1_1)sector<<=9;
	if((u32)buf%4!=0)
	{
	 	for(n=0;n<cnt;n++)
		{
			memcpy(SDIO_DATA_BUFFER,buf,512);
		 	sta=SD_WriteBlock(SDIO_DATA_BUFFER,sector,512);    	
			buf+=512;
		} 
	}else
	{
		if(cnt==1)sta=SD_WriteBlock(buf,sector,512);    	
		else sta=SD_WriteMultiBlocks(buf,sector,512,cnt);	  
	}
	return sta;
}
