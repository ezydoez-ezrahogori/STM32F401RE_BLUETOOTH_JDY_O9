/*
 * parse.c
 *
 *  Created on: 2 May, 2023
 *      Author: Ezrah Buki
 */

#include "main.h"
#include "string.h"
#include "ringbuffer.h"
#include "usart.h"
#include "tim.h"
#include "tmp102.h"
#include "stdlib.h"
#include "stdio.h"
#include "parse.h"

extern volatile uint32_t TimerCount10ms;

void Parser_DisplayTerminal(char *Msg)
{
	uint8_t Lenght = strlen(Msg);
	HAL_UART_Transmit(&huart1, (uint8_t*) Msg, Lenght, 1000);
}


void Parse_WriteDataToBuffer(Ringbuffer_t *RecieveBuffer, uint8_t *ParseBuffer)
{
	uint8_t i = 0;
	uint8_t temp;
	do
	{
		RB_Read(RecieveBuffer, &temp);
		if (temp == ENDLINE)
		{
			ParseBuffer[i] = '\n';
			ParseBuffer[i + 1] = 0;
		}
		else
		{
			ParseBuffer[i] = temp;
		}
		i++;
	} while (temp != ENDLINE);
}

/*
 * @ WAKE UP procedure
 */
static void Parser_WAKEUP(void)
{
	//wake up for 5 mins
	Parser_DisplayTerminal("System wake up\n\r");

}

/*
 * @ MEASURE procedure
 */
static void Parser_MEASURE(TMP102_t *TMP102)
{
	// send log to uart
	Parser_DisplayTerminal("Measurment done :");


	uint8_t Msg[32];
#if (TMP102_USE_FLOATNUMBERS == 1)
	float temperature;
	temperature = TMP102GetTempFloat(TMP102);
	sprintf((char*)Msg, " %2.2f deg C\n\r",temperature);
#else

	uint8_t TempBuffer[2];
	TMP102GetTempInt(TMP102,buffer);
	if (buffer[1] > 9)
	{
	sprintf((char*)Msg, " %d.%d deg C\n\r",TempBuffer[0],TempBuffer[1]);
	}
	else
	{
	sprintf((char*)Msg, " %d.0%d deg C\n\r",TempBuffer[0],TempBuffer[1]);
	}

#endif

	Parser_DisplayTerminal((char*)Msg);
	return;

	//bluetooth send to master
}

/*
 * @ DISPLAY procedure
 */
static void Parser_DISPLAY(void)
{
	// send log to uart
	Parser_DisplayTerminal("Temperature displayed for 1 minute \n\r");

	// start timer
	TimerCount10ms = 0;
	HAL_TIM_Base_Start_IT(&htim1);
}

static void Parser_HELP(void)
{
	// send log to uart
	Parser_DisplayTerminal("WAKEUP; - wake up from sleep mode \n\r");
	Parser_DisplayTerminal("MEASURE; - measure and send to terminal \n\r");
	Parser_DisplayTerminal("DISPLAY; - start measuring and display on 8segment \n\r");
	Parser_DisplayTerminal("SLEEP; - enter sleep mode \n\r");
	Parser_DisplayTerminal("HELP; - print all commands \n\r");

}

/*
 * @ SLEEP procedure
 */
static void Parser_SLEEP(uint8_t *ParseBuffer)
{
	//execute sleep

	//stop timer
	HAL_TIM_Base_Stop_IT(&htim1);

	//delete all the commands after SLEEP
	memset(ParseBuffer,0,(sizeof(ParseBuffer)));

	//send log on uart
	Parser_DisplayTerminal("Entering sleep mode\n\r");

	//stop hal tick
	HAL_SuspendTick();

	//enter sleep mode -> it will wait for IRQ to wake up
	HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

	//after wake up continue tick
	HAL_ResumeTick();

	//send log on uart
	Parser_DisplayTerminal("Waking up...\n\r");

	//start count down for going back to sleep
	HAL_TIM_Base_Start_IT(&htim1);

}


/*
 * @ function parse message and start command procedures
 */

uint8_t Parser_Parse(uint8_t *ParseBuffer, TMP102_t *TMP102)
{
	// Count how many commands we have to parse
	uint8_t cmd_count = 0;
	uint8_t i = 0;
	uint8_t LastCommand[16] =  {0};

	// For every semicolon count up until EOL
	do
	{
		if (ParseBuffer[i] == ';')
		{
			cmd_count++;
		}
		i++;
	} while (ParseBuffer[i] != '\n');


	// if there is no msg that we want to parse then just send it

	if (cmd_count == 0)
	{
		Parser_DisplayTerminal("Message received :");
		Parser_DisplayTerminal((char*)ParseBuffer);
		// return ERROR
		return PARSE_ERROR_NOCMD;
	}


	uint8_t *ParsePointer;

	// Execute cmd_count number of commands
	for (i = 0; i < cmd_count; i++)
	{

		// cut command from the message -> from beginning to ;
		//if first msg start from beginning
		if(i == 0)
		{
			ParsePointer = (uint8_t*)(strtok((char*)ParseBuffer, ";"));

		}else
		{
			ParsePointer = (uint8_t*)(strtok(NULL, ";"));
		}


		// if you put two same commands in a row - error
		if(strcmp((char*)ParsePointer,(char*)LastCommand) == 0)
		{
			Parser_DisplayTerminal("Error, same command twice in a row!\n\r");
			return PARSE_ERROR_2CMDS;
		}

		/*
		 * EXECUTE COMMANDS
		 */

		// do WAKE_UP
		if (strcmp("WAKEUP", (char*)ParsePointer) == 0)
		{
			Parser_WAKEUP();
		}
		// do MEASURE
		else if (strcmp("MEASURE", (char*)ParsePointer) == 0)
		{
			Parser_MEASURE(TMP102);
		}
		// do DISPLAY
		else if (strcmp("DISPLAY", (char*)ParsePointer) == 0)
		{
			Parser_DISPLAY();
		}
		//do help
		else if (strcmp("HELP", (char*)ParsePointer) == 0)
		{
			Parser_HELP();
		}
		// do SLEEP
		else if (strcmp("SLEEP", (char*)ParsePointer) == 0)
		{
			Parser_SLEEP(ParsePointer);
			return PARSE_OK;
		}
		else
		{
			Parser_DisplayTerminal("Commmand unknown \n\r");
			Parser_HELP();
			return PARSE_ERROR_NOCMD;
		}

		strcpy((char*)LastCommand,(char*)ParsePointer);
	}

	return PARSE_OK;
}
