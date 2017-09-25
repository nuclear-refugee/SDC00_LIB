//Author    : Name
//Last Edit : Name YYYY/MM/DD
//ASA Lib Gen2

#include "ASA_Lib.h"
#include "ASA_Core_M128.h"
#include "ff.h"

/**** Internal Definitions ****/

// SDC00 set File Control Flag:
#define SDC_FCF_CLOSE			0x01
#define SDC_FCF_READ			0x02
#define SDC_FCF_OVERWRITE		0x04
#define SDC_FCF_CONTINUEWRITE	0x08
#define SDC_FCF_READPARAMETER	0x10
#define SDC_FCF_RENAME			0x20

// ASA SDC Return Error Code Table
#define ARSDC_OK 0
#define ARSDC_ID_ERR 1
#define ARSDC_LSBYTE_ERR 2
#define ARSDC_SHIFT_ERR 3
#define ARSDC_MUTI_SET_ERR 4
#define ARSDC_FILE_ID_ERR 5
#define ARSDC_OPEN_FILE_ERR 6
#define ARSDC_SDC_STATE_ERR 7
#define ARSDC_BYTES_ERR 8
#define ARSDC_NO_OPEN_FILE 9
#define ARSDC_READ_SETTING 10
#define ARSDC_LIST_READ_ONLY 11
#define ARSDC_WRITE_SETTING 12
#define ARSDC_NO_DATA_TO_READ 13
#define ARSDC_DATA_NUM_ENOUGH 14
#define ARSDC_RENAME_ERR 15			// ���w�ɮ׽s�����s�b���ɦW�w�s�b���୫�ƨϥ�

// SDC00 set File Control Flag:
#define SDC_FCF_CLOSE			0x01
#define SDC_FCF_READ			0x02
#define SDC_FCF_OVERWRITE		0x04
#define SDC_FCF_CONTINUEWRITE	0x08
#define SDC_FCF_READPARAMETER	0x10
#define SDC_FCF_RENAME			0x20

/**** Internal Function Prototypes ****/

int RTC_init(void);

/**** Global Variables ****/
FATFS FatFs[FF_VOLUMES];		// File system object for each logical drive
FILINFO Finfo;
FIL FileObj;				// File object
DIR DirObj;					// Directory object

char FileName[13] = "DAT00000.BIN";
char ASA_DATA_Dir[9] = "ASA_DATA";
unsigned int FileID = 0;
char SDC_State = 0;
char ASA_SDC00_ID = 44;		// Have SDC:0~7, No SDC:44
static char SDC_Init_Success_Flag = 0;
static char ASA_RTC00_ID = 44;		// Have RTC:0~7, No RTC:44
static char RTC_Init_Success_Flag = 0;
unsigned long EndPointOfFile = 0;
unsigned int LastFileIndex = 0;

//Functions

char ASA_ID_check(unsigned char ASA_ID)
{
	if(ASA_ID>=0 && ASA_ID<=7)
	return 0;
	
	else
	return 1;
}

char ASA_ID_set(unsigned char ASA_ID)
{
	if(ASA_ID_check(ASA_ID)==0)
	{
		unsigned char ASA_ID_temp;
		
		ASA_ID_temp = ADDR_PORT;
		ASA_ID_temp &= ~(0x07 << ADDR0);
		ASA_ID_temp |= ((ASA_ID & 0x07) << ADDR0);
		ADDR_PORT  = ASA_ID_temp;
		
		return 0;
	}
	
	else
	return 1;
}

/**** Functions ****/
void ASA_SDC00_Select(char ASA_ID)
{
	PORTB |= 0b00000101;	// Configure MOSI/SCK/CS as output
	DDRB  |= 0b00000111;
	SPCR = 0x52;			// Enable SPI function in mode 0
	SPSR = 0x01;			// SPI 2x mode
	ASA_ID_set(ASA_ID);
}
void ASA_SDC00_Deselect(void)
{
	ASA_ID_set(0);
	SPCR = 0;				// Disable SPI function
	DDRB  &= ~0b00000111;	// Set MOSI/SCK/CS as hi-z
	PORTB &= ~0b00000111;
	PORTB |=  0b00000000;
}

