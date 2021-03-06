#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <EEPROM.h>
#include <MirfHardwareSpiDriver.h>
#include <OneWire.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define WAIT_SLEEPING	0
#define WAIT_TEMP		1
#define WAIT_ADC		2
#define WAIT_SEND		3

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif 

byte receiveBuffer[32];
byte transmitBuffer[32];

byte i;
byte present = 0;
byte type_s;
byte data[12];
byte addr[8];
float celsius;

uint16_t ownAddress;
uint16_t sleep_length;
uint8_t tempAddr[5];

volatile uint8_t state = WAIT_SLEEPING;
volatile uint16_t sleepcounter = 0;

OneWire  ds(6);

uint8_t generateChecksum(uint8_t* packet)
{
	uint8_t checksum = 0x42;
	for(int i=0; i < 31; i++)
	{
		checksum ^= packet[i];
	}
	return checksum;
}

uint8_t validateChecksum(uint8_t* packet)
{
	uint8_t checksum = 0x42;
	for(int i=0; i < 31; i++)
	{
		checksum ^= packet[i];
	}
	return (packet[31] == checksum);
}

uint8_t* buildAddress(uint16_t device)
{
	tempAddr[0] = 't';
	tempAddr[1] = 'm';
	tempAddr[2] = 'a';
	tempAddr[3] = device & 0xFF;
	tempAddr[4] = (device >> 8) & 0xFF;
	return tempAddr;
}
uint8_t* buildAddress(uint8_t lowerByte, uint8_t upperByte)
{
	tempAddr[0] = 't';
	tempAddr[1] = 'm';
	tempAddr[2] = 'a';
	tempAddr[3] = lowerByte;
	tempAddr[4] = upperByte;
	return tempAddr;
}

void setup()
{
	cbi(ADCSRA, ADEN);

	pinMode(A5, OUTPUT); // LED
	pinMode(A4, OUTPUT); // Funkchip VCC
	Serial.begin(9600);

	MCUSR = MCUSR & B11110111;
	WDTCSR = WDTCSR | B00011000; 
	WDTCSR = (1<<WDP2) | (1<<WDP1);
	// Interrupt aktivieren
	WDTCSR |= B01000000;
	MCUSR = MCUSR & B11110111;


	ownAddress = EEPROM.read(0) | (EEPROM.read(1) << 8);
	sleep_length = EEPROM.read(2) | (EEPROM.read(3) << 8);

	Mirf.spi = &MirfHardwareSpi;   
	Mirf.init();

	Mirf.setRADDR(buildAddress(ownAddress));
	Mirf.payload = 32;
	//Mirf.configRegister(RF_SETUP, B00001110);
	Mirf.config();
	Mirf.powerDown();

	if ( !ds.search(addr)) {
		ds.reset_search();
		delay(250);
		return;
	}
	switch (addr[0]) {
		case 0x10:
		  type_s = 1;
		  break;
		case 0x28:
		  type_s = 0;
		  break;
		case 0x22:
		  type_s = 0;
		  break;
		default:
		  return;
	} 

}
void startADconversion()
{
	ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
	ADCSRA |= _BV(ADEN) | (1<<ADIE); 
}
long readVcc() 
{
	cbi(ADCSRA, ADEN);

	long result;
	result = ADCL;
	result |= ADCH<<8;
	result = 1126400L / result;
	return result - 100; // OFfset?!
}

