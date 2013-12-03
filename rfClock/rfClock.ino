#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include "Wire.h"
#include "TwoWireRTC.h"
#include "DateTime.h"

byte data[32];
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
	Mirf.setRADDR(buildAddress(10));
	Mirf.payload = 32;
	Mirf.config();
}

void loop()
{
	if(!Mirf.isSending() && Mirf.dataReady())
	{
		Mirf.getData(data);
		DateTime newtime = DateTime(
									data[0],
									data[1],
									data[2],
									data[3],
									data[4],
									data[5],
									data[6]
									);
		RTC.setTime(&newtime);
		Serial.println((char *)data);
		Mirf.setTADDR(buildAddress(0));
		Mirf.send((byte *)"clockset                        ");
		while(Mirf.isSending())	{}
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
