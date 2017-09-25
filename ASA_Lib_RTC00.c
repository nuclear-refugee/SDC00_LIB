//Last Build : 2015/12/24 2pm
#include "ASA_Lib.h"
#include "ASA_Core_M128.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>

//Memory Definitions
#define set_memory_size 32
#define put_memory_size 32
#define get_memory_size 32

//Memory Union
typedef union{
	char set_memory[set_memory_size];
	char _KB00_set_reg[17];
	char _7S00_set_reg;
	char _PWM01_set_reg;
	char _THM00_set_reg;
	char _PWM00_set_reg;
	char _STP00_set_reg[2];
	char _RF00_set_reg;
	char _SDC00_set_reg[1];
	unsigned char _DAC00_set_reg;
	unsigned char _ADC00_set_reg;
	char _GR00_set_reg;
	char _ACC00_set_reg;
	char _THM01_set_reg;
	char _RTC00_set_reg;
}ASA_set_memory;

typedef union{
	char get_memory[get_memory_size];
	char _KB00_get_reg;
	double _THM00_get_reg;
	char _DIO00_get_reg;
	char _RF00_get_reg[32];
	char _GR00_get_reg[12];
	float _ACC00_get_reg[3];
	float _THM01_get_reg;
	char _RTC00_get_reg[17];
}ASA_get_memory;

typedef union{
	char put_memory[put_memory_size];
	char _7S00_put_reg[4];
	char _PWM01_put_reg[8];
	char _PWM00_put_reg[4];
	char _STP00_put_reg[4];
	char _DIO00_put_reg;
	char _RF00_put_reg[32];
	unsigned char _DAC00_put_reg[2];
	char _RTC00_put_reg[17];
}ASA_put_memory;

ASA_set_memory set_ID[8];
ASA_get_memory get_ID[8];
ASA_put_memory put_ID[8];

//Internal Definitions

typedef struct{
	WORD	year;	// 2000..2099
	BYTE	month;	// 1..12
	BYTE	mday;	// 1.. 31
	BYTE	wday;	// 1..7
	BYTE	hour;	// 0..23
	BYTE	min;	// 0..59
	BYTE	sec;	// 0..59
} RTC;

#define	TWI_INIT()	DDRD &= 0xFC; PORTD &= 0xFC	// Set SCL/SDA as hi-z
#define SCL_LOW()	DDRD |=	0x01				// SCL = LOW
#define SCL_HIGH()	DDRD &=	0xFE				// SCL = High-Z
#define	SCL_VAL		((PIND & 0x01) ? 1 : 0)		// SCL input level
#define SDA_LOW()	DDRD |=	0x02				// SDA = LOW
#define SDA_HIGH()	DDRD &=	0xFD				// SDA = High-Z
#define	SDA_VAL		((PIND & 0x02) ? 1 : 0)		// SDA input level

// ASA RTC Return Error Code Table
#define ARRTC_OK 0
#define ARRTC_ID_ERR 1
#define ARRTC_LSBYTE_ERR 2
#define ARRTC_SHIFT_ERR 3
#define ARRTC_BYTES_ERR 4
#define ARRTC_INVALID_ERR 5
#define ARRTC_STATE_ERR 6
#define ARRTC_VALUE_ERR 7

// 設定即時時鐘模式，提供兩個bit控制，
// 第 0 bit控制時間時制格式，0: 24Hr制(預設值)或 1: 12Hr制AM/PM
// 第 1 bit控制資料矩陣格式，0: ASCII字串(預設值)或 1: 數值資料串

//Internal Function Prototypes

int TWI_write(BYTE, UINT, UINT, const void*);	// Write to TWI device
int TWI_read(BYTE, UINT, UINT, void*);			// Read from TWI device
int RTC_init(void);								// Initialize RTC
int RTC_gettime(RTC*);							// Get time
int RTC_settime(const RTC*);					// Set time
DWORD get_fattime(void);

void ASA_SDC00_Select(char ASA_ID);
void ASA_SDC00_Deselect(void);

//Global Variables

char ASA_RTC00_ID = 44;		// Have RTC:0~7, No RTC:44
char RTC_Init_Success_Flag;

char ASA_SDC00_ID;

//Functions

