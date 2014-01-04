#include <SPI.h>
#include <Ethernet.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <EthernetUdp.h>

#define RX_BUFFER_CNT 4
#define UDP_PORT 44425

uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };
EthernetServer server(80);
EthernetUDP Udp;

uint16_t transmitAddr;
uint8_t payloadBuffer[33];

uint8_t dataInputBuffer[100];
String dataBuffer;
String dataInput;
int dataAvailable = 0;

uint8_t* packetInputBuffer[RX_BUFFER_CNT];
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
uint8_t getPacketSlotForReceiver(uint16_t device)
{
	for (uint8_t i = 0; i < RX_BUFFER_CNT; i++)
	{
		// Packet vorhanden
		if (packetInputBuffer[i] != 0)
		{
			uint8_t* currentPacket = packetInputBuffer[i];
			if ((currentPacket[0] == device & 0xFF) && (currentPacket[1] == (device >> 8) & 0xFF))
			{
				return i;
			}
		}
		
	}
	return false;
}
uint8_t getPacketSlot()
{
	for (uint8_t i = 0; i < RX_BUFFER_CNT; i++)
	{
		// Packet vorhanden
		if (packetInputBuffer[i] != 0)
		{
			return i;
		}
		
	}
	return false;
}

uint8_t getPacketSlotForReceiver(uint8_t lowerByte, uint8_t upperByte)
{
	for (uint8_t i = 0; i < RX_BUFFER_CNT; i++)
	{
		// Packet vorhanden
		if (packetInputBuffer[i] != 0)
		{
			uint8_t* currentPacket = packetInputBuffer[i];
			if ((currentPacket[0] == lowerByte) && (currentPacket[1] == upperByte))
			{
				return i;
			}
		}
		
	}
	return false;
}
uint8_t isPacketAvailable(uint16_t device)
{
	for (uint8_t i = 0; i < RX_BUFFER_CNT; i++)
	{
		// Packet vorhanden
		if (packetInputBuffer[i] != 0)
		{
		
			uint8_t* currentPacket = (uint8_t*)packetInputBuffer[i];
			if ((currentPacket[0] == device & 0xFF) && (currentPacket[1] == (device >> 8) & 0xFF))
			{
				return true;
			}
		}
	}
	return false;
}
uint8_t isPacketAvailable(uint8_t lowerByte, uint8_t upperByte)
{
	for (uint8_t i = 0; i < RX_BUFFER_CNT; i++)
	{
		// Packet vorhanden
		if (packetInputBuffer[i] != 0)
		{
			uint8_t* currentPacket = (uint8_t*)packetInputBuffer[i];
			if ((currentPacket[0] == lowerByte) && (currentPacket[1] == upperByte))
			{
				return true;
			}
		}
	}
	return false;
}

uint8_t getNextEmptyBufferSlot()
{
	for (uint8_t i = 0; i < RX_BUFFER_CNT; i++)
	{
		/*	Serial.print("Durchlauf: ");
			Serial.println(i);
			Serial.println((word)packetInputBuffer[i], HEX);
		*/

		// Packet nicht vorhanden
		if (packetInputBuffer[i] == 0)
		{
			return i;
		}
	}

	// Oh. Keine Puffer mehr frei. Hm. Alle Packets droppen.
	dropAllPackets();
	// Jetzt rekursiv einfach nochmal iterieren (wird dann ja der erste Slot)
    return getNextEmptyBufferSlot();
}
void dropAllPackets()
{
	for (uint8_t i = 0; i < RX_BUFFER_CNT; i++)
	{
		free(packetInputBuffer[i]);
		packetInputBuffer[i] = 0;
	}
}
void readPacketIntoBuffer()
{
	dataAvailable = 0;
	uint8_t  emptySlot     = getNextEmptyBufferSlot();
	uint8_t* receiveBuffer = (uint8_t *) malloc(32) ;
	memset (receiveBuffer,0,32);
	packetInputBuffer[emptySlot] = receiveBuffer;
	/*Serial.print("allocating memory:");
	Serial.println((word)packetInputBuffer[emptySlot], HEX);*/
    Mirf.getData((uint8_t *) receiveBuffer);
	//receiveBuffer[32] = 0x00;
}
void setup() 
{
	// Serielle Kommunikation initialisieren
	Serial.begin(9600);
	pinMode(9, OUTPUT);
	digitalWrite(9, LOW);
	pinMode(3, INPUT);
	//digitalWrite(3, HIGH);
	// Ethernet-Modul initialisieren und IP via DHCP anfordern
	if (Ethernet.begin(mac) == 0) {
		//Serial.println("Failed to configure Ethernet using DHCP");
		for(;;)
			;
	}
	Udp.begin(UDP_PORT);
	// IP erhalten. Bereitschaft via UART melden.
	Serial.println("Got IP");

	for (int i = 0; i < 4; i++)
	{
		packetInputBuffer[i] = 0;
	}

	//attachInterrupt(1, packetReceive , FALLING );

	// Serielle Schnittstelle des Funkmoduls definieren
	Mirf.spi = &MirfHardwareSpi;
	// Funkmodul initialisieren
	Mirf.init();
	// Empfangsadresse des Gateways setzen
	Mirf.setRADDR(buildAddress(0));
	// Standard Nutzdatenpaketgröße setzen
	Mirf.payload = 32;
	// Einstellung in Chipregister übertragen
	Mirf.config();
}

