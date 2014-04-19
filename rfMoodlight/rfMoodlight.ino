#include "LPD8806.h"
#include "SPI.h"
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>

int dataPin = 17;
int clockPin = 16;
LPD8806 strip = LPD8806(42, dataPin, clockPin);

byte receiveBuffer[32];
byte transmitBuffer[32];

uint16_t ownAddress = 12;
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


void setup() {
	Mirf.spi = &MirfHardwareSpi;   
	Mirf.init();
	Mirf.setRADDR(buildAddress(ownAddress));
	Mirf.payload = 32;
	Mirf.config();

	strip.begin();
	strip.show();
}

// function prototypes, do not remove these!
void colorChase(uint32_t c, uint8_t wait);
void colorWipe(uint32_t c, uint8_t wait);
void dither(uint32_t c, uint8_t wait);
void scanner(uint8_t r, uint8_t g, uint8_t b, uint8_t wait);
void wave(uint32_t c, int cycles, uint8_t wait);
void rainbowCycle(uint8_t wait);
uint32_t Wheel(uint16_t WheelPos);

void loop() 
{
	if(!Mirf.isSending() && Mirf.dataReady())
	{
		Mirf.getData(receiveBuffer);
		if (validateChecksum(receiveBuffer))
		{
			Mirf.setTADDR(buildAddress(receiveBuffer[0],receiveBuffer[1]));

			memcpy(transmitBuffer,"  set                           ",32);
			transmitBuffer[0] = ownAddress & 0xFF;
			transmitBuffer[1] = (ownAddress >> 8) & 0xFF;

			transmitBuffer[31] = generateChecksum(transmitBuffer);
			Mirf.send((byte *)transmitBuffer);

			switch (receiveBuffer[2]) 
			{
				case 0x01:
					dither(strip.Color(receiveBuffer[3],receiveBuffer[4],receiveBuffer[5]),0);
					break;
				case 0x06:
					  rainbowCycle(0); 
					break;
				
			}

			while(Mirf.isSending())	{}
		}
	}


  /*// Send a simple pixel chase in...
  colorChase(strip.Color(127,127,127), 20); // white
  colorChase(strip.Color(127,0,0), 20);     // red
  colorChase(strip.Color(127,127,0), 20);   // yellow
  colorChase(strip.Color(0,127,0), 20);     // green
  colorChase(strip.Color(0,127,127), 20);   // cyan
  colorChase(strip.Color(0,0,127), 20);     // blue
  colorChase(strip.Color(127,0,127), 20);   // magenta

  // Fill the entire strip with...
  colorWipe(strip.Color(127,0,0), 20);      // red
  colorWipe(strip.Color(0, 127,0), 20);     // green
  colorWipe(strip.Color(0,0,127), 20);      // blue
  colorWipe(strip.Color(0,0,0), 20);        // black

  // Color sparkles
  dither(strip.Color(0,127,127), 50);       // cyan, slow
  dither(strip.Color(0,0,0), 15);           // black, fast
  dither(strip.Color(127,0,127), 50);       // magenta, slow
  dither(strip.Color(0,0,0), 15);           // black, fast
  dither(strip.Color(127,127,0), 50);       // yellow, slow
  dither(strip.Color(0,0,0), 15);           // black, fast

  // Back-and-forth lights
  scanner(127,0,0, 30);        // red, slow
  scanner(0,0,127, 15);        // blue, fast

  // Wavy ripple effects
  wave(strip.Color(127,0,0), 4, 20);        // candy cane
  wave(strip.Color(0,0,100), 2, 40);        // icy

  // make a pretty rainbow cycle!
 / make it go through the cycle fairly fast

  // Clear strip data before start of next effect
  for (int i=0; i < strip.numPixels(); i++) {
	strip.setPixelColor(i, 0);
  }*/ 
}

// Cycle through the color wheel, equally spaced around the belt
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j=0; j < 384 * 5; j++) {     // 5 cycles of all 384 colors in the wheel
	for (i=0; i < strip.numPixels(); i++) {
	  // tricky math! we use each pixel as a fraction of the full 384-color
	  // wheel (thats the i / strip.numPixels() part)
	  // Then add in j which makes the colors go around per pixel
	  // the % 384 is to make the wheel cycle around
	  strip.setPixelColor(i, Wheel(((i * 384 / strip.numPixels()) + j) % 384));
	}
	strip.show();   // write all the pixels out
	delay(wait);
  }
}

// fill the dots one after the other with said color
// good for testing purposes
void colorWipe(uint32_t c, uint8_t wait) {
  int i;

  for (i=0; i < strip.numPixels(); i++) {
	  strip.setPixelColor(i, c);
	  strip.show();
	  delay(wait);
  }
}

