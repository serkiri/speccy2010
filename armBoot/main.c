#include <stdio.h>
#include <string.h>

#include "fatfs/ff.h"
#include "fatfs/diskio.h"

#include "system.h"

#define MAIN_PROG_START 0x20008000

char str[10];

bool MRCC_Config(void)
{
	MRCC_DeInit();

	int i = 16;
	while( i > 0 && MRCC_WaitForOSC4MStartUp() != SUCCESS ) i--;

	if( i != 0 )
	{
		/* Set HCLK to 60 MHz */
		MRCC_HCLKConfig(MRCC_CKSYS_Div1);
		/* Set CKTIM to 60 MHz */
		MRCC_CKTIMConfig(MRCC_HCLK_Div1);
		/* Set PCLK to 30 MHz */
		MRCC_PCLKConfig(MRCC_CKTIM_Div2);

		CFG_FLASHBurstConfig(CFG_FLASHBurst_Enable);
		MRCC_CKSYSConfig(MRCC_CKSYS_OSC4MPLL, MRCC_PLL_Mul_15);
	}

	/* GPIO pins optimized for 3V3 operation */
	MRCC_IOVoltageRangeConfig(MRCC_IOVoltageRange_3V3);

	MRCC_PeripheralClockConfig( MRCC_Peripheral_GPIO, ENABLE );
	MRCC_PeripheralClockConfig( MRCC_Peripheral_TIM0, ENABLE );
	MRCC_PeripheralClockConfig( MRCC_Peripheral_I2C, ENABLE );
	MRCC_PeripheralClockConfig( MRCC_Peripheral_SSP0, ENABLE );

	return i > 0;
}

void SPI_Config()
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIO0, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init( GPIO1, &GPIO_InitStructure );

	GPIO_WriteBit( GPIO1, GPIO_Pin_10, Bit_SET );
	GPIO_WriteBit( GPIO0, GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7, Bit_SET );

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_18 | GPIO_Pin_19;
	GPIO_Init( GPIO2, &GPIO_InitStructure );

	//----------------------------------------------------------------------------------------------------

    SSP_InitTypeDef   SSP_InitStructure;
    SSP_InitStructure.SSP_FrameFormat = SSP_FrameFormat_Motorola;
    SSP_InitStructure.SSP_Mode = SSP_Mode_Master;
    SSP_InitStructure.SSP_CPOL = SSP_CPOL_Low;
    SSP_InitStructure.SSP_CPHA = SSP_CPHA_1Edge;
    SSP_InitStructure.SSP_DataSize = SSP_DataSize_8b;
    SSP_InitStructure.SSP_NSS = SSP_NSS_Soft;
    SSP_InitStructure.SSP_ClockRate = 0;
    SSP_InitStructure.SSP_ClockPrescaler = 2; /* SSP baud rate : 30 MHz/(4*(2+1))= 2.5MHz */
    SSP_Init(SSP0, &SSP_InitStructure);

    SSP_Cmd(SSP0, ENABLE);
}

void UART0_SetBaudRate( dword baudRate )
{
	UART_InitTypeDef UART_InitStructure;
	UART_InitStructure.UART_WordLength = UART_WordLength_8D;
	UART_InitStructure.UART_StopBits = UART_StopBits_1;
	UART_InitStructure.UART_Parity = UART_Parity_No ;
	UART_InitStructure.UART_BaudRate = baudRate;
	UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
	UART_InitStructure.UART_Mode = UART_Mode_Tx_Rx;
	UART_InitStructure.UART_FIFO = UART_FIFO_Disable;

	UART_Cmd( UART0, DISABLE );
	UART_DeInit(UART0);
	UART_Init( UART0, &UART_InitStructure );

	UART_Cmd( UART0, ENABLE );
}

void UART0_Init( GPIO_TypeDef *tx_gpio, dword tx_pin, GPIO_TypeDef *rx_gpio, dword rx_pin )
{
	MRCC_PeripheralClockConfig( MRCC_Peripheral_UART0 | MRCC_Peripheral_GPIO, ENABLE );

	//---------------------------------------------

	GPIO_InitTypeDef GPIO_InitStructure;

	/* Configure the UART0_Tx as Alternate function Push-Pull */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin =  tx_pin;
	GPIO_Init(tx_gpio, &GPIO_InitStructure);

	/* Configure the UART0_Rx as input floating */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = rx_pin;
	GPIO_Init(rx_gpio, &GPIO_InitStructure);

	UART0_SetBaudRate( 115200 );
}

void UART0_WriteFile( const byte *s, dword cnt )
{
	while( cnt )
	{
        if( UART_GetFlagStatus( UART0, UART_FLAG_TxFIFOFull ) == RESET )
        {
            UART_SendData( UART0, *s++ );
            cnt--;
        }
	}

	while( UART_GetFlagStatus( UART0, UART_FLAG_Busy ) == SET );
}

void SD_Init()
{
    if( ( disk_status(0) & STA_NODISK ) == 0 && ( disk_status(0) & STA_NOINIT ) != 0 )
    {
        disk_initialize( 0 );

        if( ( disk_status(0) & STA_NOINIT ) != 0 )
        {
            __TRACE( "SD card init error :(\n" );
        }
        else
        {
            __TRACE( "SD card init OK..\n" );

            static FATFS fatfs;
            f_mount( 0, &fatfs );
        }
    }
}

