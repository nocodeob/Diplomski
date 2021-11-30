/*
 * Uart.c
 *
 * Created: 24/08/2021 15:55:39
 *  Author: vkrus
 */ 

#include "Uart.h"						/* Include USART header file */
#include "lcd.h"

void USART_Init(unsigned long BAUDRATE)				/* USART initialize function */
{
	uint16_t baudPrescaler;
	baudPrescaler = (F_CPU/(16UL*BAUDRATE))-1;
	
	//
	//#ifdef DOUBLE_SPEED_MODE
	UCSRA &=~(1 << U2X);
	//#endif
	UCSRB |= (1 << RXEN) | (1 << TXEN);				/* Enable USART transmitter and receiver */
	UCSRC |= (1 << URSEL)| (1 << UCSZ0) | (1 << UCSZ1);	/* Write USCRC for 8 bit data and 1 stop bit */
	UBRRL = baudPrescaler;							/* Load UBRRL with lower 8 bit of prescale value */
	UBRRH = (baudPrescaler >> 8);					/* Load UBRRH with upper 8 bit of prescale value */
}

char USART_Recieve()									/* Data receiving function */
{
	while (!(UCSRA & (1 << RXC)));					/* Wait until new data receive */
	return(UDR);									/* Get and return received data */
}

void USART_Send(char data)						/* Data transmitting function */
{
	while (!(UCSRA & (1<<UDRE)));					
	UDR = data;										
}

void USART_SendString(char *str)					/* Send string of USART data function */
{
	int i=0;
	while (str[i]!=0)
	{
		USART_Send(str[i]);						/* Send each char of string till the NULL */
		i++;
	}
}