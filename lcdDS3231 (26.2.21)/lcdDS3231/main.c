/*
 * lcdDS3231.c
 *
 * Created: 22/02/2021 09:34:00
 * Author : vkrus
 */ 
#define  F_CPU 11059200UL
#define	prev	0		//definiranje tipki koje cu koristiti u kodu
#define next	1
#define edit	2
#define set		3 
#define plus	4
#define minus	5
#define gate	6

#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "I2C_Master_H_file.h"
#include "lcd.h"
#include <util/delay.h>
#include <avr/interrupt.h>
#include "Uart.h"
#include <stdlib.h>			

#define SSID				"SSID"
#define PASSWORD			"PASSWORD"
#define DOMAIN				"api.thingspeak.com"
#define PORT				"80"
#define API_WRITE_KEY		"API_WRITE_KEY"
#define CHANNEL_ID			"CHANNEL_ID"

#define Device_Write_address	0xD0				/* Define RTC DS3231 slave address for write operation */
#define Device_Read_address		0xD1				/* Make LSB bit high of slave address for read operation */
#define Temperature_address		0x11				// Adresa od DS3231  na kojoj se nalazi temperatura
#define hour_24					0x00

int second,minute,hour,day,date,month,year,n1,n2;	// Inicijalizacija varijabli
uint8_t hourONtime = 23;								// vrijeme(sat) kada se sustav pali
uint8_t minuteONtime = 25;								// vrijeme(minute) kada se sustav pali
uint8_t hourOFFtime = 23;								// vrijeme(sat) kada se sustav gasi
uint8_t minuteOFFtime = 30;								// vrijeme(minute) kada se sustav gasi
uint8_t gateOFFtimeH = 3;								// vrijeme(sat) do kada ce sustav provjeravat dali su vrata otvorena
uint8_t gateOFFtimeM = 30;								// vrijeme(minute) do kada ce sustav provjeravat dali su vrata otvorena
uint8_t screenPicker = 0;									// varijabla koja omogucuje listanje na ekranu
uint8_t editMenu = 0;									// varijabla koja omogucuje odabiranje sta zelimo prepraviti
bool light = false;									// prati dali je svijetlo upaljeno ili ugaseno
bool gateFlag = false;								// prati dali su vrata otvore (bila otvorene)
uint8_t pressedEvent = 0;								// varijabla za spremanje trenutka(minute) kada su vrata bila otvorena
uint8_t pressedEventSec = 0;								// varijabla za spremanje trenutka(sekunde) kada su vrata bila otvorena
uint8_t gateInterval= 3;								// kolko dugo u minutama zelimo da svijetlo bude upaljeno ako su se vraata otvorila
uint8_t psat = 0;											// ovdje zapisemo sat kako bi kod bio lakse za citat
uint8_t pmin = 0;											// ovdje zapisemo minute kako bi kod bio lakse za citat
uint8_t psec = 0;
char uart_poruka[150];
char odgovor[10];
char *ret;
bool wifi_stanje;

void RTC_Clock_Write(char _hour, char _minute, char _second, char AMPM)
{
	_hour |= AMPM;
	I2C_Start(Device_Write_address);	// Zapocni I2C komunikaciju sa RTC 
	I2C_Write(0);						// Zapisi na 0 lokaciju, za vrijednosti sekunde 
	I2C_Write(_second);					// Zapisi na lokaciju 00 koja prima vrijednost sekunde
	I2C_Write(_minute);					// Zapisi na lokaciju 01 koja prima vrijednost minute
	I2C_Write(_hour);					// Zapisi na lokaciju 02 koja prima vrijednost sate
	I2C_Stop();							// Zaustavi I2C komunikaciju
}

void RTC_Calendar_Write(char _day, char _date, char _month, char _year)	
{
	I2C_Start(Device_Write_address);	// Zapocni I2C komunikaciju sa RTC 
	I2C_Write(3);						// Zapisi na 3 lokaciju, za vrijednosti dana 
	I2C_Write(_day);					// Zapisi na lokaciju 03 koja prima vrijednost dana, redni broj dana u tjednu
	I2C_Write(_date);					// Zapisi na lokaciju 04 koja prima vrijednost datuma/dan
	I2C_Write(_month);					// Zapisi na lokaciju 05 koja prima vrijednost datuma/mjesec
	I2C_Write(_year);					// Zapisi na lokaciju 06 koja prima vrijednost datum/godina
	I2C_Stop();							// Zaustavi I2C komunikaciju
}