void writePage(unsigned int sector, unsigned int startAddr, unsigned int endAddr, FIL newFirmware){
    char isError = 1;
    dword data4;
    dword storedData4;
    int pos;
    UINT res;
    byte *flashData;
    unsigned int realEndAddr;

    if (newFirmware.fsize < startAddr - 0x20008000){
        return;
    }

    if(newFirmware.fsize <= endAddr - 0x20008000){
        realEndAddr = newFirmware.fsize + 0x20008000;
    } else {
        realEndAddr = endAddr;
    }

    while (isError){
        __TRACE("page start=0x");
        __TRACE(itoa(startAddr, str, 16));
        __TRACE(" end=0x");
        __TRACE(itoa(realEndAddr, str, 16));
        __TRACE("\n");

        isError = 0;
        flashData = (byte*) startAddr;
        FLASH_EraseSector(sector);
        FLASH_WaitForLastOperation();
        f_lseek(&newFirmware, startAddr - 0x20008000);
        for( pos = 0; pos < realEndAddr - startAddr; pos += 4 ){
            f_read( &newFirmware, &data4, 4, &res );
//            if (pos == 0x40 && startAddr >= 0x20010000){
//              FLASH_WriteWord( (dword) flashData - 0x20000000, 0x50505050 );
//            } else {
                FLASH_WriteWord( (dword) flashData - 0x20000000, data4 );
//            }
            FLASH_WaitForLastOperation();

            storedData4 = *flashData + (*(flashData + 1) << 8) + (*(flashData + 2) << 16) + (*(flashData + 3) << 24);
            if (data4 != storedData4){
                __TRACE("error writing at position: 0x");
                __TRACE(itoa(flashData, str, 16));
                __TRACE(" data=0x");
                __TRACE(itoa(data4, str, 16));
                __TRACE(" but stored=0x");
                __TRACE(itoa(storedData4, str, 16));
                __TRACE("\n");
                isError = 1;
                break;
            }
            flashData += 4;
        }
    }
}

void UpdateFirmware()
{
    if( ( disk_status(0) & STA_NOINIT ) != 0 ) return;

    FIL newFirmware;
    if( f_open( &newFirmware, "speccy2010.bin", FA_READ ) != FR_OK )
    {
        __TRACE( "Cannot open speccy2010.bin...\n" );
        return;
    }
    unsigned int pos;
    byte data;
    UINT res;
    byte *flashData = (byte*) MAIN_PROG_START;
    byte olddata;

    for( pos = 0; pos < newFirmware.fsize; pos++ )
    {
        f_read( &newFirmware, &data, 1, &res );
        olddata = *flashData++;
        if( data != olddata )
        {
            __TRACE("Found difference - at pos =");
            __TRACE(itoa(pos, str, 16));
            __TRACE(", value on card = ");
            __TRACE(itoa(data, str, 16));
            __TRACE(", value in flash = ");
            __TRACE(itoa(olddata, str, 16));
            __TRACE("\n");

             break;
        }
    }

    if( pos >= newFirmware.fsize )
    {
        __TRACE( "Skipping firmware upgrade.\n" );
        return;
    }

//    __TRACE( "Firmware upgrade started.\n" );

    FLASH_DeInit();

    writePage(FLASH_BANK0_SECTOR4, 0x20008000, 0x20010000, newFirmware);
    writePage(FLASH_BANK0_SECTOR5, 0x20010000, 0x20020000, newFirmware);
    writePage(FLASH_BANK0_SECTOR6, 0x20020000, 0x20030000, newFirmware);
    writePage(FLASH_BANK0_SECTOR7, 0x20030000, 0x20040000, newFirmware);

//    __TRACE( "Firmware upgrade finished.\n" );
}
void JumpToMainProg()
{
	void (*MainProg)() = (void*) MAIN_PROG_START;
	MainProg();
}

int main()
{
	bool pllStatusOK;
	pllStatusOK = MRCC_Config();

    UART0_Init( GPIO0, GPIO_Pin_11, GPIO0, GPIO_Pin_10 );
    __TRACE( "Speccy2010 boot ver 1.5(rewrite pages for Ricia)!\n" );

    SPI_Config();
    SD_Init();

    UpdateFirmware();
	JumpToMainProg();

	return 0;
}

void __TRACE( const char *str )
{
    char lastChar = 0;
    const char *strPos = str;

    while( *strPos != 0 )
    {
        lastChar = *strPos++;

        if( lastChar == '\n' ) UART0_WriteFile( (byte*) "\r\n", 2 );
        else UART0_WriteFile( (byte*) &lastChar, 1 );

        WDT_Kick();
    }
}

void WDT_Kick()
{
}


char* itoa(unsigned int num, char* str, int base)
{
    int i = 0;
    bool isNegative = false;

    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (num < 0 && base == 10)
    {
        isNegative = true;
        num = -num;
    }

    while (num != 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
        num = num/base;
    }

    if (isNegative)
        str[i++] = '-';

    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end)
    {
        char temp = *(str+end);
        *(str+end) = *(str+start);
        *(str+start) = temp;
        start++;
        end--;
    }

    return str;
}