uint8_t getVal(uint8_t c)
{
	 if(c >= '0' && c <= '9')
		 return (uint8_t)(c - '0');
	 else
		 return (uint8_t)(c-'A'+10);
}

void loop() 
{
		EthernetClient client = server.available();
		if (client) 
		{
			dataBuffer = "";
			dataInput  = "";
			boolean currentLineIsBlank = true;
			while (client.connected()) 
			{
				if (client.available()) 
				{
					uint8_t c = client.read();
					dataBuffer += (char) c;
					if (c == '\n' && currentLineIsBlank) 
					{
						// Einfachen HTTP-Header senden.
						client.println("HTTP/1.1 200 OK");
						client.println("Content-Type: text/plain");
						client.println();
						
						// Daten rausfrickeln und als String herausgeben
						dataInput = dataBuffer.substring(dataBuffer.indexOf("GET /?")+6,dataBuffer.indexOf(" HTTP/"));
						
						// Valide Datenmenge ist 34 Bytes (2 Byte Empfängeradresse, 2 Byte Senderadresse und 30 Bytes Nutzdaten)
						// Wenn die Länge der Inputdaten nicht 68 Zeichen (34-Hex-Bytes) beträgt...
						if (dataInput.length() != 66)
						{   // ... abbruch.
							client.println("Invalid argument length");
							client.stop();
							break;
						}
						// String zur mathematischen Verarbeitung in Char-Array wandeln.
						dataInput.toCharArray((char *) dataInputBuffer, dataInput.length() + 1);
						// Unbenutzte Variable leeren
						dataInput  = "";

						// Gehe jedes 2te Byte im dataInputBuffer Char-Array durch
						for(int i = 0; dataInputBuffer[i] != 0;i+=2)
						{
								// Ist das aktuell verarbeitete Byte noch innerhalb des Adressbereichs? 
								if ((i >= 0) && (i <= 3))
								{   // Ja. 
									if (i == 0)
									{
										transmitAddr = (getVal(dataInputBuffer[i+1]) + (getVal(dataInputBuffer[i]) << 4));

									}
									else
									{
										transmitAddr |= (getVal(dataInputBuffer[i+1]) + (getVal(dataInputBuffer[i]) << 4)) << 8;
									}
									
								}
								else
								{   // Nein.
									payloadBuffer[(i / 2) - 2] = getVal(dataInputBuffer[i+1]) + (getVal(dataInputBuffer[i]) << 4);
								}
						}
						// Empfänger konfigurieren
						Mirf.setTADDR(buildAddress(transmitAddr));
						// Nutzdaten in den Sendepuffer schreiben
						Mirf.send((uint8_t *)payloadBuffer);
						// Auf Abschluss der Übertragung warten
						while(Mirf.isSending())	{}
						// Aktuelle Zeit loggen
						unsigned long time = millis();
						delay(10);

						// Solange keine Antwort da ist...
						while(!isPacketAvailable(transmitAddr)){
							// ist schon ne Sekunde rum? 
							if ( ( millis() - time ) > 1000 ) 
							{
								// Upps ): 
								// Keine Antwort. Abbruch.
								client.println("Timeout on response");
								client.stop();
								return;
							}
							if (Mirf.dataReady())
							{
								readPacketIntoBuffer();
							}
						}

						/*char* packetData = (char*)packetInputBuffer[getPacketSlotForReceiver(transmitAddr)];
						for (int i = 0; i < 32; i++)
						{
							Serial.write(packetData[i]);
						}
						Serial.println();*/

						// An den TCP-Clienten (via HTTP) ausgeben
						char* dataStorage = ((char *) packetInputBuffer[getPacketSlotForReceiver(transmitAddr)]);
						for (int i = 2; i < 32; i++)
						{
							client.write(dataStorage[i]);
						}

						/*Serial.print("Der benutze Speicher: ");
						Serial.println((word)packetInputBuffer[getPacketSlotForReceiver(transmitAddr)], HEX);*/

						free(packetInputBuffer[getPacketSlotForReceiver(transmitAddr)]);
						packetInputBuffer[getPacketSlotForReceiver(transmitAddr)] = 0;
						// Verbindung beenden
						client.stop();
					}
					if (c == '\n') {
						currentLineIsBlank = true;
					} 
					else if (c != '\r') {
						currentLineIsBlank = false;
					}
				
				}
			}
		}

		// Wenn Daten im Empfangspuffer sind -- einlesen.
		if (Mirf.dataReady())
		{
			Serial.println("DATEN!");
			readPacketIntoBuffer();
			Serial.println("Gelesen.");
			//delay(1000);
			Serial.println("Versende.");
			Udp.beginPacket(Ethernet.dnsServerIP(), UDP_PORT);
			Udp.write((uint8_t *) packetInputBuffer[getPacketSlot()], 32);
			Udp.endPacket();

			Serial.println("loesche puffer.");
			free(packetInputBuffer[getPacketSlot()]);
			packetInputBuffer[getPacketSlot()] = 0;
		}
		
}
