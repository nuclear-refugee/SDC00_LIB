//Author    : Name
//Last Edit : Name YYYY/MM/DD
//ASA Lib Gen2

#include "ASA_Lib.h"
#include "ASA_Core_M128.h"
#include "ff.h"

#define _DEBUG_INFO

/**** Internal Definitions ****/

// SDC00 set File Control Flag:
#define SDC_FCF_CLOSE			0x00
#define SDC_FCF_READ			0x01
#define SDC_FCF_OVERWRITE		0x05
#define SDC_FCF_CONTINUEWRITE	0x07
//#define SDC_FCF_READPARAMETER		0x10
//#define SDC_FCF_RENAME			0x20

// ASA SDC Return Error Code Table
#define ARSDC_OK				0
#define ARSDC_ID_ERR			1
#define ARSDC_LSBYTE_ERR		2
#define ARSDC_SHIFT_ERR			3
#define ARSDC_MUTI_SET_ERR		4
#define ARSDC_FILE_ID_ERR		5
#define ARSDC_OPEN_FILE_ERR		6
#define ARSDC_SDC_STATE_ERR		7
#define ARSDC_BYTES_ERR			8
#define ARSDC_NO_OPEN_FILE		9
#define ARSDC_READ_SETTING		10
#define ARSDC_LIST_READ_ONLY	11
#define ARSDC_WRITE_SETTING		12
#define ARSDC_NO_DATA_TO_READ	13
#define ARSDC_DATA_NUM_ENOUGH	14
#define ARSDC_RENAME_ERR		15			// 指定檔案編號不存在或檔名已存在不能重複使用

/**** SDC00 LSByte definition ****/
// Register address
#define LSBYTE_DATA_SWAP_BUF	0
#define LSBYTE_NAME_BUF			64
#define LSBYTE_NAME_EXT_BUF		72
#define LSBYTE_ATTR_ACCESS		75
#define LSBYTE_ATTR_TIME		76
#define LSBYTE_ATTR_DATE		78
#define LSBYTE_CLUSTER			80
#define LSBYTE_FILE_SZ			82
#define LSBYTE_FILE_SERIAL_ID	86
#define LSBYTE_MODE				200

// Register size
#define SZ_DATA_SWAP_BUF		64
#define SZ_NAME_BUF				8
#define SZ_NAME_EXT_BUF			3
#define SZ_ATTR_ACCESS			1
#define SZ_ATTR_TIME			2
#define SZ_ATTR_DATE			2
#define SZ_CLUSTER				2
#define SZ_FILE_SZ				4
#define SZ_FILE_SERIAL_ID		2
#define SZ_FILE_MODE			1

/**** Internal Function Prototypes ****/

int RTC_init(void);

/**** Global Variables ****/
FATFS FatFs[FF_VOLUMES];		// File system object for each logical drive
FILINFO Finfo;
FIL FileObj;				// File object
DIR DirObj;					// Directory object

char FileName[13];
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
FRESULT check_info(char* file_name, FILINFO* p_fno);
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
	if( f_open(&FileObj, "ASYSINFO.ASI", FA_OPEN_EXISTING | FA_READ) != FR_OK )	// 開檔讀系統檔
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
		FRESULT res = f_read(&FileObj, SysInfoIDTemp, 2, &BytesTemp);
		if( res != FR_OK ) {
			printf("Read ASYSINFO.ASI failed, error code <%d> \n", res);
			return ARSDC_SDC_STATE_ERR;
		}
		
		f_close(&FileObj);
		LastFileIndex = (unsigned int)SysInfoIDTemp[1]*256 + SysInfoIDTemp[0];
		printf("LastFileIndex  = %d\n", LastFileIndex);
	}
	return ARSDC_OK;
}

char ASA_SDC00_Init(char ASA_ID)
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

	_delay_ms(100);

	res = 0;
#ifdef _DEBUG_INFO
	printf("Set ASA ID :%d \n", ASA_ID);
#endif
	ASA_ID_set(ASA_ID);
	
	if( f_mount(&FatFs[0], "", 1) != FR_OK )			// SD card mount a volume
	{
#ifdef _DEBUG_INFO
		printf("Mount ID<%d> Failed\n", ASA_ID);
#endif
		res = 1;
		return res;
	}
#ifdef _DEBUG_INFO
	else {
		printf("Mount ID<%d> successful\n", ASA_ID);
	}
