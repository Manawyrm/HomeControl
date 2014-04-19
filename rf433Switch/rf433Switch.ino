#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <RCSwitch.h>

String address433 = "11111";

byte receiveBuffer[32];
byte transmitBuffer[32];

uint16_t ownAddress = 15;
uint8_t tempAddr[5];

RCSwitch mySwitch = RCSwitch();

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


void setup()
{
	Serial.begin(9600);
	Mirf.spi = &MirfHardwareSpi;   
	Mirf.init();
	Mirf.setRADDR(buildAddress(ownAddress));
	Mirf.payload = 32;
	Mirf.config();

	mySwitch.enableTransmit(10);
}

void loop()
{
	if(!Mirf.isSending() && Mirf.dataReady())
	{
		Mirf.getData(receiveBuffer);
		if (validateChecksum(receiveBuffer))
		{
			address433.setCharAt(0,receiveBuffer[2]);
			address433.setCharAt(1,receiveBuffer[3]);
			address433.setCharAt(2,receiveBuffer[4]);
			address433.setCharAt(3,receiveBuffer[5]);
			address433.setCharAt(4,receiveBuffer[6]);

			

			mySwitch.switchOn(address433, 2); 

			Mirf.setTADDR(buildAddress(receiveBuffer[0],receiveBuffer[1]));

			memcpy(transmitBuffer,"  switched                      ",32);
			transmitBuffer[0] = ownAddress & 0xFF;
			transmitBuffer[1] = (ownAddress >> 8) & 0xFF;

			transmitBuffer[31] = generateChecksum(transmitBuffer);
			Mirf.send((byte *)transmitBuffer);


			while(Mirf.isSending())	{}
		}
	}
}
