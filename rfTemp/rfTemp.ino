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
	pinMode(A3, OUTPUT); // LED
	pinMode(A0, OUTPUT); // Funkchip VCC
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

void loop()
{
	// Wenn der Controller noch im Sleepmode ist und noch nicht lang genug geschlafen hat -- weiterschlafen
	if ((state == WAIT_SLEEPING) && (sleepcounter != sleep_length))
	{
		// Gute Nacht!
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		wdt_reset();
		sleep_mode();

		return;
	}
	
	if ((state == WAIT_SLEEPING) && (sleepcounter == sleep_length))
	{
		//Aktuellen Zustand setzen = Warte auf Fertigstellung der Messung
		state = WAIT_TEMP;

		//Messung anstoßen
		startMeasure();

		// Gute Nacht!
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		wdt_reset();

		// In ner Sekunde gehts weiter.
		sleep_enable();
        sleep_cpu();
		return;
	}
	if (state == WAIT_TEMP)
	{
		// Sekunde vorbei, Messwerte sind fertig.		
		float temp_float = readTemperature();
		byte * temp = (byte *) &temp_float;

		//long vcc_long = readVcc();
		long vcc_long = 3444;
		byte * vcc = (byte *) &vcc_long;

	    // Sendepuffer mit Leerzeichen überschreiben
		memset(transmitBuffer,32,32);

		// Eigene Adresse
		transmitBuffer[0] = ownAddress & 0xFF;
		transmitBuffer[1] = (ownAddress >> 8) & 0xFF;

		// 64bit Adresse des Sensors
		transmitBuffer[2] = addr[0];
		transmitBuffer[3] = addr[1];
		transmitBuffer[4] = addr[2];
		transmitBuffer[5] = addr[3];
		transmitBuffer[6] = addr[4];
		transmitBuffer[7] = addr[5];
		transmitBuffer[8] = addr[6];
		transmitBuffer[9] = addr[7];
		
		// Aktueller Messwert
		transmitBuffer[10] = temp[0];
    	transmitBuffer[11] = temp[1];
   		transmitBuffer[12] = temp[2];
    	transmitBuffer[13] = temp[3]; 
		
		// Betriebsspannung
		transmitBuffer[14] = vcc[0];
    	transmitBuffer[15] = vcc[1];
   		transmitBuffer[16] = vcc[2];
    	transmitBuffer[17] = vcc[3]; 

    	transmitBuffer[31] = generateChecksum((uint8_t *)transmitBuffer);

    	Mirf.setTADDR(buildAddress(0));
		Mirf.send((byte *)transmitBuffer);
		while(Mirf.isSending())	{}
		Mirf.powerDown();

		// Fertig.  Gute Nacht!
		state = WAIT_SLEEPING;
		sleepcounter = 0;
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		wdt_reset();
		sleep_enable();
        sleep_cpu();

		return;

	}	
}
long readVcc() {
  long result;
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); 
  ADCSRA |= _BV(ADSC); 
  while (bit_is_set(ADCSRA,ADSC))
  {
	set_sleep_mode(SLEEP_MODE_ADC);
	sleep_enable();
    sleep_cpu();  
	}
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result;
  return result - 100; // OFfset?!
}

ISR(WDT_vect)
{

	//Wieder aus dem Watchdog-Schlaf aufgewacht.
	if (state == WAIT_SLEEPING)
	{
		sleepcounter++;
	}
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