// TWI bus protocol
static void TWI_delay(void)
{
	int n;
	for (n = 4; n; n--) PINB;
}
// Generate start condition on the TWI bus
static void TWI_start(void)
{
	SDA_HIGH();
	TWI_delay();
	SCL_HIGH();
	TWI_delay();
	SDA_LOW();
	TWI_delay();
	SCL_LOW();
	TWI_delay();
}
// Generate stop condition on the TWI bus
static void TWI_stop(void)
{
	SDA_LOW();
	TWI_delay();
	SCL_HIGH();
	TWI_delay();
	SDA_HIGH();
	TWI_delay();
}
// Send a byte to the TWI bus
static int TWI_send(BYTE dat)
{
	BYTE b = 0x80;
	int ack;
	do {
		if (dat & b)	 {	// SDA = Z/L
			SDA_HIGH();
		} else {
			SDA_LOW();
		}
		TWI_delay();
		SCL_HIGH();
		TWI_delay();
		SCL_LOW();
		TWI_delay();
	} while (b >>= 1);
	SDA_HIGH();
	TWI_delay();
	SCL_HIGH();
	ack = SDA_VAL ? 0 : 1;	// Sample ACK
	TWI_delay();
	SCL_LOW();
	TWI_delay();
	return ack;
}
// Receive a byte from the TWI bus
static BYTE TWI_rcvr(int ack)
{
	UINT d = 1;
	do {
		d <<= 1;
		SCL_HIGH();
		if (SDA_VAL) d++;
		TWI_delay();
		SCL_LOW();
		TWI_delay();
	} while (d < 0x100);
	if (ack) {		// SDA = ACK
		SDA_LOW();
	} else {
		SDA_HIGH();
	}
	TWI_delay();
	SCL_HIGH();
	TWI_delay();
	SCL_LOW();
	SDA_HIGH();
	TWI_delay();
	return (BYTE)d;
}
// TWI block read/write controls
int TWI_read(
				BYTE dev,		// Device address
				UINT adr,		// Read start address
				UINT cnt,		// Read byte count
				void* buff		// Read data buffer
)
{
	BYTE *rbuff = buff;
	int n;
	if (!cnt) return 0;
	n = 10;
	do {								// Select device
		TWI_start();
	} while (!TWI_send(dev) && --n);
	if (n) {
		if (TWI_send((BYTE)adr)) {		// Set start address
			TWI_start();				// Reselect device in read mode
			if (TWI_send(dev | 1)) {
				do {					// Receive data
					cnt--;
					*rbuff++ = TWI_rcvr(cnt ? 1 : 0);
				} while (cnt);
			}
		}
	}
	TWI_stop();							// Deselect device
	return cnt ? 0 : 1;
}
int TWI_write(
				BYTE dev,				// Device address
				UINT adr,				// Write start address
				UINT cnt,				// Write byte count
				const void* buff		// Data to be written
)
{
	const BYTE *wbuff = buff;
	int n;
	if (!cnt) return 0;
	n = 10;
	do {								// Select device
		TWI_start();
	} while (!TWI_send(dev) && --n);
	if (n) {
		if (TWI_send((BYTE)adr)) {		// Set start address
			do {						// Send data
				if (!TWI_send(*wbuff++)) break;
			} while (--cnt);
		}
	}
	TWI_stop();							// Deselect device
	return cnt ? 0 : 1;
}
// RTC functions
int RTC_gettime(RTC *RTC_str)
{
	BYTE buf[8];
	if (!TWI_read(0xD0, 0, 7, buf)) return 0;
	RTC_str->sec = (buf[0] & 0x0F) + ((buf[0] >> 4) & 7) * 10;
	RTC_str->min = (buf[1] & 0x0F) + (buf[1] >> 4) * 10;
	RTC_str->hour = (buf[2] & 0x0F) + ((buf[2] >> 4) & 3) * 10;
	RTC_str->wday = (buf[2] & 0x07);
	RTC_str->mday = (buf[4] & 0x0F) + ((buf[4] >> 4) & 3) * 10;
	RTC_str->month = (buf[5] & 0x0F) + ((buf[5] >> 4) & 1) * 10;
	RTC_str->year = 2000 + (buf[6] & 0x0F) + (buf[6] >> 4) * 10;
	return 1;
}
int RTC_settime(const RTC *RTC_str)
{
	BYTE buf[8];
	if( RTC_str->sec > 59 ) return 2;
	if( RTC_str->min > 59 ) return 2;
	if( RTC_str->hour > 23 ) return 2;
	if( RTC_str->wday > 7 ) return 2;
	if( RTC_str->mday > 31 ) return 2;
	if( RTC_str->month > 12 ) return 2;
	if( RTC_str->year > 2099 ) return 2;
	buf[0] = RTC_str->sec / 10 * 16 + RTC_str->sec % 10;
	buf[1] = RTC_str->min / 10 * 16 + RTC_str->min % 10;
	buf[2] = RTC_str->hour / 10 * 16 + RTC_str->hour % 10;
	buf[3] = RTC_str->wday & 7;
	buf[4] = RTC_str->mday / 10 * 16 + RTC_str->mday % 10;
	buf[5] = RTC_str->month / 10 * 16 + RTC_str->month % 10;
	buf[6] = (RTC_str->year - 2000) / 10 * 16 + (RTC_str->year - 2000) % 10;
	return TWI_write(0xD0, 0, 7, buf);
}
int RTC_init(void)
{
	BYTE buf[8];						// RTC R/W buffer
	UINT adr;
	TWI_INIT();							// Initialize TWI function
	// Read RTC registers
	if (!TWI_read(0xD0, 0, 8, buf)) return 0;	// TWI error
	if (buf[7] & 0x20) {				// When data has been volatiled, set default time
		// Clear nv-ram. Reg[8..63]
		memset(buf, 0, 8);
		for (adr = 8; adr < 64; adr += 8)
			TWI_write(0x0D, adr, 8, buf);
		// Reset time to Nov 13, '15. Reg[0..7]
		buf[4] = 11; buf[5] = 13; buf[6] = 15;
		TWI_write(0x0D, 0, 8, buf);
	}
	return 1;
}

