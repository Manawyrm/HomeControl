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
byte receiveBuffer[32];
byte transmitBuffer[32];

uint8_t tempAddr[5];
uint16_t ownAddress;

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
	Serial.begin(9600);


	ownAddress = EEPROM.read(0) | (EEPROM.read(1) << 8);

	Mirf.spi = &MirfHardwareSpi;   
	Mirf.init();

	Mirf.setRADDR(buildAddress(ownAddress));
	Mirf.payload = 32;
	//Mirf.configRegister(RF_SETUP, B00001110);
	Mirf.config();
	Mirf.powerDown();


}

void loop()
{
		memset(transmitBuffer,32,32);

		// Eigene Adresse
		transmitBuffer[0] = ownAddress & 0xFF;
		transmitBuffer[1] = (ownAddress >> 8) & 0xFF;

		// 64bit Adresse des Sensors
		transmitBuffer[2] = 0x47;

		transmitBuffer[31] = generateChecksum((uint8_t *)transmitBuffer);

		Mirf.setTADDR(buildAddress(0));
		Mirf.send((byte *)transmitBuffer);
		while(Mirf.isSending())	{}
		Mirf.powerDown();


}