char CheckSysInfoAndReadLastFileIndex(void)
{
	char SysInfoIDTemp[2];
	unsigned int BytesTemp;
	if( f_open(&FileObj, "ASYSINFO.ASI", FA_READ) != FR_OK )	// �}��Ū�t����
	{
		// �Y�t���ɤ��s�b�A�h�۰ʲ��ͤ@�ӷs���t����
		if( f_open(&FileObj, "ASYSINFO.ASI", FA_CREATE_ALWAYS) != FR_OK )
		return ARSDC_SDC_STATE_ERR;
		LastFileIndex = 3;
		SysInfoIDTemp[1] = LastFileIndex/256;
		SysInfoIDTemp[0] = LastFileIndex%256;
		if( f_open(&FileObj, "ASYSINFO.ASI", FA_WRITE) )		// �}�ɼg�t����
		return ARSDC_SDC_STATE_ERR;
		if( f_write(&FileObj, SysInfoIDTemp, 2, &BytesTemp) )		// �ק�t���ɤw�άy���Ǹ�
		return ARSDC_SDC_STATE_ERR;
		f_close(&FileObj);
		if( f_chmod("ASYSINFO.ASI", 0x07, 0xFF) )			// �]�w��Ū
		return ARSDC_SDC_STATE_ERR;
	}
	else
	{
		if( f_read(&FileObj, SysInfoIDTemp, 2, &BytesTemp) != FR_OK )
		return ARSDC_SDC_STATE_ERR;
		f_close(&FileObj);
		LastFileIndex = (unsigned int)SysInfoIDTemp[1]*256 + SysInfoIDTemp[0];
	}
	return ARSDC_OK;
}

char ASA_SDC00_Init(void)
{
	char res;	// 0:All OK, 1:SD Not OK, 2:RTC Not OK, 3:All Not OK.
	char i;
	// TODO: Add RTC Library and enable these code
	//// ���ˬd�O�_��RTC00�i�Ѱt�X���Ѯɶ�
	//if( RTC_Init_Success_Flag == 0 )			// �Ĥ@���Q�I�s�]�w�ɶi��RTC��l��
	//{
		//for(i=0; i<8; i++)						// �]�@��for loop�۰ʰ���RTC ID�s��
		//{
			//ASA_ID_set(i);
			//if( RTC_init() == 1 )
			//{
				//RTC_Init_Success_Flag = 1;
				//ASA_RTC00_ID = i;
				//break;
			//}
		//}
		////	if( ASA_RTC00_ID == 44 )
		////		res = 2;
		////return ARRTC_INVALID_ERR;
	//}


	ASA_SDC00_Select(0);

	for(i=0; i<8; i++)										// �]�@��for loop�۰ʰ���SDC ID�s��
	{
		res = 0;
		ASA_ID_set(i);
		
		if( f_mount(&FatFs[0], "0:", 0) != FR_OK )			// SD card mount a volume
		{
			res = 1;
			continue;
		}

		if( f_chdir(ASA_DATA_Dir) != FR_OK )				// �������|��ASA�w�]��Ƨ�
		{
			f_mkdir(ASA_DATA_Dir);							// �Y�w�]��Ƨ����s�b�h�إߤ@�ӷs��
			if( f_chdir(ASA_DATA_Dir) != FR_OK )			// �A�����L�h�@��
			res += 1;
			else
			{
				if( CheckSysInfoAndReadLastFileIndex() != ARSDC_OK )
				return ARSDC_SDC_STATE_ERR;
				ASA_SDC00_ID = i;
				ASA_DATA_Dir[0] = '\0';					// ���L�h��N�w�]���|�]����
				break;
			}
		}
		else
		{
			if( CheckSysInfoAndReadLastFileIndex() != ARSDC_OK )
			return ARSDC_SDC_STATE_ERR;
			ASA_SDC00_ID = i;
			ASA_DATA_Dir[0] = '\0';						// ���L�h��N�w�]���|�]����
			break;
		}
	}

	ASA_SDC00_Deselect();

	return res;
}