void rtc_get_temp()									// funkcija za dohvacanje temperature od ds3231
{
	uint8_t MSB;
	uint8_t LSB;
	I2C_Start(Device_Write_address);			// Zapocni I2C komunikaciju sa RTC 
	I2C_Write(Temperature_address);				// Zapisi koja ce se adrese citati
	I2C_Repeated_Start(Device_Read_address);	// Ponovo pokreni komunikaciju, i predaj adresu s koje se cita vrijednost
	MSB = I2C_Read_Ack();						// Procitaj podatke
	LSB = I2C_Read_Nack();
	n1=MSB;
	n2=((LSB >> 6) * 0.25 );					//u 0x12 je decimalni dio temperature u bit7 i bit8 (po 0.25)
	I2C_Stop();									// Zaustavi I2C komunikaciju
}

void RTC_Read_Clock(char read_clock_address)		// funkcija za dohvacanje vremena sa DS3231
{
	I2C_Start(Device_Write_address);			// Zapocni I2C komunikaciju sa RTC 
	I2C_Write(read_clock_address);				// Zapisi koja ce se adrese citati
	I2C_Repeated_Start(Device_Read_address);	// Ponovo pokreni komunikaciju, i predaj adresu s koje se cita vrijednost

	second = I2C_Read_Ack();					// Procitaj sekunde
	minute = I2C_Read_Ack();					// Procitaj minute
	hour = I2C_Read_Nack();						// Procitaj sat
	I2C_Stop();									// Zaustavi I2C komunikaciju
}

void RTC_Read_Calendar(char read_calendar_address)  // funkcija za dohvacanje datuma sa DS3231
{
	I2C_Start(Device_Write_address);			// Zapocni I2C komunikaciju sa RTC 
	I2C_Write(read_calendar_address);			// Zapisi koja ce se adrese citati
	I2C_Repeated_Start(Device_Read_address);	// Ponovo pokreni komunikaciju, i predaj adresu s koje se cita vrijednost

	day = I2C_Read_Ack();						//Procitaj redni broj dana u tjednu
	date = I2C_Read_Ack();						//Procitaj datum/dan
	month = I2C_Read_Ack();						//Procitaj datum/mjesec
	year = I2C_Read_Nack();						//Procitaj datum/godinu
	I2C_Stop();									// Zaustavi I2C komunikaciju
}

/*	Funkcija koja provjerava dali je tipka stisnuta na portu B 
	ovisno primljenoj varijabli i postavlja debeounce tipke
	Funkcija prima int vrijednost tipke koju promatramo i vraca
	1 ako je tipka stisnuta, tj 0 ako nije */

unsigned char button_state(int button)			
{												
	if (!(PINB & (1<<button)))
	{
		_delay_ms(30);
		if (!(PINB & (1<<button)))
			return 1;
	}
	return 0;
}

/* Funkcija koja primljenu varijablu
	prebacuje u decimali oblik i BCD
	te tu decimalnu vrijednost vraca */

int BCDToDecimal(int BCD)							// funkcija koja pretvara iz BCD u decimalni oblik
{
	return (((BCD>>4)*10) + (BCD & 0xF));
}

/*	Funkcija koja ispisuje sve podatke na pocetnom ekranu (ekran 0)
	Funckija neprima nikakve podatke i nevraca nikakav podatak */

void screen0(void)									
{
	char ds3231_buffer[20];					
	snprintf(ds3231_buffer,20, "%02x:%02x", hour, minute); //zapisemo u buffer sta zelimo ispisati (x Unsigned hexadecimal integer)
	lcd_gotoxy(5,0);									// odredujemo poziciju gdje zelimo pisati po ekranu
	lcd_puts(ds3231_buffer);							// postavljamo na ekran informacije iz buffera
	
	rtc_get_temp();
	snprintf(ds3231_buffer,20,"%d.%d C", n1,n2);
	lcd_gotoxy(10,1);
	lcd_puts(ds3231_buffer);
	
	RTC_Read_Calendar(3);						
	snprintf(ds3231_buffer,20, "%02x/%02x/%02x ", date, month, year);
	lcd_gotoxy(0,1);
	lcd_puts(ds3231_buffer);


}

/*	Funkcija koja ispisuje sve podatke na pocetnom ekranu (ekran 0)
	kada udjemo u mod za prepravljanje podataka
	Funckija prima podatke sati i min koje su tipa int
	i nevraca nikakav podatak */

