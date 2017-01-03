#include <stdio.h>
#include <string.h>

#include "fatfs/ff.h"
#include "fatfs/diskio.h"

#include "system.h"

#define MAIN_PROG_START 0x20002000

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

void SimpleFill(dword data4)
{
    int pos;
    byte *flashData = (byte*) MAIN_PROG_START;
    flashData = (byte*) MAIN_PROG_START;

    for( pos = MAIN_PROG_START; pos < 0x2003FFFF; pos += 4 )
    {
        if( ( (dword) flashData & 0xfff ) == 0 ) __TRACE( "." );
//        if ((dword) flashData == 0x20012340 ){
//            FLASH_WriteWord( (dword) flashData - 0x20000000, 0x12345678 );
//        } else {
            FLASH_WriteWord( (dword) flashData - 0x20000000, data4 );
//        }
        FLASH_WaitForLastOperation();

        flashData += 4;
    }
    __TRACE("\n");
}

void SipleVerify(char data)
{
    char str[10];

    int pos;
    byte *flashData = (byte*) MAIN_PROG_START;
    flashData = (byte*) MAIN_PROG_START;
    byte olddata;

    for( pos = MAIN_PROG_START; pos < 0x2003FFFF; pos++ )
    {
        olddata = *flashData++;
        if (olddata != data){
            __TRACE("Found error - at pos =");
            __TRACE(itoa(pos, str, 16));
            __TRACE(", value in flash = ");
            __TRACE(itoa(olddata, str, 16));
            __TRACE(", should be = ");
            __TRACE(itoa(data, str, 16));
            __TRACE("\n");
        }
    }
}

void ScrambleFill()
{
    int pos;
    unsigned int data;
    byte *flashData = (byte*) MAIN_PROG_START;
    flashData = (byte*) MAIN_PROG_START;

    for( pos = MAIN_PROG_START; pos < 0x2003FFFF; pos += 4 )
    {
        if( ( (dword) flashData & 0xfff ) == 0 ) __TRACE( "." );
//        if ((dword) flashData == 0x20012340 ){
//            FLASH_WriteWord( (dword) flashData - 0x20000000, 0x12345678 );
//        } else {
            data = pos % 0x100;
            data = (data << 8) + data;
            data = (data << 16) + data;
            FLASH_WriteWord( (dword) flashData - 0x20000000, data );
//        }
        FLASH_WaitForLastOperation();

        flashData += 4;
    }
    __TRACE("\n");
}

void ScrambleVerify()
{
    char str[10];

    int pos;
    unsigned int data;
    byte *flashData = (byte*) MAIN_PROG_START;
    flashData = (byte*) MAIN_PROG_START;
    unsigned int olddata;

    for( pos = MAIN_PROG_START; pos < 0x2003FFFF; pos += 4 )
    {

        data = pos % 0x100;
        data = (data << 8) + data;
        data = (data << 16) + data;

        olddata = *flashData++;
        olddata = (olddata << 8) + *flashData++;
        olddata = (olddata << 8) + *flashData++;
        olddata = (olddata << 8) + *flashData++;

        if (olddata != data){
            __TRACE("Found error - at pos =");
            __TRACE(itoa(pos, str, 16));
            __TRACE(", value in flash = ");
            __TRACE(itoa(olddata, str, 16));
            __TRACE(", should be = ");
            __TRACE(itoa(data, str, 16));
            __TRACE("\n");
        }
    }
}

void eraseFlash(){
    __TRACE("Erasing\n");
    FLASH_EraseSector(FLASH_BANK0_SECTOR1);
    FLASH_WaitForLastOperation();
    FLASH_EraseSector(FLASH_BANK0_SECTOR2);
    FLASH_WaitForLastOperation();
    FLASH_EraseSector(FLASH_BANK0_SECTOR3);
    FLASH_WaitForLastOperation();
    FLASH_EraseSector(FLASH_BANK0_SECTOR4);
    FLASH_WaitForLastOperation();
    FLASH_EraseSector(FLASH_BANK0_SECTOR5);
    FLASH_WaitForLastOperation();
    FLASH_EraseSector(FLASH_BANK0_SECTOR6);
    FLASH_WaitForLastOperation();
    FLASH_EraseSector(FLASH_BANK0_SECTOR7);
    FLASH_WaitForLastOperation();
}

int main()
{
	bool pllStatusOK;
	pllStatusOK = MRCC_Config();

    UART0_Init( GPIO0, GPIO_Pin_11, GPIO0, GPIO_Pin_10 );
    __TRACE( "Internal flash test with simple scramble v2 (bug fixed)\n" );

    eraseFlash();
    __TRACE( "Writing 0x55\n" );
    SimpleFill(0x55555555);
    __TRACE( "Reading\n" );
    SipleVerify(0x55);

    eraseFlash();
    __TRACE( "Writing 0xAA\n" );
    SimpleFill(0xAAAAAAAA);
    __TRACE( "Reading\n" );
    SipleVerify(0xAA);

    eraseFlash();
    __TRACE( "Writing scramble\n" );
    ScrambleFill();
    __TRACE( "Reading scramble\n" );
    ScrambleVerify();


    __TRACE( "The End\n" );

    while (true);
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