char FileIDtoNameConvert(int ID, char* Name)
{
	if( f_opendir(&DirObj, ASA_DATA_Dir) != FR_OK )
	return 1;
	int obj_index = 0;
	while(1)
	{
		if( f_readdir(&DirObj, &Finfo) != FR_OK )	// SDŪ�g�q�T���`
			return 2;
		
		if( !Finfo.fname[0] )						// �Yfname���ťN��w�g�S���ɮץiŪ��
			return 3;
		if(Finfo.fattrib & AM_DIR)
			;
		else if(Finfo.fattrib & AM_RDO || Finfo.fattrib & AM_HID || Finfo.fattrib & AM_SYS)
			;
		else {
			if( obj_index == ID )
			{
				sprintf(Name, "%s", Finfo.fname);	// �O�U�ɮצW��
				EndPointOfFile = Finfo.fsize;		// �O�U�ɮפj�p���ɧ�
				return ARSDC_OK;
			}
		}
		obj_index++; // Manual count-up index ( new version FatFs without DRI.index member )
	}
}


char ASA_SDC00_set(char ASA_ID, char LSByte, char Mask, char shift, char Data)
{
	char set_Data = 0, SysInfoIDTemp[2];
	unsigned int BytesTemp;
	printf("SDC00 initial set! \n");
	if( SDC_Init_Success_Flag == 0 )			// �Ĥ@���Q�I�s�]�w�ɶi��SDC��l��
	{
		if( ASA_SDC00_Init() == 0 )
		SDC_Init_Success_Flag = 1;
		else
		return ARSDC_SDC_STATE_ERR;
	}
	if( ASA_SDC00_ID != ASA_ID )	// ASA_ID���~
	return ARSDC_ID_ERR;
	if( shift > 7 || shift < 0 )		// Shift���~
	return ARSDC_SHIFT_ERR;
	set_Data = (set_Data & (~Mask)) | ((Data<<shift) & Mask);
	ASA_SDC00_Select(ASA_ID);
	switch(LSByte)
	{
		case 200:
		switch(set_Data)
		{
			case SDC_FCF_CLOSE:			// ����
			f_close(&FileObj);
			SDC_State = 0;
			break;
			
			case SDC_FCF_READ:			// �}�ɤ�Ū
			if( FileIDtoNameConvert(FileID, FileName) != ARSDC_OK )
			return ARSDC_OPEN_FILE_ERR;
			if( f_open(&FileObj, FileName, FA_READ) != FR_OK )
			return ARSDC_OPEN_FILE_ERR;
			SDC_State = 1;
			break;
			
			case SDC_FCF_OVERWRITE:		// �}�ɤ��\�g/�Y�ɮ׽s���]��0�h�}�s�ɮ�
			case SDC_FCF_CONTINUEWRITE:	// �}�ɤ���g/�Y�ɮ׽s���]��0�h�}�s�ɮ�
			// �Y�@�}�l�N�n�}�s�ɮסA�h���w�ɮ׽s��ID��0(�w�]�O�d���۰ʩR�W�}�s��)�Y�i
			if( FileIDtoNameConvert(FileID, FileName) != ARSDC_OK )				// ��ID�h�����}�@�����|��X�����ɦW��A�}��
			{
				if( FileID == 0 )												// �Y���wID��0�۰ʶ}�s�ɮ�
				{
					// �۰ʲ��ͷs�ɦW�ö}�s�ɮ�
					sprintf(FileName, "DAT%05d.BIN", LastFileIndex+1);
					if( f_open(&FileObj, FileName, FA_CREATE_ALWAYS) != FR_OK )	// �}�s�ɮ�
					return ARSDC_SDC_STATE_ERR;
					f_close(&FileObj);

					// �p�G�۰ʲ��ͪ��s�ɦW�}�ɦ��\�A�h�ק�t���ɰO�����ɦW�y���Ǹ�
					LastFileIndex++;
					SysInfoIDTemp[1] = LastFileIndex/256;
					SysInfoIDTemp[0] = LastFileIndex%256;
					if( f_chmod("ASYSINFO.ASI", 0x06, 0xFF) )				// �Ѱ���Ū
					return ARSDC_SDC_STATE_ERR;
					if( f_open(&FileObj, "ASYSINFO.ASI", FA_WRITE) )		// �}�ɼg�t����
					return ARSDC_SDC_STATE_ERR;
					if( f_write(&FileObj, SysInfoIDTemp, 2, &BytesTemp) )	// �ק�t���ɤw�άy���Ǹ�
					return ARSDC_SDC_STATE_ERR;
					f_close(&FileObj);
					if( f_chmod("ASYSINFO.ASI", 0x07, 0xFF) )				// �]�w��Ū
					return ARSDC_SDC_STATE_ERR;

					if( f_open(&FileObj, FileName, FA_WRITE) != FR_OK )			// �}�ɳ]���g
					return ARSDC_SDC_STATE_ERR;
				}
				else
				return ARSDC_OPEN_FILE_ERR;
			}
			else
			{
				if( set_Data == SDC_FCF_OVERWRITE )								// �Y�]���}�ɻ\�g
				{
					if( f_open(&FileObj, FileName, FA_CREATE_ALWAYS) != FR_OK )	// �j��}�s�ɮ׻\�g
					return ARSDC_SDC_STATE_ERR;
					if( f_open(&FileObj, FileName, FA_WRITE) != FR_OK )			// �}�ɳ]���g
					return ARSDC_SDC_STATE_ERR;
				}
				else	// if( set_Data == SDC_FCF_CONTINUEWRITE )				// �Y�]���}����g
				{
					if( f_open(&FileObj, FileName, FA_WRITE) != FR_OK )			// �}�ɳ]���g
					return ARSDC_SDC_STATE_ERR;
					if( f_lseek(&FileObj, EndPointOfFile) != FR_OK )			// �N��Ʀ�m���в����ɧ���g
					return ARSDC_SDC_STATE_ERR;
				}
			}
			SDC_State = 2;
			break;
			
			case SDC_FCF_READPARAMETER:		// Ū��
			if( f_opendir(&DirObj, ASA_DATA_Dir) != FR_OK )
			return ARSDC_SDC_STATE_ERR;
			SDC_State = 4;
			break;
			
			case SDC_FCF_RENAME:			// ��W
			if( FileIDtoNameConvert(FileID, FileName) != ARSDC_OK )					// ��ID�h�����}�@�����|��X�����ɦW��A�}��
			return ARSDC_RENAME_ERR;
			SDC_State = 5;
			break;
			
			default:	// �P�ɳ]�w��ӥH�W�X�СA�^�����~
			return ARSDC_MUTI_SET_ERR;
			break;
		}
		break;
		case 201:	// �ɮ׽s���������g�CByte�A�g��Byte
		FileID = (unsigned char)set_Data;
		break;
		case 202:
		FileID += (unsigned char)set_Data;
		break;
		default:	// LSByte���~�A���b�]�w�Ȧs���C��LSB200~202���d��
		return ARSDC_LSBYTE_ERR;
		break;
	}

	ASA_SDC00_Deselect();

	return ARSDC_OK;
}