void screen01(int sati, int min)									
{
	char ds3231_buffer[20];
	snprintf(ds3231_buffer,20, "%02d:%02d", sati, min); //zapisemo u buffer sta zelimo ispisati (x Unsigned hexadecimal integer)
	lcd_gotoxy(5,0);								// odredujemo poziciju gdje zelimo pisati po ekranu
	lcd_puts(ds3231_buffer);						// postavljamo na ekran informacije iz buffera
	
	rtc_get_temp();
	snprintf(ds3231_buffer,20,"%d.%d C", n1,n2);
	lcd_gotoxy(10,1);
	lcd_puts(ds3231_buffer);
	
	RTC_Read_Calendar(3);						
	snprintf(ds3231_buffer,20, "%02x/%02x/%02x ", date, month, year);
	lcd_gotoxy(0,1);
	lcd_puts(ds3231_buffer);


}

/*	Funkcija koja ispisuje sve podatke na prvom ekranu (ekran 1)
	Funckija neprima nikakve podatke i nevraca nikakav podatak*/

void screen1(void)									
{	
	char buffer1[20];
	lcd_gotoxy(0,0);
	lcd_puts("Rasvjeta");
	lcd_gotoxy(0,1);

	lcd_gotoxy(1,1);
	sprintf(buffer1,"%02d", hourONtime);
	lcd_puts(buffer1);

	lcd_gotoxy(3,1);
	sprintf(buffer1,":%02d", minuteONtime);
	lcd_puts(buffer1);

	lcd_gotoxy(7,1);
	lcd_puts("do");

	lcd_gotoxy(10,1);
	sprintf(buffer1,"%02d", hourOFFtime);
	lcd_puts(buffer1);

	lcd_gotoxy(12,1);
	sprintf(buffer1,":%02d", minuteOFFtime);
	lcd_puts(buffer1);


}

/*	Funkcija koja ispisuje sve podatke na prvom ekranu (ekran 1)
	Funckija neprima nikakve podatke i nevraca nikakav podatak*/

void screen2(void)									
{	
	char buffer1[20];
	lcd_gotoxy(0,0);
	lcd_puts("Vrata:");
	lcd_gotoxy(8,0);
	sprintf(buffer1,"%02dmin", gateInterval);
	lcd_puts(buffer1);
	lcd_gotoxy(0,1);
	lcd_puts("Do:");
	lcd_gotoxy(5,1);
	sprintf(buffer1,"%02d:%02d", gateOFFtimeH,gateOFFtimeM);
	lcd_puts(buffer1);
}

/*	Funkcija koja pali svijetla (PORT D i C)
	Funkcija neprima nikakav podatak i nevraca nikakav podatak */

void turnON(void)									
{
	for (uint8_t i = 2; i < 8; i++)
	{
		PORTD &=~(1<<i);
	}
	for (uint8_t j = 6; j < 8; j++)
	{
		PORTC &=~(1<<j);
	}
}

/*	Funkcija koja gasi svijetla (PORT D i C)
	Funkcija neprima nikakav podatak i nevraca nikakav podatak */

void turnOFF(void)									
{
	for (uint8_t i = 7; i >=2 ; i--)
	{
		PORTD |=(1<<i);
	}
	for (uint8_t j = 7; j >=6 ; j--)
	{
		PORTC |=(1<<j);
	}
}

/*	Funkcija koja regulira da ce primljeni podatak biti 
	u intervalu 0 do 24
	Funkcija prima podatak tipa int i vraca podatak tipa int */

int intervalH (int val)								
{							
	if ( val > 24)
	{
		val = 1;
	}
	if (val < 0)
	{
		val = 24;
	}
	return val;
}

/*	Funkcija koja regulira da ce primljeni podatak biti 
	u intervalu 0 do 59
	Funkcija prima podatak tipa int i vraca podatak tipa int */

int intervalM (int val)	
{						
	if ( val > 59)
	{
		val = 0;
	}
	if (val < 0)
	{
		val = 59;
	}
	return val;
}

/*	Funkcija koja prati dali je proslo odredjeno
	 vrijeme ovisno o varijabli gateIntervalu 
	Funkcija neprima nikakav podatak i nevraca nikakav podatak */

