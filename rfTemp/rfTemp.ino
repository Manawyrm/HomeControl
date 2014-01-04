#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <OneWire.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define WAIT_SLEEPING	0
#define WAIT_TEMP		1
#define WAIT_ADC		2

#define SLEEP_LEN	2 					// Interval in Sekunden zur Messung und Übermittlung der Messwerte

#define SLEEP_LEN_MODE 	WDTO_1S				// Verwendeter Watchdog-Clock Modus


byte receiveBuffer[32];
byte transmitBuffer[32];

byte i;
byte present = 0;
byte type_s;
byte data[12];
byte addr[8];
float celsius;

uint16_t ownAddress = 21;
uint8_t tempAddr[5];

volatile uint8_t state = WAIT_SLEEPING;
volatile uint8_t sleepcounter = 0;

OneWire  ds(6);

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
	pinMode(A3, OUTPUT);
	Serial.begin(9600);

	MCUSR = MCUSR & B11110111;
	WDTCSR = WDTCSR | B00011000; 
	WDTCSR = (1<<WDP2) | (1<<WDP1);

	// Interrupt aktivieren
	WDTCSR |= B01000000;

	MCUSR = MCUSR & B11110111;

	Mirf.spi = &MirfHardwareSpi;   
	Mirf.init();
	Mirf.setRADDR(buildAddress(ownAddress));
	Mirf.payload = 32;
	Mirf.config();

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
	if ((state == WAIT_SLEEPING) && (sleepcounter != SLEEP_LEN))
	{
		// Gute Nacht!
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		wdt_reset();
		sleep_mode();

		return;
	}
	
	if ((state == WAIT_SLEEPING) && (sleepcounter == SLEEP_LEN))
	{
		//Aktuellen Zustand setzen = Warte auf Fertigstellung der Messung
		state = WAIT_TEMP;

		//Messung anstoßen
		startMeasure();

		// Gute Nacht!
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		wdt_reset();

		// In ner Sekunde gehts weiter.
		sleep_mode();
		return;
	}
	if (state == WAIT_TEMP)
	{
		// Sekunde vorbei, Messwerte sind fertig.
		state = WAIT_SLEEPING;

		
		float temp_float = readTemperature();
		byte * temp = (byte *) &temp_float;

	    Mirf.setTADDR(buildAddress(0));

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
		
		Mirf.send((byte *)transmitBuffer);
		while(Mirf.isSending())	{}
	}
	/*digitalWrite(A3, HIGH);

    Mirf.setTADDR(buildAddress(0));

	memcpy(transmitBuffer,"  Temperatur:                   ",32);
	transmitBuffer[0] = ownAddress & 0xFF;
	transmitBuffer[1] = (ownAddress >> 8) & 0xFF;
	Mirf.send((byte *)transmitBuffer);

	delay(100);
	digitalWrite(A3, LOW);

	while(Mirf.isSending())	{}
	delay(100);*/
	
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
	ds.write(0x44, 0);        // start conversion, with parasite power on at the end
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