char ASA_SDC00_put(char ASA_ID, char LSByte, char Bytes, void *Data_p)
{
	char FileNameNew[13];
	unsigned int WriteBytes, i;

	if( LSByte == 200 )						// �]�w�ɮױ���X�мȦs��
	return ASA_SDC00_set(ASA_ID, LSByte, 0xFF, 0x00, (char)((char*)Data_p)[0]);
	if( LSByte == 202 || LSByte == 201 )	// �]�w�ɮ׽s���Ȧs��
	{
		if( LSByte == 201 )
		{
			if( Bytes == 1 )
			FileID = (unsigned char)((char*)Data_p)[0];
			if( Bytes == 2 )
			{
				FileID = (unsigned char)((char*)Data_p)[0];
				FileID += (unsigned char)((char*)Data_p)[1];
			}
		}
		else
		{
			if( Bytes == 1 )
			FileID += (unsigned char)((char*)Data_p)[1];
			if( Bytes == 2 )
			return ARSDC_BYTES_ERR;
		}
		return ARSDC_OK;
	}
	if( SDC_Init_Success_Flag == 0 )			// �Y�S��l�ƪ�}�ɷ|����
	return ARSDC_SDC_STATE_ERR;
	if( ASA_SDC00_ID != ASA_ID )
	return ARSDC_ID_ERR;
	if( LSByte > 1 || LSByte < 0 )		// ��X�Ȧs��
	return ARSDC_LSBYTE_ERR;
	if( LSByte == 0 )
	if( Bytes > 64 || Bytes < 0 )
	return ARSDC_BYTES_ERR;
	if( LSByte == 1 )
	if( Bytes > 8 || Bytes < 0 )
	return ARSDC_BYTES_ERR;

	ASA_SDC00_Select(ASA_ID);
	
	switch(SDC_State)
	{
		case 0:	// ����
		return ARSDC_NO_OPEN_FILE;
		break;
		case 1:	// �}�ɤ�Ū
		return ARSDC_READ_SETTING;
		break;
		case 2:	// �}�ɤ��\�g/��g
		if( LSByte == 0 )
		{
			if( f_write(&FileObj, Data_p, Bytes, &WriteBytes) != FR_OK )
			return ARSDC_SDC_STATE_ERR;
		}
		else
		return ARSDC_LSBYTE_ERR;
		break;
		case 4:	// Ū��
		return ARSDC_LIST_READ_ONLY;
		break;
		case 5:	// ��W
		if( LSByte == 1 )
		{
			for(i=0; i<Bytes; i++)
			FileNameNew[i] = ((char*)Data_p)[i];
			sprintf(&FileNameNew[i], ".BIN");
			if( f_rename(FileName, FileNameNew) != FR_OK )
			return ARSDC_RENAME_ERR;
		}
		else
		return ARSDC_LSBYTE_ERR;
		break;
	}

	ASA_SDC00_Deselect();

	return 0;
}

