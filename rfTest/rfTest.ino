#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>

byte receiveBuffer[32];
byte transmitBuffer[32];

uint16_t ownAddress = 20;
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

void setup()
{
	pinMode(A2, OUTPUT);
	pinMode(A3, OUTPUT);
	digitalWrite(A2, HIGH);
	Serial.begin(9600);

	Mirf.spi = &MirfHardwareSpi;   
	Mirf.init();
	Mirf.setRADDR(buildAddress(ownAddress));
	Mirf.payload = 32;
	Mirf.config();
	digitalWrite(A2, LOW);

	delay (5000);

	digitalWrite(A3, HIGH);

	Mirf.setTADDR(buildAddress(0));

	memcpy(transmitBuffer,"  testsignal                    ",32);
	transmitBuffer[0] = ownAddress & 0xFF;
	transmitBuffer[1] = (ownAddress >> 8) & 0xFF;
	Mirf.send((byte *)transmitBuffer);

	delay(100);
		digitalWrite(A3, LOW);


}

void loop()
{
	while(Mirf.isSending())	{}
	delay(100);
}