#endif

	if( f_chdir(ASA_DATA_Dir) != FR_OK )				// 切換路徑至ASA預設資料夾
	{
		f_mkdir(ASA_DATA_Dir);							// 若預設資料夾不存在則建立一個新的
		if( f_chdir(ASA_DATA_Dir) != FR_OK )			// 再切換過去一次
		res += 1;
		else
		{
			if( CheckSysInfoAndReadLastFileIndex() != ARSDC_OK )
			return ARSDC_SDC_STATE_ERR;
			ASA_SDC00_ID = ASA_ID;
			ASA_DATA_Dir[0] = '\0';					// 切過去後將預設路徑設為空
		}
	}
	else
	{
		f_chdir("\\");
		if( CheckSysInfoAndReadLastFileIndex() != ARSDC_OK )
		return ARSDC_SDC_STATE_ERR;
		f_chdir(ASA_DATA_Dir);
		ASA_SDC00_ID = ASA_ID;
		ASA_DATA_Dir[0] = '\0';						// 切過去後將預設路徑設為空
	}

	ASA_SDC00_Deselect();
#ifdef _DEBUG_INFO
	printf("<SDC00_Init> Initialize with result : %d \n", res);
#endif
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
	if( SDC_Init_Success_Flag == 0 )	// Initialize at first call
	{
		if( ASA_SDC00_Init(ASA_ID) == 0 )
		SDC_Init_Success_Flag = 1;
		else
		return ARSDC_SDC_STATE_ERR;
	}
	if( ASA_SDC00_ID != ASA_ID )	// Not a valid ASA_ID
	return ARSDC_ID_ERR;
	if( shift > 7 || shift < 0 )	// Not a valid shift value ( 0 <= shift <= 7 )
	return ARSDC_SHIFT_ERR;
	set_Data = (set_Data & (~Mask)) | ((Data<<shift) & Mask);
	ASA_SDC00_Select(ASA_ID);
	switch(LSByte)
	{
		case LSBYTE_MODE:
		switch(set_Data)
		{
			case SDC_FCF_CLOSE:			// Close file
				f_close(&FileObj);
				SDC_State = 0;
				break;
			
			case SDC_FCF_READ:			// Open File (Read only)
				// TODO: need to make sure there is "FileName already setup"
				if( f_open(&FileObj, FileName, FA_READ) != FR_OK )
					return ARSDC_OPEN_FILE_ERR;
				SDC_State = 1;
				break;
			
			case SDC_FCF_OVERWRITE:		// 若設為開檔蓋寫							
				if( f_open(&FileObj, FileName, FA_CREATE_ALWAYS) != FR_OK )	// 強制開新檔案蓋寫
					return ARSDC_SDC_STATE_ERR;
				if( f_open(&FileObj, FileName, FA_WRITE) != FR_OK )			// 開檔設為寫
					return ARSDC_SDC_STATE_ERR;
				SDC_State = 2;
				break;
			case SDC_FCF_CONTINUEWRITE:	// 開檔允續寫/若檔案編號設為0則開新檔案
				if( f_open(&FileObj, FileName, FA_WRITE) != FR_OK )			// 開檔設為寫
					return ARSDC_SDC_STATE_ERR;
				if( f_lseek(&FileObj, f_size(&FileObj)) != FR_OK )			// 將資料位置指標移到檔尾續寫
					return ARSDC_SDC_STATE_ERR;
				SDC_State = 2;
				break;
			
			default:	// 同時設定兩個以上旗標，回應錯誤
				return ARSDC_MUTI_SET_ERR;
		}
		break;
		default:	// LSByte錯誤，不在設定暫存器列表LSB200~200的範圍內
		return ARSDC_LSBYTE_ERR;
		break;

	}

	ASA_SDC00_Deselect();

	return ARSDC_OK;
}