char ASA_SDC00_get(char ASA_ID, char LSByte, char Bytes, void *Data_p)
{
	unsigned int ReadBytes;
	int obj_index = 0;

	if( SDC_Init_Success_Flag == 0 )			// �Y�S��l�ƪ�}�ɷ|����
	return ARSDC_SDC_STATE_ERR;
	if( ASA_SDC00_ID != ASA_ID )
	return ARSDC_ID_ERR;
	if( LSByte > 101 || LSByte < 100 )
	return ARSDC_LSBYTE_ERR;
	if( LSByte == 100 )
	if( Bytes > 64 || Bytes < 0 )
	return ARSDC_BYTES_ERR;
	if( LSByte == 101 )
	if( Bytes > 47 || Bytes < 0 )
	return ARSDC_BYTES_ERR;

	ASA_SDC00_Select(ASA_ID);

	switch(SDC_State)
	{
		case 0:	// ����
		return ARSDC_NO_OPEN_FILE;
		break;
		case 1:	// �}�ɤ�Ū
		if( LSByte == 100 )
		{
			if( f_read(&FileObj, Data_p, Bytes, &ReadBytes) != FR_OK )
			return ARSDC_SDC_STATE_ERR;
			if(Bytes != ReadBytes)
			{
				if(ReadBytes == 0)
				return ARSDC_NO_DATA_TO_READ;
				else
				{
					((char*)Data_p)[(int)Bytes-1] = ReadBytes;		// Ū����Ƥ����]�w���ƶq�A�N�u��Ū�X�ƶq��J��ƽw�İϳ̫�@��byte
					return ARSDC_DATA_NUM_ENOUGH;
				}
			}
		}
		else
		return ARSDC_LSBYTE_ERR;
		break;
		case 2:	// �}�ɤ��\�g/��g
		return ARSDC_WRITE_SETTING;
		break;
		case 4:	// Ū��
		if( LSByte == 101 )
		while(1)
		{
			if( f_readdir(&DirObj, &Finfo) != FR_OK )
			return ARSDC_SDC_STATE_ERR;
			if( !Finfo.fname[0] )	// �Yfname���ťN��w�g�S���ɮץiŪ��
			return ARSDC_NO_DATA_TO_READ;
			if(Finfo.fattrib & AM_DIR)
			;
			else if(Finfo.fattrib & AM_RDO || Finfo.fattrib & AM_HID || Finfo.fattrib & AM_SYS)
			;
			else
			{
				sprintf(Data_p, " %5u %12s %u/%02u/%02u %02u:%02u %9lu",
				obj_index++,	// �ɮ׽s��
				Finfo.fname,	// �ɮצW��
				(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,	// �ɮפ��
				(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63,	// �ɮ׮ɶ�
				Finfo.fsize);	// �ɮפj�p
				break;
			}
		}
		else
		return ARSDC_LSBYTE_ERR;
		break;
		case 5:	// ��W
		return ARSDC_WRITE_SETTING;
		break;
	}

	ASA_SDC00_Deselect();

	return 0;
}
