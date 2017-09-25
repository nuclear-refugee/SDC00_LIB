#include "ASA_Lib.h"

int main(void)
{
	
	ASA_M128_set();
	printf("Test!!\n");
	#define GetLSByteStart 100				// 輸入暫存器於記憶體列表之起始位置
	#define SetLSByteStart 200				// 設定暫存器於記憶體列表之起始位置
	#define FileParaSize 47				// 一行檔案參數的字串大小
	/* Variables */
	unsigned char ASA_ID = 1, Mask = 0xFF, Shift = 0, Setting = 0xFF, Bytes, i;
	char check;						// 宣告模組通訊狀態變數
	char File_Para[FileParaSize];			// 宣告檔案參數暫存字串變數

	// 讀參範例
	Bytes = FileParaSize;
	while(1) {
		// 設定為開檔讀參
		Setting = 0x10;				// 設定變數值為允許讀取檔案參數
		check = ASA_SDC00_set(ASA_ID, SetLSByteStart, Mask, Shift, Setting);// 送出旗標組合
		if( check != 0 ) {				// 檢查回傳值做後續處理
			printf("Debug point 1, error code <%d>\n", check);
			break;
		}

		// 讀取檔案參數
		printf("\nFile Parameter List:\n");
		printf("FileID __FileName__ _FileDate_ _Time _FileSize:\n");
		while(1) {
			check = ASA_SDC00_get(ASA_ID, GetLSByteStart+1, Bytes, File_Para);
			if( check == 0 )			// 接收回傳值判斷參數是否讀完或異常
			printf("%s\n", File_Para);	// 收到一行參數字串就印出一次
			else					// check=6 表沒檔案參數可讀
			break;				// 沒檔案參數可讀則跳離迴圈
		}

		// 設定為關檔
		Setting = 0x01;				// 設定變數值為關檔
		check = ASA_SDC00_set(ASA_ID, SetLSByteStart, Mask, Shift, Setting);// 送出旗標組合
		if( check != 0 )				// 檢查回傳值做後續處理
		printf("Debug point 2, error code <%d>\n", check);
		break;
	}
}
