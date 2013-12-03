#include <SPI.h>
#include <Ethernet.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>

uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };
EthernetServer server(80);
uint16_t transmitAddr;
uint8_t payloadBuffer[33];

uint8_t dataInputBuffer[100];
String dataBuffer;
String dataInput;

uint16_t packetInputBuffer[4];
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
uint8_t* getPacketFromBuffer(uint8_t packet)
{
	//return packetInputBuffer[packet];
}
uint8_t* getPacketForReceiver()
{
		
}
void setup() 
{
	// Serielle Kommunikation initialisieren
	Serial.begin(9600);

	// Ethernet-Modul initialisieren und IP via DHCP anfordern
	if (Ethernet.begin(mac) == 0) {
		//Serial.println("Failed to configure Ethernet using DHCP");
		for(;;)
			;
	}
	// IP erhalten. Bereitschaft via UART melden.
	//Serial.println("Got IP");

	for (int i = 0; i < 4; i++)
	{
		packetInputBuffer[i] = 0;
	}



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
						
						// Valide Datenmenge ist 34 Bytes (2 Byte Adresse und 32 Bytes Nutzdaten)
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
						Serial.println((char *) buildAddress(transmitAddr));
						//Serial.println((char *) payloadBuffer);

						// Nutzdaten in den Sendepuffer schreiben
						Mirf.send((uint8_t *)payloadBuffer);
						// Auf Abschluss der Übertragung warten
						while(Mirf.isSending())	{}
						// Aktuelle Zeit loggen
						unsigned long time = millis();
						delay(10);
						// Solange keine Antwort da ist...
						while(!Mirf.dataReady()){
							// ist schon ne Sekunde rum? 
							if ( ( millis() - time ) > 1000 ) 
							{
								// Upps ): 
								// Keine Antwort. Abbruch.
								client.println("Timeout on response");
								client.stop();
								break;
							}
						}
						// Antwort da. Lesen und in payloadBuffer schreiben
						Mirf.getData((uint8_t *) &payloadBuffer);
						// Letztes Byte des Nutzdaten-Puffers als String-Nullbyte-Terminator setzen.
						payloadBuffer[32] = 0x00;
						// An den TCP-Clienten (via HTTP) ausgeben
						client.print((char *)payloadBuffer);
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
}