char ASA_RTC00_set(char ASA_ID, char LSByte, char Mask, char shift, char Data)
{
	char set_Data = 0, i;

	if( RTC_Init_Success_Flag == 0 )			// 第一次被呼叫設定時進行RTC初始化
	{
		for(i=0; i<8; i++)						// 跑一趟for loop自動偵測RTC ID編號
		{
			ASA_ID_set(i);
			if( RTC_init() == 1 )
			{
				RTC_Init_Success_Flag = 1;
				ASA_RTC00_ID = i;
				break;
			}
		}
		if( ASA_RTC00_ID == 44 )
			return ARRTC_INVALID_ERR;
	}
	if( ASA_RTC00_ID != ASA_ID )		// ASA_ID錯誤(旋鈕設定跟參數設定不符)
		return ARRTC_ID_ERR;
	if( LSByte != 200 )					// LSByte錯誤
		return ARRTC_LSBYTE_ERR;
	if( shift > 7 || shift < 0 )		// Shift錯誤
		return ARRTC_SHIFT_ERR;

	set_Data = set_ID[(unsigned char)ASA_ID]._RTC00_set_reg;
	set_ID[(unsigned char)ASA_ID]._RTC00_set_reg = (set_ID[(unsigned char)ASA_ID]._RTC00_set_reg & (~Mask)) | ((Data<<shift) & Mask);

	return ARRTC_OK;
}
// 設定即時時鐘數值，依照時間格式及資料矩陣格式設定，若格式不符會回應錯誤
char ASA_RTC00_put(char ASA_ID, char LSByte, char Bytes, void *Data_p)
{
	int i, HrMode, year, month, mday, wday, hour, min, sec;
	RTC RTC_str;

	if( RTC_Init_Success_Flag == 0 )			// 第一次被呼叫設定時進行RTC初始化
	{
		for(i=0; i<8; i++)						// 跑一趟for loop自動偵測RTC ID編號
		{
			ASA_ID_set(i);
			if( RTC_init() == 1 )
			{
				RTC_Init_Success_Flag = 1;
				ASA_RTC00_ID = i;
				break;
			}
		}
		if( ASA_RTC00_ID == 44 )
			return ARRTC_INVALID_ERR;
	}
	if( ASA_RTC00_ID != ASA_ID )		// ASA_ID錯誤(旋鈕設定跟參數設定不符)
		return ARRTC_ID_ERR;
	if( LSByte > 16 || LSByte < 0)		// LSByte錯誤
		return ARRTC_LSBYTE_ERR;
	if( (LSByte+Bytes) > 17 || (LSByte+Bytes) < 0 )
		return ARRTC_BYTES_ERR;

	ASA_SDC00_Select(ASA_ID);

	for(i=0; i<Bytes; i++)
		put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[LSByte+i] = ((char*)Data_p)[i];

	switch( set_ID[(unsigned char)ASA_ID]._RTC00_set_reg&0x03 )
	{
		case 0x00:	// 24Hr制/ASCII字串
			sscanf(put_ID[(unsigned char)ASA_ID]._RTC00_put_reg, "%04d%02d%02d%01d%01d%02d%02d%02d",
			&year, &month, &mday, &wday, &HrMode, &hour, &min, &sec);
			RTC_str.year = year; RTC_str.month = month; RTC_str.mday = mday;
			RTC_str.wday = wday; RTC_str.hour = hour; RTC_str.min = min; RTC_str.sec = sec;
			break;
		case 0x01:	// 12Hr制AM/PM/ASCII字串
			sscanf(put_ID[(unsigned char)ASA_ID]._RTC00_put_reg, "%04d%02d%02d%01d%01d%02d%02d%02d",
			&year, &month, &mday, &wday, &HrMode, &hour, &min, &sec);
			RTC_str.year = year; RTC_str.month = month; RTC_str.mday = mday;
			RTC_str.wday = wday; RTC_str.hour = hour; RTC_str.min = min; RTC_str.sec = sec;
			RTC_str.hour = RTC_str.hour<13 ? (HrMode<2 ? RTC_str.hour : RTC_str.hour+12) : RTC_str.hour;
			break;
		case 0x02:	// 24Hr制/數值資料串
			RTC_str.year = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[0];
			RTC_str.year += put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[1]*256;
			RTC_str.month = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[2];
			RTC_str.mday = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[3];
			RTC_str.wday = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[4];
			RTC_str.hour = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[6];
			RTC_str.min = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[7];
			RTC_str.sec = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[8];
			break;
		case 0x03:	// 12Hr制AM/PM/數值資料串
			RTC_str.year = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[0];
			RTC_str.year += put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[1]*256;
			RTC_str.month = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[2];
			RTC_str.mday = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[3];
			RTC_str.wday = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[4];
			HrMode = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[5];
			RTC_str.hour = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[6];
			RTC_str.hour = RTC_str.hour<13 ? (HrMode<2 ? RTC_str.hour : RTC_str.hour+12) : RTC_str.hour;
			RTC_str.min = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[7];
			RTC_str.sec = put_ID[(unsigned char)ASA_ID]._RTC00_put_reg[8];
			break;
	}

	i = RTC_settime(&RTC_str);
	ASA_SDC00_Deselect();

	if( i == 0 )
		return ARRTC_STATE_ERR;					// 通訊失敗回應通訊狀態異常
		else if( i == 2 )
			return ARRTC_VALUE_ERR;				// 設定時間數值超過範圍異常
			else
				return ARRTC_OK;				// 通訊OK
}
// 取得即時時鐘數值，依照時間格式及資料矩陣格式輸出
char ASA_RTC00_get(char ASA_ID, char LSByte, char Bytes, void *Data_p)
{
	int i;
	RTC RTC_str;

	if( RTC_Init_Success_Flag == 0 )			// 第一次被呼叫設定時進行RTC初始化
	{
		for(i=0; i<8; i++)						// 跑一趟for loop自動偵測RTC ID編號
		{
			ASA_ID_set(i);
			if( RTC_init() == 1 )
			{
				RTC_Init_Success_Flag = 1;
				ASA_RTC00_ID = i;
				break;
			}
		}
		if( ASA_RTC00_ID == 44 )
			return ARRTC_INVALID_ERR;
	}
	if( ASA_RTC00_ID != ASA_ID )		// ASA_ID錯誤(旋鈕設定跟參數設定不符)
		return ARRTC_ID_ERR;
	if( LSByte > 116 || LSByte < 100)	// LSByte錯誤
		return ARRTC_LSBYTE_ERR;
	if( (LSByte+Bytes) > 117 || (LSByte+Bytes) < 100 )
		return ARRTC_BYTES_ERR;

	ASA_SDC00_Select(ASA_ID);

	if( RTC_gettime(&RTC_str) == 0 )
	{
		ASA_SDC00_Deselect();
		return ARRTC_STATE_ERR;			// 通訊失敗回應通訊狀態異常
	}
	// DS1307的星期進位怪怪，直接軟體用基姆拉爾森計算公式從年月日去計算星期幾
	BYTE m = RTC_str.month<=2 ? RTC_str.month+12 : RTC_str.month;
	WORD y = RTC_str.month<=2 ? RTC_str.year-1 : RTC_str.year;
	RTC_str.wday = 1 + (RTC_str.mday + 2*m + 3*(m+1)/5 + y + y/4 - y/100 + y/400)%7;

	switch( set_ID[(unsigned char)ASA_ID]._RTC00_set_reg&0x03 )
	{
		case 0x00:	// 24Hr制/ASCII字串
			sprintf(get_ID[(unsigned char)ASA_ID]._RTC00_get_reg, "%04d%02d%02d%01d%01d%02d%02d%02d",
					RTC_str.year, RTC_str.month, RTC_str.mday, RTC_str.wday, 0,
					RTC_str.hour, RTC_str.min, RTC_str.sec);
			break;
		case 0x01:	// 12Hr制AM/PM/ASCII字串
			sprintf(get_ID[(unsigned char)ASA_ID]._RTC00_get_reg, "%04d%02d%02d%01d%01d%02d%02d%02d",
					RTC_str.year, RTC_str.month, RTC_str.mday, RTC_str.wday, RTC_str.hour<12 ? 1 : 2,
					RTC_str.hour>12 ? RTC_str.hour-12 : RTC_str.hour, RTC_str.min, RTC_str.sec);
			break;
		case 0x02:	// 24Hr制/數值資料串
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[0] = RTC_str.year%256;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[1] = RTC_str.year/256;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[2] = RTC_str.month;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[3] = RTC_str.mday;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[4] = RTC_str.wday;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[5] = 0;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[6] = RTC_str.hour;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[7] = RTC_str.min;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[8] = RTC_str.sec;
			break;
		case 0x03:	// 12Hr制AM/PM/數值資料串
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[0] = RTC_str.year%256;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[1] = RTC_str.year/256;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[2] = RTC_str.month;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[3] = RTC_str.mday;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[4] = RTC_str.wday;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[5] = RTC_str.hour<12 ? 1 : 2;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[6] = RTC_str.hour>12 ? RTC_str.hour-12 : RTC_str.hour;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[7] = RTC_str.min;
			get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[8] = RTC_str.sec;
			break;
	}

	for(i=0; i<Bytes; i++)
		((char*)Data_p)[i] = get_ID[(unsigned char)ASA_ID]._RTC00_get_reg[LSByte-100+i];

	ASA_SDC00_Deselect();

	return ARRTC_OK;
}
DWORD get_fattime(void)
{
	RTC RTC_str;
	if (!RTC_Init_Success_Flag)
	{
		RTC_str.year = 2105;
		RTC_str.month = 11;
		RTC_str.mday = 11;
		RTC_str.hour = 11;
		RTC_str.min = 11;
		RTC_str.sec = 11;
		return	  ((DWORD)(RTC_str.year - 1980) << 25)
				| ((DWORD)RTC_str.month << 21)
				| ((DWORD)RTC_str.mday << 16)
				| ((DWORD)RTC_str.hour << 11)
				| ((DWORD)RTC_str.min << 5)
				| ((DWORD)RTC_str.sec >> 1);
	}
	// Get local time
	ASA_SDC00_Select(ASA_RTC00_ID);
	RTC_gettime(&RTC_str);
	//ASA_SDC00_Deselect();
	ASA_SDC00_Select(ASA_SDC00_ID);
	// Pack date and time into a DWORD variable
	return	  ((DWORD)(RTC_str.year - 1980) << 25)
			| ((DWORD)RTC_str.month << 21)
			| ((DWORD)RTC_str.mday << 16)
			| ((DWORD)RTC_str.hour << 11)
			| ((DWORD)RTC_str.min << 5)
			| ((DWORD)RTC_str.sec >> 1);
}