char ASA_SDC00_put(char ASA_ID, char LSByte, char Bytes, void *Data_p)
{
	FRESULT res;
	char FileNameNew[13];
	unsigned int WriteBytes, i;

	if( LSByte == 200 )						// 設定檔案控制旗標暫存器
		return ASA_SDC00_set(ASA_ID, LSByte, 0xFF, 0x00, (char)((char*)Data_p)[0]);
	
	if( SDC_Init_Success_Flag == 0 )	// Initialize at first call
	{
		if( ASA_SDC00_Init(ASA_ID) == 0 )
		SDC_Init_Success_Flag = 1;
		else
		return ARSDC_SDC_STATE_ERR;
	}
	if( ASA_SDC00_ID != ASA_ID )
		return ARSDC_ID_ERR;
	if( LSByte == 0 )
		if( Bytes > 64 || Bytes < 0 )
			return ARSDC_BYTES_ERR;

	ASA_SDC00_Select(ASA_ID);
	
	static char name_buffer[8];
	static char name_ext_buffer[3];
#ifdef _DEBUG_INFO
	printf("<State> SDC status: %d\n", SDC_State);
#endif
	switch(SDC_State)
	{
		case 0:	// If current is in closed file mode
			switch(LSByte) {
			case LSBYTE_NAME_BUF:
				for(int i=0; i<SZ_NAME_BUF; i++) {
#ifdef _DEBUG_INFO
					printf("Namebuf[%d]: %c\n", i, ((char*)Data_p)[i]);
#endif
					// Copy each byte until EOF or At byte 7
					if(((char*)Data_p)[i] == '\0' || i ==(SZ_NAME_BUF-1) || i== Bytes) {
						name_buffer[i] = '\0';
						break;
					}
					name_buffer[i] = ((char*)Data_p)[i];
				}
				break;

			// Note: End of name_ext string is reach '\0' or Byte 2
			case LSBYTE_NAME_EXT_BUF:
				for(int i=0; i<SZ_NAME_EXT_BUF; i++) {
#ifdef _DEBUG_INFO
					printf("Nameextbuf[%d]: %c\n", i, ((char*)Data_p)[i]);
#endif
					// Copy each byte until EOF or At byte-2
					if(((char*)Data_p)[i] == '\0' || i==Bytes) {
						name_ext_buffer[i] = '\0';
						break;
					}
					name_ext_buffer[i] = ((char*)Data_p)[i];
				}
				break;
			default:
				return ARSDC_NO_OPEN_FILE;
			}
			break;
		case 1:	// If current is at Opened file mode (Read Only)
			return ARSDC_READ_SETTING;
			break;
		case 2:	// If current is at Opened file mode ([Overlap/Continue] Write mode)
			if( LSByte == 0 ) {
				// Try to write date to SDC00
#ifdef _DEBUG_INFO
				printf("<FAT Wrtie> Try to writing %d bytes data\n", Bytes);
				for(int i=0; i<Bytes; i++) {
					printf("%02x ", ((char*)Data_p)[i]);
					if((i+1)%8 == 0) {
						printf("\n");
					}
				}
				printf("\n");
				for(int i=0; i<Bytes; i++) {
					printf("%2c ", ((char*)Data_p)[i]);
					if((i+1)%8 == 0) {
						printf("\n");
					}
				}
#endif
				if(  res = f_write(&FileObj, Data_p, Bytes, &WriteBytes)) {
#ifdef _DEBUG_INFO
					printf("<FAT Write> write fail errorcode: %d\n", res);
#endif
					return ARSDC_SDC_STATE_ERR;

				}
#ifdef _DEBUG_INFO
				printf("<FAT Write> write result code: %d\n", res);
				printf("<FAT Write> write %d Bytes: %ul\n", WriteBytes);
#endif
			}
			else
				return ARSDC_LSBYTE_ERR;
			break;
	}

	ASA_SDC00_Deselect();

	// While in closed file mode, then update full filename
	if(SDC_State == 0) {
#ifdef _DEBUG_INFO
	printf("<Update Filename>\n");
	printf("name_buffer = %s\n", name_buffer);
	printf("name_ext_buf = %s\n", name_ext_buffer);
#endif
		// Check name_buffer actual size
		int8_t sz_name_buffer = -1;
		for(int i=0; i<8; i++) {
			if(name_buffer[i] == '\0') {
				sz_name_buffer = i;
				break;
			}
		}

		if(sz_name_buffer > 0) {
			for(int i=0; i<sz_name_buffer; i++) {
				FileName[i] = name_buffer[i];
			}
			FileName[sz_name_buffer] = '.';
			for(int i=0; i<3; i++) {
				FileName[sz_name_buffer+1+i] = name_ext_buffer[i];
			}
			FileName[sz_name_buffer+4] = '\0';
#ifdef _DEBUG_INFO
			printf("<Update FileName: %s\n", FileName);
#endif
		}
		
	}

	return 0;
}