// Chase a dot down the strip
// good for testing purposes
void colorChase(uint32_t c, uint8_t wait) {
  int i;

  for (i=0; i < strip.numPixels(); i++) {
	strip.setPixelColor(i, 0);  // turn all pixels off
  }

  for (i=0; i < strip.numPixels(); i++) {
	  strip.setPixelColor(i, c); // set one pixel
	  strip.show();              // refresh strip display
	  delay(wait);               // hold image for a moment
	  strip.setPixelColor(i, 0); // erase pixel (but don't refresh yet)
  }
  strip.show(); // for last erased pixel
}

// An "ordered dither" fills every pixel in a sequence that looks
// sparkly and almost random, but actually follows a specific order.
void dither(uint32_t c, uint8_t wait) {

  // Determine highest bit needed to represent pixel index
  int hiBit = 0;
  int n = strip.numPixels() - 1;
  for(int bit=1; bit < 0x8000; bit <<= 1) {
	if(n & bit) hiBit = bit;
  }

  int bit, reverse;
  for(int i=0; i<(hiBit << 1); i++) {
	// Reverse the bits in i to create ordered dither:
	reverse = 0;
	for(bit=1; bit <= hiBit; bit <<= 1) {
	  reverse <<= 1;
	  if(i & bit) reverse |= 1;
	}
	strip.setPixelColor(reverse, c);
	strip.show();
	delay(wait);
  }
  delay(250); // Hold image for 1/4 sec
}

// "Larson scanner" = Cylon/KITT bouncing light effect
void scanner(uint8_t r, uint8_t g, uint8_t b, uint8_t wait) {
  int i, j, pos, dir;

  pos = 0;
  dir = 1;

  for(i=0; i<((strip.numPixels()-1) * 8); i++) {
	// Draw 5 pixels centered on pos.  setPixelColor() will clip
	// any pixels off the ends of the strip, no worries there.
	// we'll make the colors dimmer at the edges for a nice pulse
	// look
	strip.setPixelColor(pos - 2, strip.Color(r/4, g/4, b/4));
	strip.setPixelColor(pos - 1, strip.Color(r/2, g/2, b/2));
	strip.setPixelColor(pos, strip.Color(r, g, b));
	strip.setPixelColor(pos + 1, strip.Color(r/2, g/2, b/2));
	strip.setPixelColor(pos + 2, strip.Color(r/4, g/4, b/4));

	strip.show();
	delay(wait);
	// If we wanted to be sneaky we could erase just the tail end
	// pixel, but it's much easier just to erase the whole thing
	// and draw a new one next time.
	for(j=-2; j<= 2; j++) 
		strip.setPixelColor(pos+j, strip.Color(0,0,0));
	// Bounce off ends of strip
	pos += dir;
	if(pos < 0) {
	  pos = 1;
	  dir = -dir;
	} else if(pos >= strip.numPixels()) {
	  pos = strip.numPixels() - 2;
	  dir = -dir;
	}
  }
}

// Sine wave effect
#define PI 3.14159265
void wave(uint32_t c, int cycles, uint8_t wait) {
  float y;
  byte  r, g, b, r2, g2, b2;

  // Need to decompose color into its r, g, b elements
  g = (c >> 16) & 0x7f;
  r = (c >>  8) & 0x7f;
  b =  c        & 0x7f; 

  for(int x=0; x<(strip.numPixels()*5); x++)
  {
	for(int i=0; i<strip.numPixels(); i++) {
	  y = sin(PI * (float)cycles * (float)(x + i) / (float)strip.numPixels());
	  if(y >= 0.0) {
		// Peaks of sine wave are white
		y  = 1.0 - y; // Translate Y to 0.0 (top) to 1.0 (center)
		r2 = 127 - (byte)((float)(127 - r) * y);
		g2 = 127 - (byte)((float)(127 - g) * y);
		b2 = 127 - (byte)((float)(127 - b) * y);
	  } else {
		// Troughs of sine wave are black
		y += 1.0; // Translate Y to 0.0 (bottom) to 1.0 (center)
		r2 = (byte)((float)r * y);
		g2 = (byte)((float)g * y);
		b2 = (byte)((float)b * y);
	  }
	  strip.setPixelColor(i, r2, g2, b2);
	}
	strip.show();
	delay(wait);
  }
}

/* Helper functions */

//Input a value 0 to 384 to get a color value.
//The colours are a transition r - g - b - back to r

uint32_t Wheel(uint16_t WheelPos)
{
  byte r, g, b;
  switch(WheelPos / 128)
  {
	case 0:
	  r = 127 - WheelPos % 128; // red down
	  g = WheelPos % 128;       // green up
	  b = 0;                    // blue off
	  break;
	case 1:
	  g = 127 - WheelPos % 128; // green down
	  b = WheelPos % 128;       // blue up
	  r = 0;                    // red off
	  break;
	case 2:
	  b = 127 - WheelPos % 128; // blue down
	  r = WheelPos % 128;       // red up
	  g = 0;                    // green off
	  break;
  }
  return(strip.Color(r,g,b));
}