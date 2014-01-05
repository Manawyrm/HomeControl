#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include "utility/RS5C372A.h"
#include "TwoWireRTC.h"
#include "utility/twi.h"
#include "Wire.h"
#include "DateTime.h"




byte receiveBuffer[32];
byte transmitBuffer[32];

uint16_t ownAddress = 10;
int latchPin = A2;
int clockPin = A1;
int dataPin = A0;
DateTime time;

byte numbers[] = {
B11111101,
B01100001,
B11011011,
B11110011,
B01100111,
B10110111,
B10111111,
B11100001,
B11111111,
B11110111
};

uint8_t tempAddr[5];

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

    Wire.begin();
    RTC.begin();
    pinMode(latchPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(dataPin, OUTPUT);

	Mirf.spi = &MirfHardwareSpi;   
	Mirf.init();
	Mirf.setRADDR(buildAddress(ownAddress));
	Mirf.payload = 32;
	Mirf.config();
}

void loop()
{
	if(!Mirf.isSending() && Mirf.dataReady())
	{
		Mirf.getData(receiveBuffer);

		if (validateChecksum(receiveBuffer))
		{
			DateTime newtime = DateTime(
                                                        receiveBuffer[2],
                                                        receiveBuffer[3],
                                                        receiveBuffer[4],
                                                        receiveBuffer[5],
                                                        receiveBuffer[6],
                                                        receiveBuffer[7],
                                                        receiveBuffer[8]
                                                        );

			RTC.setTime(&newtime);
			Mirf.setTADDR(buildAddress(receiveBuffer[0],receiveBuffer[1]));

			memcpy(transmitBuffer,"  clockset                      ",32);
			transmitBuffer[0] = ownAddress & 0xFF;
			transmitBuffer[1] = (ownAddress >> 8) & 0xFF;

			transmitBuffer[31] = generateChecksum(transmitBuffer);
			Mirf.send((byte *)transmitBuffer);


			while(Mirf.isSending())	{}
		}
	}
    RTC.getTime(&time);
    digitalWrite(latchPin, LOW);

    shiftOut(dataPin, clockPin, MSBFIRST, ~numbers[(int)time.getSecond() % 10]);  //Sekunde 2
    shiftOut(dataPin, clockPin, MSBFIRST, ~numbers[(int)time.getSecond() / 10]);  //Sekunde 1
    shiftOut(dataPin, clockPin, MSBFIRST, ~numbers[(int)time.getMinute() % 10]);  //Minute 2
    shiftOut(dataPin, clockPin, MSBFIRST, ~numbers[(int)time.getMinute() / 10]);  //Minute 1
    shiftOut(dataPin, clockPin, MSBFIRST, ~numbers[(int)time.getHour() % 10]);  //Stunde 2
    shiftOut(dataPin, clockPin, MSBFIRST, ~numbers[(int)time.getHour() / 10]);  //Stunde 1
    digitalWrite(latchPin, HIGH);
	delay(100);
}