void loop()
{
	// Wenn der Controller noch im Sleepmode ist und noch nicht lang genug geschlafen hat -- weiterschlafen
	if ((state == WAIT_SLEEPING) && (sleepcounter != sleep_length))
	{
		// Gute Nacht!
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	}
	
	if ((state == WAIT_SLEEPING) && (sleepcounter == sleep_length))
	{
		//Aktuellen Zustand setzen = Warte auf Fertigstellung der Messung
		state = WAIT_TEMP;

		//Messung anstoßen
		startMeasure();

		// Gute Nacht!
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	}
	if (state == WAIT_ADC)
	{
		startADconversion();
		set_sleep_mode(SLEEP_MODE_ADC);
		digitalWrite(A4, 0);
		digitalWrite(A5, 1);

		//if (!(ADCSRA & (1<<ADSC)))
		//{
		//	state = WAIT_SEND;
		//}
	}	
	if (state == WAIT_SEND)
	{ 
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		digitalWrite(A4, 1);
		digitalWrite(A5, 0);
		// Sekunde vorbei, Messwerte sind fertig.		
		float temp_float = readTemperature();
		byte * temp = (byte *) &temp_float;

		long vcc_long = readVcc();
		//long vcc_long = 3444;
		byte * vcc = (byte *) &vcc_long;
		// Sendepuffer mit Leerzeichen überschreiben
		memset(transmitBuffer,32,32);

		// Eigene Adresse
		transmitBuffer[0] = ownAddress & 0xFF;
		transmitBuffer[1] = (ownAddress >> 8) & 0xFF;

		transmitBuffer[2] = 0x02;

		// 64bit Adresse des Sensors
		transmitBuffer[3] = addr[0];
		transmitBuffer[4] = addr[1];
		transmitBuffer[5] = addr[2];
		transmitBuffer[6] = addr[3];
		transmitBuffer[7] = addr[4];
		transmitBuffer[8] = addr[5];
		transmitBuffer[9] = addr[6];
		transmitBuffer[10] = addr[7];
		
		// Aktueller Messwert
		transmitBuffer[11] = temp[0];
		transmitBuffer[12] = temp[1];
		transmitBuffer[13] = temp[2];
		transmitBuffer[14] = temp[3]; 
		
		// Betriebsspannung
		transmitBuffer[15] = vcc[0];
		transmitBuffer[16] = vcc[1];
		transmitBuffer[17] = vcc[2];
		transmitBuffer[18] = vcc[3]; 

		transmitBuffer[31] = generateChecksum((uint8_t *)transmitBuffer);

		Mirf.setTADDR(buildAddress(0));
		Mirf.send((byte *)transmitBuffer);
		while(Mirf.isSending())	{}
		Mirf.powerDown();

		// Fertig.  Gute Nacht!
		state = WAIT_SLEEPING;
		sleepcounter = 0;
	}
	wdt_reset();
	sleep_enable();
	sleep_cpu();
	digitalWrite(A4, 1);
		digitalWrite(A5, 0);
}

ISR(WDT_vect)
{

	//Wieder aus dem Watchdog-Schlaf aufgewacht.
	if (state == WAIT_SLEEPING)
	{
		sleepcounter++;
	}
	if (state == WAIT_TEMP)
	{
		state = WAIT_ADC;
	}
}
ISR(ADC_vect)
{
	state = WAIT_SEND;
	digitalWrite(A4, 1);
	digitalWrite(A5, 1);
}


void startMeasure()
{
	ds.reset();
	ds.select(addr);
	ds.write(0x44, 1);        // start conversion, with parasite power on at the end
}

float readTemperature()
{
	present = ds.reset();
	ds.select(addr);    
	ds.write(0xBE);

	for ( i = 0; i < 9; i++) 
	{
		data[i] = ds.read();
	}

	// Convert the data to actual temperature
	// because the result is a 16 bit signed integer, it should
	// be stored to an "int16_t" type, which is always 16 bits
	// even when compiled on a 32 bit processor.
	int16_t raw = (data[1] << 8) | data[0];
	if (type_s) {
	raw = raw << 3; // 9 bit resolution default
	if (data[7] == 0x10) {
	  // "count remain" gives full 12 bit resolution
	  raw = (raw & 0xFFF0) + 12 - data[6];
	}
	} else {
	byte cfg = (data[4] & 0x60);
	// at lower res, the low bits are undefined, so let's zero them
	if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
	else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
	else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
	//// default is 12 bit resolution, 750 ms conversion time
	}
	return (float)raw / 16.0;
}