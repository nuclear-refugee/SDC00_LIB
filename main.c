#include "ASA_Lib.h"

#define TEST_PROG4_EN
#define TEST_PROG5_EN
#define TEST_PROG1_EN
#define TEST_PROG2_EN
#define TEST_PROG3_EN

void main(void) {

	ASA_M128_set();

	/* Variables for ASA lib */
	unsigned char ASA_ID = 4, Mask = 0xFF, Shift = 0, Setting = 0xFF;

	// The buffer used to swap data with SDC00
	uint8_t swap_buffer[64];
	memset(swap_buffer, 0, sizeof(swap_buffer));

	char check = 0;	// module communication result state flag

#ifdef TEST_PROG4_EN // EXAMPLE for override(create) files which named "testa.txt"
					 // and write data to the file

	/*** Open file ***/
	// Select target file
	ASA_SDC00_put(ASA_ID, 64, 8, "testa");
	ASA_SDC00_put(ASA_ID, 72, 3, "txt");

	// Configure to open file
	Setting = 0x05;		// Setting mode for openFile (overlapp or create)
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, Setting);
	if( check != 0 ) {				// 檢查回傳值做後續處理
		printf("Debug point 41, error code <%d>\n", check);
		return;
	}

	printf("Write data to file with overlapp(or create file) mode [Start]!!\n");
	for(int i= 1; i<50; i++) {
		sprintf(swap_buffer, "There is a string from M128 %d\r\n",i);

		// Compute size of string in swap_buffer
		int sizeof_string =64;
		for(int i=0; i<sizeof(swap_buffer); i++) {
			if( swap_buffer[i] == '\0' ) {
				sizeof_string = i;
				break;
			}
		}

		// Write data to the file
		ASA_SDC00_put(ASA_ID, 0, sizeof_string, swap_buffer);
	}

	printf("Write data to file with overlapp mode [Finish]!!\n");

	// Close file
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, 0x00);
	if( check != 0 ) {				// 檢查回傳值做後續處理
		printf("Debug point 42, error code <%d>\n", check);
		return;
	}

#endif // TEST_PROG4_EN

#ifdef TEST_PROG5_EN // EXAMPLE for append data to files which named "testa.txt"

	/*** Open file ***/
	// Select target file
	ASA_SDC00_put(ASA_ID, 64, 8, "testa");
	ASA_SDC00_put(ASA_ID, 72, 3, "txt");

	// Configure to open file
	Setting = 0x07;		// Setting mode for openFile (append)
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, Setting);
	if( check != 0 ) {				// 檢查回傳值做後續處理
		printf("Debug point 51, error code <%d>\n", check);
		return;
	}

	printf("Write data to file with append mode [Start]!!\n");
	for(int i= 1; i<25; i++) {
		sprintf(swap_buffer, "There is a append string from M128 %d\r\n",i);

		// Compute size of string in swap_buffer
		int sizeof_string =64;
		for(int i=0; i<sizeof(swap_buffer); i++) {
			if( swap_buffer[i] == '\0' ) {
				sizeof_string = i;
				break;
			}
		}
		// Write data to the file
		ASA_SDC00_put(ASA_ID, 0, sizeof_string, swap_buffer);
	}

	printf("Write data to file with append mode [Finish]!!\n");


	// Close file
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, 0x00);
	if( check != 0 ) {				// 檢查回傳值做後續處理
		printf("Debug point 52, error code <%d>\n", check);
		return;
	}

#endif // TEST_PROG5_EN

#ifdef TEST_PROG1_EN // EXAMPLE for read files parameter which named "testa.txt"

	/*** Open file ***/
	// Select target file
	ASA_SDC00_put(ASA_ID, 64, 8, "testa");
	ASA_SDC00_put(ASA_ID, 72, 3, "txt");

	// Configure to open file
	Setting = 0x01;		// Setting mode for open File (readonly)
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, Setting);	// 送出旗標組合
	if( check != 0 ) {				// 檢查回傳值做後續處理, If check equal 0 then operation was successful
		printf("Debug point 11, error code <%d>\n", check);
		return;
	}
	/*** End open file ***/

	unsigned char sz_buffer[4];

	// Read file parameter (File size)
	check = ASA_SDC00_get(ASA_ID, 82, 4, sz_buffer);

	for(int i=0; i<4; i++) {
		printf("<OUT> sz_buffer[%d]: %d\n",i,  (int)(sz_buffer[i]));
	}
	// File size(byte) : (bit 31:0)
	printf("<OUT> Sz: %ld\n",
	 ((long int)(sz_buffer[0])) +
	 ((long int)(sz_buffer[1]) << 8) +
	 ((long int)(sz_buffer[2]) << 16) +
	 ((long int)(sz_buffer[3]) << 24));

	// Configure to close file mode
	Setting = 0x00;
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, Setting);// 送出旗標組合
	if( check != 0 ) {				// 檢查回傳值做後續處理
		printf("Debug point 12, error code <%d>\n", check);
		return;
	}