void gateTimer(void)								
{
	RTC_Read_Clock(0);
	int currentTime = BCDToDecimal(minute);
	int currentTimeSec = BCDToDecimal(second);
	if ( currentTime < pressedEvent)
	{
		currentTime = currentTime + pressedEvent;
	}
	if (currentTime-pressedEvent >= gateInterval)
	{
		if (currentTimeSec >= pressedEventSec)
		{
			gateFlag = false;
		}
	}
}

/*	Funkcija koja provjeri dali su vrata otvorena,
	ako su otvorena zapamti vrijeme i postavi zastavicu 
	Funkcija neprima nikakav podatak i nevraca nikakav podatak */

void gateEvent(void)								
{
	if(button_state(gate))
	{
		pressedEvent = pmin;
		pressedEventSec = psec;
		gateFlag = true;
	}
}

/*	Funkcija koja salje s atmega16 na esp8266 preko uarta
	AT komandu za spajanje na stranicu
	Funkcija neprima nikakav podatak i nevraca nikakav podatak */

void esp_site(void)
{
	char _atCommand[60];
	sprintf(_atCommand, "AT+CIPSTART=\"TCP\",\"%s\",%s", DOMAIN, PORT);
	USART_SendString(_atCommand);
	USART_SendString("\r\n");
	_delay_ms(1500);
}

/*	Funkcija koja salje s atmega16 na esp8266 preko uarta
	AT komandu koja ce rec kolko dugacku poruku saljemo 
	i nakon toga saljemo podatak GET ..... za zapis podatka na stranici
	Funkcija prima podatake i svi su tipa uint8_t 
	field - koje polje na stranici thingspeak se upisuje podatak
	d_num - kolko podataka ce se zapisati u polje
	data1 - podatak 1 koji se upisuje u polje
	data2 - podatak 2 koji se upisuje u polje 
	Funkcija nevraca nikakav podatak */

void esp_publish(uint8_t field, uint8_t d_num, uint8_t data1, uint8_t data2)
{
	char _atCommand[60];
	char get_buffer[150];
	if (d_num == 1)
	{
		sprintf(get_buffer, "GET https://api.thingspeak.com/update?api_key=%s&field%d=%d",API_WRITE_KEY,field,data1);
	} 
	else if (d_num ==2)
	{
		sprintf(get_buffer, "GET https://api.thingspeak.com/update?api_key=%s&field%d=%d%d",API_WRITE_KEY,field,data1,data2);
	}
	
	sprintf(_atCommand, "AT+CIPSEND=%d", (strlen(get_buffer)+2));
	USART_SendString(_atCommand);
	USART_SendString("\r\n");
	_delay_ms(1500);
	USART_SendString(get_buffer);
	USART_SendString("\r\n");
	_delay_ms(1500);	
}

/*	Funkcija koja salje s atmega16 na esp8266 preko uarta
	AT komandu koja ce rec kolko dugacku poruku saljemo
	i nakon toga saljemo podatak GET ..... za citanje podatka sa stranice
	Funkcija prima podatak tipa uint8_t 
	field - koje polje na stranici thingspeak se upisuje podatak
	Funkcija nevraca nikakav podatak */

void esp_local_update(uint8_t field)
{
	char _atCommand[60];
	char get_buffer[150];
	sprintf(get_buffer, "GET https://api.thingspeak.com/channels/%s/fields/%d/last.txt",CHANNEL_ID,field);
	sprintf(_atCommand, "AT+CIPSEND=%d", (strlen(get_buffer)+2));
	
	USART_SendString(_atCommand);
	USART_SendString("\r\n");
	_delay_ms(1500);
	USART_SendString(get_buffer);
	USART_SendString("\r\n");
	
}

/*	Funkcija koja ovisno o primljenoj poruci s esp8266 preko uarta,
	(primljena poruka se nalazi u uart_poruka),
	obnavlja vrijednosti lokalnih varijabli ili postavlja flag za wifi,
	Funkcija prima podatak tipa uint8_t
	field - koje polje na stranici thingspeak se upisuje podatak
	Funkcija nevraca nikakav podatak */

