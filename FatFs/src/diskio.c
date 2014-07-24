/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2013        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control module to the FatFs module with a defined API.        */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "sdio_sdcard.h"

/* Definitions of physical drive number for each media */
#define SDCARD 0

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
BYTE pdrv	/* Physical drive nmuber (0..) */
)
{
	int result;

	switch (pdrv) 
	{
		case SDCARD:
					result = SD_Init();
					return result;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
BYTE pdrv	/* Physical drive nmuber (0..) */
)
{
	pdrv--;
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s) */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
BYTE pdrv,	/* Physical drive nmuber (0..) */
BYTE *buff,	/* Data buffer to store read data */
DWORD sector,	/* Sector address (LBA) */
BYTE count	/* Number of sectors to read (1..128) */
)
{
	DRESULT res;
	u8 retry=0X1F;	
	switch (pdrv) 
	{
		case SDCARD:
					while(retry)
					{
						res=SD_ReadDisk(buff,sector,count);
						if(res==0)break;						
						retry--;
					}
					break;					
	}
    if(res==0x00)return RES_OK;	 
    else return RES_ERROR;	
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s) */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
BYTE pdrv,	/* Physical drive nmuber (0..) */
const BYTE *buff,	/* Data to be written */
DWORD sector,	/* Sector address (LBA) */
BYTE count	/* Number of sectors to write (1..128) */
)
{
	u8 res=0;  
	u8 retry=0X1F;

	switch (pdrv) 
	{
		case SDCARD:
			while(retry)
			{
				res=SD_WriteDisk((u8*)buff,sector,count);
				if(res==0)break;
				retry--;
			}
			break;
	}
    if(res == 0x00)return RES_OK;	 
    else return RES_ERROR;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
BYTE pdrv,	/* Physical drive nmuber (0..) */
BYTE cmd,	/* Control code */
void *buff	/* Buffer to send/receive control data */
)
{
	DRESULT res = 0;						  			     
	if(pdrv==SDCARD)
	{
	    switch(cmd)
	    {
		    case CTRL_SYNC:	    
 		        res = RES_OK;
		        break;	 
		    case GET_SECTOR_SIZE:
		        *(WORD*)buff = 512;
		        res = RES_OK;
		        break;	 
		    case GET_BLOCK_SIZE:
		        *(WORD*)buff = 8;
		        res = RES_OK;
		        break;	 
		    case GET_SECTOR_COUNT:
 		        res = RES_OK;
		        break;
		    default:
		        res = RES_PARERR;
		        break;
	    }
	}
	return res;
}
#endif


DWORD get_fattime(void)
{
	return 0;
}
