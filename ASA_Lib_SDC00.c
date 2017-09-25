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
#define ARSDC_RENAME_ERR 15			// 指定檔案編號不存在或檔名已存在不能重複使用

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
	if( f_open(&FileObj, "ASYSINFO.ASI", FA_READ) != FR_OK )	// 開檔讀系統檔
	{
		// 若系統檔不存在，則自動產生一個新的系統檔
		if( f_open(&FileObj, "ASYSINFO.ASI", FA_CREATE_ALWAYS) != FR_OK )
		return ARSDC_SDC_STATE_ERR;
		LastFileIndex = 3;
		SysInfoIDTemp[1] = LastFileIndex/256;
		SysInfoIDTemp[0] = LastFileIndex%256;
		if( f_open(&FileObj, "ASYSINFO.ASI", FA_WRITE) )		// 開檔寫系統檔
		return ARSDC_SDC_STATE_ERR;
		if( f_write(&FileObj, SysInfoIDTemp, 2, &BytesTemp) )		// 修改系統檔已用流水序號
		return ARSDC_SDC_STATE_ERR;
		f_close(&FileObj);
		if( f_chmod("ASYSINFO.ASI", 0x07, 0xFF) )			// 設定唯讀
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
	//// 先檢查是否有RTC00可供配合提供時間
	//if( RTC_Init_Success_Flag == 0 )			// 第一次被呼叫設定時進行RTC初始化
	//{
		//for(i=0; i<8; i++)						// 跑一趟for loop自動偵測RTC ID編號
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

	for(i=0; i<8; i++)										// 跑一趟for loop自動偵測SDC ID編號
	{
		res = 0;
		ASA_ID_set(i);
		
		if( f_mount(&FatFs[0], "0:", 0) != FR_OK )			// SD card mount a volume
		{
			res = 1;
			continue;
		}

		if( f_chdir(ASA_DATA_Dir) != FR_OK )				// 切換路徑至ASA預設資料夾
		{
			f_mkdir(ASA_DATA_Dir);							// 若預設資料夾不存在則建立一個新的
			if( f_chdir(ASA_DATA_Dir) != FR_OK )			// 再切換過去一次
			res += 1;
			else
			{
				if( CheckSysInfoAndReadLastFileIndex() != ARSDC_OK )
				return ARSDC_SDC_STATE_ERR;
				ASA_SDC00_ID = i;
				ASA_DATA_Dir[0] = '\0';					// 切過去後將預設路徑設為空
				break;
			}
		}
		else
		{
			if( CheckSysInfoAndReadLastFileIndex() != ARSDC_OK )
			return ARSDC_SDC_STATE_ERR;
			ASA_SDC00_ID = i;
			ASA_DATA_Dir[0] = '\0';						// 切過去後將預設路徑設為空
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
		if( f_readdir(&DirObj, &Finfo) != FR_OK )	// SD讀寫通訊異常
			return 2;
		
		if( !Finfo.fname[0] )						// 若fname為空代表已經沒有檔案可讀參
			return 3;
		if(Finfo.fattrib & AM_DIR)
			;
		else if(Finfo.fattrib & AM_RDO || Finfo.fattrib & AM_HID || Finfo.fattrib & AM_SYS)
			;
		else {
			if( obj_index == ID )
			{
				sprintf(Name, "%s", Finfo.fname);	// 記下檔案名稱
				EndPointOfFile = Finfo.fsize;		// 記下檔案大小當成檔尾
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
	if( SDC_Init_Success_Flag == 0 )			// 第一次被呼叫設定時進行SDC初始化
	{
		if( ASA_SDC00_Init() == 0 )
		SDC_Init_Success_Flag = 1;
		else
		return ARSDC_SDC_STATE_ERR;
	}
	if( ASA_SDC00_ID != ASA_ID )	// ASA_ID錯誤
	return ARSDC_ID_ERR;
	if( shift > 7 || shift < 0 )		// Shift錯誤
	return ARSDC_SHIFT_ERR;
	set_Data = (set_Data & (~Mask)) | ((Data<<shift) & Mask);
	ASA_SDC00_Select(ASA_ID);
	switch(LSByte)
	{
		case 200:
		switch(set_Data)
		{
			case SDC_FCF_CLOSE:			// 關檔
			f_close(&FileObj);
			SDC_State = 0;
			break;
			
			case SDC_FCF_READ:			// 開檔允讀
			if( FileIDtoNameConvert(FileID, FileName) != ARSDC_OK )
			return ARSDC_OPEN_FILE_ERR;
			if( f_open(&FileObj, FileName, FA_READ) != FR_OK )
			return ARSDC_OPEN_FILE_ERR;
			SDC_State = 1;
			break;
			
			case SDC_FCF_OVERWRITE:		// 開檔允蓋寫/若檔案編號設為0則開新檔案
			case SDC_FCF_CONTINUEWRITE:	// 開檔允續寫/若檔案編號設為0則開新檔案
			// 若一開始就要開新檔案，則指定檔案編號ID為0(預設保留給自動命名開新檔)即可
			if( FileIDtoNameConvert(FileID, FileName) != ARSDC_OK )				// 用ID去全部開一次路徑找出對應檔名後再開檔
			{
				if( FileID == 0 )												// 若指定ID為0自動開新檔案
				{
					// 自動產生新檔名並開新檔案
					sprintf(FileName, "DAT%05d.BIN", LastFileIndex+1);
					if( f_open(&FileObj, FileName, FA_CREATE_ALWAYS) != FR_OK )	// 開新檔案
					return ARSDC_SDC_STATE_ERR;
					f_close(&FileObj);

					// 如果自動產生的新檔名開檔成功，則修改系統檔記錄之檔名流水序號
					LastFileIndex++;
					SysInfoIDTemp[1] = LastFileIndex/256;
					SysInfoIDTemp[0] = LastFileIndex%256;
					if( f_chmod("ASYSINFO.ASI", 0x06, 0xFF) )				// 解除唯讀
					return ARSDC_SDC_STATE_ERR;
					if( f_open(&FileObj, "ASYSINFO.ASI", FA_WRITE) )		// 開檔寫系統檔
					return ARSDC_SDC_STATE_ERR;
					if( f_write(&FileObj, SysInfoIDTemp, 2, &BytesTemp) )	// 修改系統檔已用流水序號
					return ARSDC_SDC_STATE_ERR;
					f_close(&FileObj);
					if( f_chmod("ASYSINFO.ASI", 0x07, 0xFF) )				// 設定唯讀
					return ARSDC_SDC_STATE_ERR;

					if( f_open(&FileObj, FileName, FA_WRITE) != FR_OK )			// 開檔設為寫
					return ARSDC_SDC_STATE_ERR;
				}
				else
				return ARSDC_OPEN_FILE_ERR;
			}
			else
			{
				if( set_Data == SDC_FCF_OVERWRITE )								// 若設為開檔蓋寫
				{
					if( f_open(&FileObj, FileName, FA_CREATE_ALWAYS) != FR_OK )	// 強制開新檔案蓋寫
					return ARSDC_SDC_STATE_ERR;
					if( f_open(&FileObj, FileName, FA_WRITE) != FR_OK )			// 開檔設為寫
					return ARSDC_SDC_STATE_ERR;
				}
				else	// if( set_Data == SDC_FCF_CONTINUEWRITE )				// 若設為開檔續寫
				{
					if( f_open(&FileObj, FileName, FA_WRITE) != FR_OK )			// 開檔設為寫
					return ARSDC_SDC_STATE_ERR;
					if( f_lseek(&FileObj, EndPointOfFile) != FR_OK )			// 將資料位置指標移到檔尾續寫
					return ARSDC_SDC_STATE_ERR;
				}
			}
			SDC_State = 2;
			break;
			
			case SDC_FCF_READPARAMETER:		// 讀參
			if( f_opendir(&DirObj, ASA_DATA_Dir) != FR_OK )
			return ARSDC_SDC_STATE_ERR;
			SDC_State = 4;
			break;
			
			case SDC_FCF_RENAME:			// 改名
			if( FileIDtoNameConvert(FileID, FileName) != ARSDC_OK )					// 用ID去全部開一次路徑找出對應檔名後再開檔
			return ARSDC_RENAME_ERR;
			SDC_State = 5;
			break;
			
			default:	// 同時設定兩個以上旗標，回應錯誤
			return ARSDC_MUTI_SET_ERR;
			break;
		}
		break;
		case 201:	// 檔案編號必須先寫低Byte再寫高Byte
		FileID = (unsigned char)set_Data;
		break;
		case 202:
		FileID += (unsigned char)set_Data;
		break;
		default:	// LSByte錯誤，不在設定暫存器列表LSB200~202的範圍內
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

	if( LSByte == 200 )						// 設定檔案控制旗標暫存器
	return ASA_SDC00_set(ASA_ID, LSByte, 0xFF, 0x00, (char)((char*)Data_p)[0]);
	if( LSByte == 202 || LSByte == 201 )	// 設定檔案編號暫存器
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
	if( SDC_Init_Success_Flag == 0 )			// 若沒初始化表開檔會失敗
	return ARSDC_SDC_STATE_ERR;
	if( ASA_SDC00_ID != ASA_ID )
	return ARSDC_ID_ERR;
	if( LSByte > 1 || LSByte < 0 )		// 輸出暫存器
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
		case 0:	// 關檔
		return ARSDC_NO_OPEN_FILE;
		break;
		case 1:	// 開檔允讀
		return ARSDC_READ_SETTING;
		break;
		case 2:	// 開檔允蓋寫/續寫
		if( LSByte == 0 )
		{
			if( f_write(&FileObj, Data_p, Bytes, &WriteBytes) != FR_OK )
			return ARSDC_SDC_STATE_ERR;
		}
		else
		return ARSDC_LSBYTE_ERR;
		break;
		case 4:	// 讀參
		return ARSDC_LIST_READ_ONLY;
		break;
		case 5:	// 改名
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

	if( SDC_Init_Success_Flag == 0 )			// 若沒初始化表開檔會失敗
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
		case 0:	// 關檔
		return ARSDC_NO_OPEN_FILE;
		break;
		case 1:	// 開檔允讀
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
					((char*)Data_p)[(int)Bytes-1] = ReadBytes;		// 讀取資料不足設定的數量，將真實讀出數量填入資料緩衝區最後一個byte
					return ARSDC_DATA_NUM_ENOUGH;
				}
			}
		}
		else
		return ARSDC_LSBYTE_ERR;
		break;
		case 2:	// 開檔允蓋寫/續寫
		return ARSDC_WRITE_SETTING;
		break;
		case 4:	// 讀參
		if( LSByte == 101 )
		while(1)
		{
			if( f_readdir(&DirObj, &Finfo) != FR_OK )
			return ARSDC_SDC_STATE_ERR;
			if( !Finfo.fname[0] )	// 若fname為空代表已經沒有檔案可讀參
			return ARSDC_NO_DATA_TO_READ;
			if(Finfo.fattrib & AM_DIR)
			;
			else if(Finfo.fattrib & AM_RDO || Finfo.fattrib & AM_HID || Finfo.fattrib & AM_SYS)
			;
			else
			{
				sprintf(Data_p, " %5u %12s %u/%02u/%02u %02u:%02u %9lu",
				obj_index++,	// 檔案編號
				Finfo.fname,	// 檔案名稱
				(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,	// 檔案日期
				(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63,	// 檔案時間
				Finfo.fsize);	// 檔案大小
				break;
			}
		}
		else
		return ARSDC_LSBYTE_ERR;
		break;
		case 5:	// 改名
		return ARSDC_WRITE_SETTING;
		break;
	}

	ASA_SDC00_Deselect();

	return 0;
}