void uart_korisni_dio(uint8_t field)
{
	if (strstr(uart_poruka,"CLO")!=NULL)
	{
		ret = strstr(uart_poruka,"+IPD");	// uzmi adresu gdje pocinje "+IPD" u pristigloj poruci
		int vrijednost=atoi(ret+7);			//korisni podatak se nalazi na +7 adresi od ret +IPD
		
		//provjeravamo gdje moramo upisati podatak
		if (field==1)
		{
			hourONtime=vrijednost/100;
			minuteONtime=vrijednost%100;
		}
		else if (field==2)
		{
			hourOFFtime=vrijednost/100;
			minuteOFFtime=vrijednost%100;
		}
		else if (field==3)
		{
			gateInterval=vrijednost;
		}
		else if (field==4)
		{
			gateOFFtimeH=vrijednost/100;
			gateOFFtimeM=vrijednost%100;
		}
		//ocistimo uart_poruka i ret
		memset(uart_poruka,0,150);
		ret =0;
	}
	else if (strstr(uart_poruka,"STATUS:")!=NULL)
	{
		ret = strstr(uart_poruka,"STATUS:");
		int vrijednost = atoi(ret+7);
		
		if (vrijednost==5)
			wifi_stanje=false;
		else
			wifi_stanje=true;
		//ocistimo uart_poruka i ret
		memset(uart_poruka,0,150);
		ret =0;
	}
	else if (strstr(uart_poruka,"WIFI CONNECTED")!=NULL)
	{
		wifi_stanje=true;
		//ocistimo uart_poruka i ret
		memset(uart_poruka,0,150);
		ret =0;
	}
	else if (strstr(uart_poruka,"FAIL")!=NULL)
	{
		wifi_stanje=false;
		//ocistimo uart_poruka i ret
		memset(uart_poruka,0,150);
		ret =0;
	}
}

/*	Funkcija koja prima poruku s esp8266 preko uarta,
	(primljena poruka se nalazi u uart_poruka),
	Funkcija prima podatak tipa uint8_t i char*
	field - koje polje na stranici thingspeak se upisuje podatak
	ocekivani_odgovor - podatak koji ocekujemo ukoliko smo uspjesno poslali poruku
	neocekivani_odgovor + podatak koji ocekujemo ukoliko smo neuspjesno poslali poruku
	Funkcija nevraca nikakav podatak */

void uart_primi(uint8_t field,char* ocekivani_odgovor,char* neocekivani_odgovor)
{
	uint8_t i=0;
	//primaj poruku sve dok nedodjes do ocekivani_odgovor ili neocekivani_odgovor
	do          
	{
		uart_poruka[i]=USART_Recieve();
		i++;
		if (i==150)
		{
			i=0;
		}
	}while ((strstr(uart_poruka,ocekivani_odgovor)==NULL)&&(strstr(uart_poruka,neocekivani_odgovor)==NULL));
	
	
	if (strstr(uart_poruka,"ERROR")!=NULL)
	{
		memset(uart_poruka,0,150);
		ret =0;
	}
	uart_korisni_dio(field);
}

/* Funkcija koja salje s atmega16 na esp8266 preko uarta
	AT komandu za spajanje na wifi mrezu
	Funkcija neprima nikakav podatak i nevraca nikakav podatak */
 
void esp_wifi(void)
{
	char _atCommand[60];
	sprintf(_atCommand, "AT+CWJAP=\"%s\",\"%s\"", SSID, PASSWORD);
	USART_SendString(_atCommand);
	USART_SendString("\r\n");
	uart_primi(0,"WIFI CONNECTED","FAIL");
	_delay_ms(2000);
}