char ASA_SDC00_get(char ASA_ID, char LSByte, char Bytes, void *Data_p)
{
	unsigned int ReadBytes;
	int obj_index = 0;
	char* p_data = Data_p;
	FRESULT fr;
	FILINFO fno;

	if( SDC_Init_Success_Flag == 0 )			// 若沒初始化表開檔會失敗
	return ARSDC_SDC_STATE_ERR;
	if( ASA_SDC00_ID != ASA_ID )
	return ARSDC_ID_ERR;
	// TODO: Modify valid LSByte section
	//if( LSByte > 101 || LSByte < 100 )
	//return ARSDC_LSBYTE_ERR;
	if( LSByte == LSBYTE_DATA_SWAP_BUF )
	if( Bytes > 64 || Bytes < 0 )
	return ARSDC_BYTES_ERR;

	ASA_SDC00_Select(ASA_ID);

	switch(SDC_State)
	{
		case 0:	// If current is in closed file mode
		return ARSDC_NO_OPEN_FILE;
		break;
		case 1:	// If current is at Opened file mode (Read Only)
			// Fetch file information first
			if(check_info(FileName, &fno) != FR_OK) {
				return ARSDC_OPEN_FILE_ERR;
			}

			switch(LSByte) {
			case LSBYTE_DATA_SWAP_BUF:
				if(f_read(&FileObj, p_data, Bytes, &ReadBytes) != FR_OK)
					return ARSDC_SDC_STATE_ERR;
				if(Bytes != ReadBytes) {
					return ARSDC_NO_DATA_TO_READ;
				}
				else {												// While read bytes less than user setting,
					((char*) p_data)[(int)Bytes-1] = ReadBytes; 	// then record received bytes at last byte in buffer
					return ARSDC_DATA_NUM_ENOUGH;
				}
				break;
			case LSBYTE_ATTR_TIME:
#ifdef _DEBUG_INFO
				printf("<temp Raw data> fno.ftime = %x\n", fno.ftime);
#endif
				p_data[0] = (uint8_t)(fno.ftime);
				p_data[1] = (uint8_t)(fno.ftime >> 8);
#ifdef _DEBUG_INFO
				printf("<temp Raw data> p_data[0] = %x\n", p_data[0]);
				printf("<temp Raw data> p_data[1] = %x\n", p_data[1]);
#endif
				break;
			case LSBYTE_ATTR_DATE:
#ifdef _DEBUG_INFO
				printf("<temp Raw data> fno.fdate = %x\n", fno.fdate);
#endif
				p_data[0] = (uint8_t)(fno.fdate);
				p_data[1] = (uint8_t)(fno.fdate >> 8);
#ifdef _DEBUG_INFO
				printf("<temp Raw data> p_data[0] = %x\n", p_data[0]);
				printf("<temp Raw data> p_data[1] = %x\n", p_data[1]);
#endif
				break;
			case LSBYTE_ATTR_ACCESS:
				if(fno.fattrib & AM_RDO)
					// forbidden writing
					p_data[0] = 1;
				else
					p_data[1] = 0;
				break;
			case LSBYTE_FILE_SZ:
				p_data[0] = fno.fsize;
				p_data[1] = fno.fsize >> 8;
				p_data[2] = fno.fsize >> 16;
				p_data[3] = fno.fsize >> 24;
				break;
			}

		case 2:	// If current is at Opened file mode ([Overlap/Continue] Write mode)
			return ARSDC_WRITE_SETTING;
		default:
			return ARSDC_LSBYTE_ERR;

	}

	ASA_SDC00_Deselect();

	return 0;
}

FRESULT check_info(char* file_name, FILINFO* p_fno) {

	char file_path[20];

	sprintf(file_path,"/%s/%s", "ASA_DATA", file_name);

	FRESULT fr;
#ifdef _DEBUG_INFO
	printf("Check info for '%s'...\n", file_path);
#endif

	// Check for file
	fr = f_stat(file_path, p_fno);
#ifdef _DEBUG_INFO
	switch (fr) {
		case FR_OK:
		printf("<Raw data>fdate = %x, ftime = %x\n",p_fno->fdate, p_fno->ftime);
		printf("Size: %lu\n", p_fno->fsize);
		printf("Timestamp: %u/%02u/%02u, %02u:%02u\n",
		(p_fno->fdate >> 9) + 1980, p_fno->fdate >> 5 & 15, p_fno->fdate & 31,
		p_fno->ftime >> 11, p_fno->ftime >> 5 & 63);
		printf("Attributes: %c%c%c%c%c\n",
		(p_fno->fattrib & AM_DIR) ? 'D' : '-',
		(p_fno->fattrib & AM_RDO) ? 'R' : '-',
		(p_fno->fattrib & AM_HID) ? 'H' : '-',
		(p_fno->fattrib & AM_SYS) ? 'S' : '-',
		(p_fno->fattrib & AM_ARC) ? 'A' : '-');
		break;

		case FR_NO_FILE:
		printf("It is not exist.\n");
		break;

		default:
		printf("An error occured. (%d)\n", fr);
	}
#endif

	return fr;
}