#endif // TEST_PROG1_EN

#ifdef TEST_PROG2_EN

	uint16_t date_rawdata;
	uint16_t time_rawdata;

	/*** Open file ***/
	// Select target file
	ASA_SDC00_put(ASA_ID, 64, 8, "testa");
	ASA_SDC00_put(ASA_ID, 72, 3, "txt");

	// Configure to open file
	Setting = 0x01;		// Setting mode for openFile (readonly)
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, Setting);	// 送出旗標組合
	if( check != 0 ) {	// 檢查回傳值做後續處理, If check equal 0 then operation was successful
		printf("Debug point 21, error code <%d>\n", check);
	}
	/*** End open file ***/

	unsigned char date_buffer[2];
	unsigned char time_buffer[2];
	// Read file parameter (File last modify date)
	check = ASA_SDC00_get(ASA_ID, 78, 2, date_buffer);

	// Years: 1980 + (bit 15:9)
	// Month: (bit 8:5)
	// Day:   (bit 4:0)
	date_rawdata = date_buffer[0];
	date_rawdata += date_buffer[1] << 8;
	printf("<OUT> combine Date %0x %0x   -> %0x\n", date_buffer[1], date_buffer[0], date_rawdata);
	printf("<OUT>Date %u/%u/%u\n", 1980+(uint16_t)(date_rawdata>>9), (uint16_t)(date_rawdata>>5)& 0x0F, (uint16_t)(date_rawdata)& 0x1f);

	// Read file parameter (File last modify time)
	check = ASA_SDC00_get(ASA_ID, 76, 2, time_buffer);
	// Hour: (bit 15:11)
	// Min:  (bit 10:5)
	// Sec:  (bit 4:0)/2
	time_rawdata = time_buffer[0];
	time_rawdata += time_buffer[1] << 8;
	printf("<OUT> combine Time %0x%0x   -> %0x\n", time_buffer[1], time_buffer[0], time_rawdata);
	printf("<OUT>Time %u:%u:%u\n", time_rawdata>>11, (time_rawdata>>5) &0x3f, (time_rawdata & 0x1f) /2);

	// Show the raw data from Register "78" & "76"
	printf("<Rec Raw data>fdate = %0x, ftime = %0x\n", date_rawdata, time_rawdata);

	// Configure to close file mode
	Setting = 0x00;
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, Setting);// 送出旗標組合
	if( check != 0 ) {				// 檢查回傳值做後續處理
		printf("Debug point 22, error code <%d>\n", check);
		return;
	}

#endif // TEST_PROG2_EN

#ifdef TEST_PROG3_EN

	/*** Open file ***/
	// Select target file
	ASA_SDC00_put(ASA_ID, 64, 8, "testa");
	ASA_SDC00_put(ASA_ID, 72, 3, "txt");

	// Configure to open file
	Setting = 0x01;		// Setting mode for openFile (readonly)
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, Setting);	// 送出旗標組合
	if( check != 0 ) {	// 檢查回傳值做後續處理, If check equal 0 then operation was successful
		printf("Debug point 31, error code <%d>\n", check);
		return;
	}

	printf("Start reading file data\n");
	int rec = 0;
	for(int i=0; i<4000; i++) {
		/*** Read data from SDC00 ****/
		if(rec = ASA_SDC00_get(ASA_ID, 0, 64, swap_buffer)) {
			for(int i=0; i<64; i++) {
				printf("%c", swap_buffer[i]);
			}
			printf("\nReading finish! Get %ld  bytes %d\n", (long)i*64 + swap_buffer[63], rec);
			break;
		}
		for(int i=0; i<64; i++) {
			printf("%c", swap_buffer[i]);
		}
	}
	printf("Finish reading file data \n");

	// Configure to close file mode
	Setting = 0x00;
	check = ASA_SDC00_set(ASA_ID, 200, Mask, Shift, Setting);// 送出旗標組合
	if( check != 0 ) {				// 檢查回傳值做後續處理
		printf("Debug point 32, error code <%d>\n", check);
		return;
	}

#endif // TEST_PROG3_EN
	return;
}