int main(void)
{	
	//int b=0;
//***** OUTPUT ********
	DDRD = 0b11111100;									// ukljuci sve pinove za port D
	PORTD = 0b11111100;									//pocetna vrijednost outputa 1
	DDRC = 0b11000000;									// ukljuci sve pinove za port D
	PORTC = 0b11000000;
//***** INPUT ********
	DDRB = 0x00;									// postavit  pinove na input na portu B 
	PORTB = 0b01111111;								// odabrati koristenje zeljenog moda
	SFIOR &= ~(1<<PUD);								// odabrati koristenje zeljenog moda
	
	I2C_Init();										/* Initialize I2C */
	lcd_init(LCD_DISP_ON);							/* Initialize LCD16x2 */
	USART_Init(115200);
	
	//RTC_Clock_Write(0x20, 0x01, 0x00, hour_24); //Zapisuje sat, minute, sekunde u tocno ovom formatu
	//RTC_Calendar_Write(0x1, 0x30, 0x8, 0x21);	// Zapisuje redni broj dana u tjednu, dan u mjesecu, mjesec i godinu
	
	_delay_ms(1000);
	USART_SendString("AT+CIPSTATUS\r\n");
	uart_primi(1,"OK","ERROR");
	if (wifi_stanje==false)
	{
		esp_wifi();
	}

    while(1)
    {
		RTC_Read_Clock(0);
		psat =BCDToDecimal(hour) ;
		pmin = BCDToDecimal(minute);
		psec = BCDToDecimal(second);
		
		switch (screenPicker)						// switch case pomocu kojeg biramo na kojem smo ekranu
		{
//***** home screen ********
		case 0:
		
			screen0();
			
				if (psec==30)
				{	
					USART_SendString("AT+CIPSTATUS\r\n");
					uart_primi(1,"OK","ERROR");
					lcd_gotoxy(15,0);
					lcd_puts("*");
					if (wifi_stanje==false)
						esp_wifi();
					
					if (wifi_stanje==true)
					{
						
						esp_site();
						esp_local_update(1);
						uart_primi(1,"CLO","ERROR");
						esp_site();
						esp_local_update(2);
						uart_primi(2,"CLO","ERROR");
						esp_site();
						esp_local_update(3);
						uart_primi(3,"CLO","ERROR");
						esp_site();
						esp_local_update(4);
						uart_primi(4,"CLO","ERROR");
						lcd_gotoxy(15,0);
						lcd_puts(" ");
					}
				}
				/////////////////////////////////////////////////////////////////////////////////////////////////////////
				if (button_state(edit))
				{
					int postaviH = psat;
					int postaviM = pmin;
					
					do
					{
						switch (editMenu)
						{
							case 0:
							
							lcd_gotoxy(4,0);
							lcd_puts(">");
							screen01(postaviH,postaviM);
							if (button_state(plus))
							{
								postaviH++;
								postaviH = intervalH(postaviH);
								screen01(postaviH,postaviM);
								_delay_ms(200);
							}
							if (button_state(minus))
							{
								postaviH--;
								postaviH = intervalH(postaviH);
								screen01(postaviH,postaviM);
								_delay_ms(200);
							}
							if (button_state(next))
							{
								editMenu++;
								_delay_ms(200);
								lcd_clrscr();
								
							}
							break;
							
							case 1:
							lcd_gotoxy(10,0);
							lcd_puts("<");
							screen01(postaviH,postaviM);
							if (button_state(plus))
							{
								postaviM++;
								postaviM = intervalM(postaviM);
								screen01(postaviH,postaviM);
								_delay_ms(200);
							}
							if (button_state(minus))
							{
								postaviM--;
								postaviM = intervalM(postaviM);
								screen01(postaviH,postaviM);
								_delay_ms(200);
							}
							if (button_state(next))
							{
								editMenu++;
								_delay_ms(200);
								lcd_clrscr();
							}
							if (button_state(prev))
							{
								editMenu--;
								_delay_ms(200);
								lcd_clrscr();
								
							}
							break;
						}
					} while (button_state(set)!=1);
					if (postaviH > 9 && postaviH < 20)
						postaviH = postaviH + 6;
					else if(postaviH >= 20)
						postaviH = postaviH + 12;
					
					if (postaviM > 9 && postaviM < 20)
						postaviM = postaviM + 6;
					else if(postaviM >= 20 && postaviM < 30)
						postaviM = postaviM + 12; 
					else if(postaviM >= 30 && postaviM < 40)
						postaviM = postaviM + 18;
					else if(postaviM >= 40 && postaviM < 50)
						postaviM = postaviM + 24;
					else if(postaviM >= 50)
						postaviM = postaviM + 30;

					RTC_Clock_Write(postaviH, postaviM, 0x00, hour_24);/* Write Hour Minute Second Format */
					lcd_clrscr();// ocisti ekran kad izadjes iz edit moda
				}
				///////////////////////////////////////////////////////////////////////////////////////////////////////////
						
			if (button_state(next))						// provjera dali je pritisnuta tipka next
			{
				screenPicker++;							// povecamo varijablu kako bi usli u case 1 od switch case (screen 1)
				lcd_clrscr();							// obrisemo sve s ekrana
				_delay_ms(200);							// pricekamo 200 ms kako nebi jedan pritisak registrirao vise puta
			}
		 break;
//***** screen 1 **********
		case 1:
			screen1();
			if (button_state(edit))						// provjera dali je stisnuta edit tipka, ako je ulazimo u edit mode
			{
				do										// zadrzavamo se u edit modu sve dok se nepritisne tipka set
				{
					switch (editMenu)					// switch case s kojim odredujemo koju varijablu zelimo editirati
					{
						case 0:							// editiranje sati kada se pali svjetlo
						
							lcd_gotoxy(0,1);
							lcd_puts(">");				// pomocu ovog korisniku dajemo do znanja da je u edit modu i koju varijablu trenutno editira
							screen1();
							if (button_state(plus))		// ako se stisne ili drzi tipka plus povecavaj brojcanik
							{
								hourONtime++;
								hourONtime = intervalH(hourONtime);
								screen1();
								_delay_ms(200);			// omogucuje da pritisak registrira samo jednom a drzanje vise puta
							}
							if (button_state(minus))	// ako se stisne ili drzi tipka minus smanjuj brojcanik
							{
								hourONtime--;
								hourONtime = intervalH(hourONtime);
								screen1();
								_delay_ms(200);
							}
							if (button_state(next))		// ako se stisne tipka next prelazimo na sljedecu varijablu koju cemo editirat
							{
								editMenu++;
								_delay_ms(200);
								lcd_clrscr();
								
							}
						break;
						
						case 1:
						
							lcd_gotoxy(6,1);
							lcd_puts("<");
							screen1();
	
							if (button_state(plus))		
							{
								minuteONtime++;
								minuteONtime = intervalM(minuteONtime);
								screen1();
								_delay_ms(200);
							}
							if (button_state(minus))
							{
								minuteONtime--;
								minuteONtime = intervalM(minuteONtime);
								screen1();
								_delay_ms(200);
							}
							if (button_state(next))
							{
								editMenu++;
								_delay_ms(200);
								lcd_clrscr();	
							}
							if (button_state(prev))
							{
								editMenu--;
								_delay_ms(200);
								lcd_clrscr();
								
							}		
						break;
						
						case 2:
							lcd_gotoxy(9,1);
							lcd_puts(">");
							screen1();
							if (button_state(plus))
							{
								hourOFFtime++;
								hourOFFtime = intervalH(hourOFFtime);
								screen1();
								_delay_ms(200);
							}
							if (button_state(minus))
							{
								hourOFFtime--;
								hourOFFtime = intervalH(hourOFFtime);
								screen1();
								_delay_ms(200);
							}
							if (button_state(next))
							{
								editMenu++;
								_delay_ms(200);
								lcd_clrscr();
							}
							if (button_state(prev))
							{
								editMenu--;
								_delay_ms(200);
								lcd_clrscr();
							}
						break;
						
						case 3:
							lcd_gotoxy(15,1);
							lcd_puts("<");
							screen1();
							if (button_state(plus))
							{
								minuteOFFtime++;
								minuteOFFtime = intervalM(minuteOFFtime);
								screen1();
								_delay_ms(200);
							}
							if (button_state(minus))
							{
								minuteOFFtime--;
								minuteOFFtime = intervalM(minuteOFFtime);
								screen1();
								_delay_ms(200);
							}
							if (button_state(prev))
							{
								editMenu--;
								_delay_ms(200);
								lcd_clrscr();
							}	
						break;
					}
				} while (button_state(set)!=1);					// zadrzavamo se u edit modu sve dok se nepritisne tipka set
				if (wifi_stanje == true)
				{
					lcd_clrscr();
					lcd_gotoxy(4,0);
					lcd_puts("UPDATING");
					esp_site();
					esp_publish(1,2,hourONtime,minuteONtime);
					_delay_ms(15500);
					esp_site();
					esp_publish(2,2,hourOFFtime,minuteOFFtime);
				}
				lcd_clrscr();									// ocisti ekran kad izadjes iz edit moda
			}
			if (button_state(next))								// provjera dali pritisnuta tipka next da bi osli na sljedeci ekran
			{
				screenPicker++;
				lcd_clrscr();
				_delay_ms(200);
			}

			if (button_state(prev))								// provjera dali pritisnuta tipka prev da bi osli na prosli ekran
			{
				screenPicker--;
				lcd_clrscr();
				_delay_ms(200);
			}
		break;
//******* screen 2 ***********
		case 2:
			screen2();
			editMenu=0;											// postavimo editMenu na 0 kako bi krenili editiranje od prve varijable
			if (button_state(edit))
			{
				do
				{
					switch (editMenu)
					{
						case 0:
			
							lcd_gotoxy(6,0);
							lcd_puts(">");
							screen2();
							if (button_state(plus))
							{
								gateInterval++;
								gateInterval = intervalM(gateInterval);
								screen2();
								_delay_ms(200);
							}
							if (button_state(minus))
							{
								gateInterval--;
								gateInterval = intervalM(gateInterval);
								screen2();
								_delay_ms(200);
							}
							if (button_state(next))
							{
								editMenu++;
								_delay_ms(200);
								lcd_clrscr();
				
							}	
						break;
			
						case 1:
							lcd_gotoxy(3,1);
							lcd_puts(">");
							screen2();
							if (button_state(plus))
							{
								gateOFFtimeH++;
								gateOFFtimeH = intervalH(gateOFFtimeH);
								screen2();
								_delay_ms(200);
							}
							if (button_state(minus))
							{
								gateOFFtimeH--;
								gateOFFtimeH = intervalH(gateOFFtimeH);
								screen2();
								_delay_ms(200);
							}
							if (button_state(next))
							{
								editMenu++;
								_delay_ms(200);
								lcd_clrscr();
							}
							if (button_state(prev))
							{
								editMenu--;
								_delay_ms(200);
								lcd_clrscr();
				
							}
						break;
			
						case 2:
							lcd_gotoxy(10,1);
							lcd_puts("<");
							screen2();
							if (button_state(plus))
							{
								gateOFFtimeM++;
								gateOFFtimeM = intervalM(gateOFFtimeM);
								screen2();
								_delay_ms(200);
							}
							if (button_state(minus))
							{
								gateOFFtimeM--;
								gateOFFtimeM = intervalM(gateOFFtimeM);
								screen2();
								_delay_ms(200);
							}
							if (button_state(prev))
							{
								editMenu--;
								_delay_ms(200);
								lcd_clrscr();
							}
						break;
					}
				} while (button_state(set)!=1);
				
				if (wifi_stanje == true)
				{
					lcd_clrscr();
					lcd_gotoxy(4,0);
					lcd_puts("UPDATING");
					esp_site();
					esp_publish(3,1,gateInterval,0);
					_delay_ms(15500);
					esp_site();
					esp_publish(4,2,gateOFFtimeH,gateOFFtimeM);
				}
				lcd_clrscr();// ocisti ekran kad izadjes iz edit moda
			}

			if (button_state(prev))								// na zadnjem ekranu ocekujemo samo pritisak tipke za odlazak na prosli ekran
			{
				screenPicker = screenPicker-1;
				lcd_clrscr();
				_delay_ms(200);
			}
			
		break;
		}

		//***************************************************
		// paljenje i gasenje
		//***************************************************
		
		if (gateFlag == false)									// ovaj dio koda ce se izvrsavat samo ako su vrata zatvorena (nisu bila otvorena u odredjenom intervalu)
		{
			if ( (psat > hourONtime) && (psat < hourOFFtime) )	// ovaj dio gleda kada su sati > < 
				light = true;
			else if (psat == hourONtime && pmin >= minuteONtime) // ovaj dio gleda za tocno odredeni sat i minute ON TIME
				light = true;
			else if (psat == hourOFFtime && pmin < minuteOFFtime)  // ovaj dio gleda za tocno odredeni sat i minute OFF TIME
				light = true;
			else
				light = false;
		}
//******** gate stuff ***********************************************
		if ((psat > hourOFFtime) && (psat < gateOFFtimeH))		// ovo ce se izvrsavat od trenutka gasenja svijetla do gate off time
			gateEvent();
		else if ((psat == hourOFFtime) && (pmin >= minuteOFFtime))
			gateEvent();
		else if (psat == gateOFFtimeH && pmin <= gateOFFtimeM )
			gateEvent();
		else if (gateOFFtimeH < hourOFFtime)		// ako je vrijeme gasenja od vrata manje od OFF time 
		{											// podjeli na dva intervala i provjeri u kojem je
			if (psat > hourOFFtime && psat <= 24)	// od gasenja do ponoci
				gateEvent();
			if (psat < gateOFFtimeH)			// od 01 do gasenja od vrata
				gateEvent();
		}
		
		if (gateFlag == true)		// ako su vrata otvorena/bila otvorena pocni odbrojavanje
			gateTimer();
		if (gateFlag == true)		// ako su vrata otvorena/bila otvorena (i unutar intervala od timera) upali sv
			light = true;
		
		if (light)			//upali svjetlo ovisno o zastavici za light
			turnON();		
		else
			turnOFF();		//ugasi svjetlo ovisno o zastavici za light
												

			

    }
}