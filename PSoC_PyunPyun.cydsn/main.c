/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include <project.h>
#include <math.h>
#include "wavetable.h"

#define SAMPLE_CLOCK (48000.0f)

/* I2C slave address to communicate with */
#define I2C_LCD_ADDR	(0b0111110)

/* Buffer and packet size */
#define BUFFER_SIZE     (2u)
#define PACKET_SIZE     (BUFFER_SIZE)

/* Command valid status */
#define TRANSFER_CMPLT    (0x00u)
#define TRANSFER_ERROR    (0xFFu)

/***************************************
*               Macros
****************************************/

/* Set LED RED color */
#define RGB_LED_ON_RED  \
                do{     \
                    LED_RED_Write  (0u); \
                    LED_GREEN_Write(1u); \
                }while(0)

/* Set LED GREEN color */
#define RGB_LED_ON_GREEN \
                do{      \
                    LED_RED_Write  (1u); \
                    LED_GREEN_Write(0u); \
                }while(0)
					
/* DDS用変数 */
volatile uint32 phaseRegister;
volatile uint32 tuningWord;
                
/* LCDのコントラストの設定 */
uint8 contrast = 0b100000;	// 3.0V時 数値を上げると濃くなります。
							// 2.7Vでは0b111000くらいにしてください。。
							// コントラストは電源電圧，温度によりかなり変化します。実際の液晶をみて調整してください。

uint32 LCD_Write(uint8 *buffer)
{
	uint32 status = TRANSFER_ERROR;
	
    I2CM_I2CMasterWriteBuf(I2C_LCD_ADDR, buffer, PACKET_SIZE, I2CM_I2C_MODE_COMPLETE_XFER);
    
	while (0u == (I2CM_I2CMasterStatus() & I2CM_I2C_MSTAT_WR_CMPLT))
    {
        /* Waits until master completes write transfer */
    }

    /* Displays transfer status */
    if (0u == (I2CM_I2C_MSTAT_ERR_XFER & I2CM_I2CMasterStatus()))
    {
        RGB_LED_ON_GREEN;

        /* Check if all bytes was written */
        if(I2CM_I2CMasterGetWriteBufSize() == BUFFER_SIZE)
        {
            status = TRANSFER_CMPLT;
			
			// １命令ごとに余裕を見て50usウェイトします。
			CyDelayUs(50);	
        }
    }
    else
    {
        RGB_LED_ON_RED;
    }

    (void) I2CM_I2CMasterClearStatus();
	   
	return (status);
}

// コマンドを送信します。HD44780でいうRS=0に相当
void LCD_Cmd(uint8 cmd)
{
	uint8 buffer[BUFFER_SIZE];
	buffer[0] = 0b00000000;
	buffer[1] = cmd;
	(void) LCD_Write(buffer);
}

// データを送信します。HD44780でいうRS=1に相当
void LCD_Data(uint8 data)
{
	uint8 buffer[BUFFER_SIZE];
	buffer[0] = 0b01000000;
	buffer[1] = data;
	(void) LCD_Write(buffer);
}

void LCD_Init()
{
	CyDelay(40);
	LCD_Cmd(0b00111000);	// function set
	LCD_Cmd(0b00111001);	// function set
	LCD_Cmd(0b00010100);	// interval osc
	LCD_Cmd(0b01110000 | (contrast & 0xF));	// contrast Low
	LCD_Cmd(0b01011100 | ((contrast >> 4) & 0x3)); // contast High/icon/power
	LCD_Cmd(0b01101100); // follower control
	CyDelay(300);
	
	LCD_Cmd(0b00111000); // function set
	LCD_Cmd(0b00001100); // Display On
}

void LCD_Clear()
{
	LCD_Cmd(0b00000001); // Clear Display
	CyDelay(2);	// Clear Displayは追加ウェイトが必要
}

void LCD_SetPos(uint32 x, uint32 y)
{
	LCD_Cmd(0b10000000 | (x + y * 0x40));
}

// （主に）文字列を連続送信します。
void LCD_Puts(char8 *s)
{
	while(*s) {
		LCD_Data((uint8)*s++);
	}
}

CY_ISR(TimerISR_Handler)
{
	// Caluclate Wave Value
	phaseRegister += tuningWord;

	// 32bitのphaseRegisterをテーブルの10bit(1024個)に丸める
	uint32 index = phaseRegister >> 22;
    uint16 waveValue = waveTableSine[index];
	
	//DACSetVoltage(waveValue);
    IDAC8_SetValue(waveValue >> 4);
    //IDAC7_SetValue(waveValue >> 5);
    
    SamplingTimer_ClearInterrupt(SamplingTimer_INTR_MASK_TC);
}

int main()
{
    // 変数を初期化
	double waveFrequency = 1000.0f;
	tuningWord = waveFrequency * pow(2.0, 32) / SAMPLE_CLOCK;
    phaseRegister = 0;
    
    // コンポーネントを初期化
    SamplingTimer_Start(); 
    TimerISR_StartEx(TimerISR_Handler);
    IDAC8_Start();
    //IDAC7_Start();
    
    I2CM_Start();
	CyDelay(500);
    
    CyGlobalIntEnable;
    
    // LCDをRESET
    LCD_RST_Write(0u);
    CyDelay(1);
    LCD_RST_Write(1u);
    CyDelay(10);
    
    LCD_Init();
    LCD_Clear();
    
	LCD_Puts("PSoC PyunPyun");
	
	LCD_SetPos(1, 1);
    LCD_Puts("Demonstration");
    
    for(;;)
    {
        /* Place your application code here. */
    }
}

/* [] END OF FILE */
