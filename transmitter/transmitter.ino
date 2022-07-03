#define DEBUG

#ifdef DEBUG
  #define DEBUG_PRINT(x)    Serial.print (x)
  #define DEBUG_PRINTDEC(x) Serial.print (x, DEC)
  #define DEBUG_PRINTHEX(x) Serial.print (x, HEX)
  #define DEBUG_PRINTARRAYHEX(x, y) for(int i = 0; i<y; i++) Serial.print (x[i], HEX)
  #define DEBUG_PRINTARRAYHEXB(x, y) for(int i = 0; i<y; i++) {Serial.print ("0x");Serial.print (x[i], HEX);Serial.print (" ");}
  #define DEBUG_PRINTARRAYASCII(x, y) for(int i = 0; i<y; i++) Serial.write (x[i])
  #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTARRAYHEX(x, y)
  #define DEBUG_PRINTARRAYHEXB(x, y)
  #define DEBUG_PRINTARRAYASCII(x, y)
  #define DEBUG_PRINTLN(x)
#endif

#include "CRC.h"
#include <SoftwareSerial.h>

#define maxDataSize 256

uint8_t EDFBuffer[maxDataSize+30];
int EDFSize = 0;

uint8_t source_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
uint8_t destination_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

uint8_t input[maxDataSize];
int input_i = 0;
bool inputComplete = false;

SoftwareSerial mySerial(10, 11); // RX, TX

void setup() {
  Serial.begin(57600);
  mySerial.begin(4800);
}


void loop() {
   if (Serial.available()) {
    uint8_t inChar = (uint8_t)Serial.read();
    if (inChar == '\n') {
      inputComplete = true;
    }
    else{
      input[input_i] += inChar;
      input_i++;
    }
  }

  if (inputComplete) {
    encodeEthernetDataFrame(destination_MAC, source_MAC, input, input_i);

    for(int i = 0; i<EDFSize; i++) mySerial.write(EDFBuffer[i]);
    mySerial.write('\n');

    inputComplete = false;
    for(int i = 0; i < input_i+1; i++)input[i] = 0;
    input_i = 0;
  }
}

/*
Ethernet Frame Details:
+----------+-----------------------+-----------------+------------+---------------------+----------------+-----------------------------------+
| Preamble | Start frame delimiter | MAC destination | MAC source | length (IEEE 802.3) |    Payload     | Frame check sequence (32‑bit CRC) |
+----------+-----------------------+-----------------+------------+---------------------+----------------+-----------------------------------+
| 7 octets | 1 octet               | 6 octets        | 6 octets   | 2 octets            | 46-1500 octets | 4 octets                          |
+----------+-----------------------+-----------------+------------+---------------------+----------------+-----------------------------------+
*/

void encodeEthernetDataFrame(uint8_t destinationMAC[6], uint8_t sourceMAC[6], uint8_t data[maxDataSize], int dataSize){
  // Clear Buffer
  for(int i = 0; i< maxDataSize+30; i++) EDFBuffer[i] = 0x00;

  // Latch minimum data to 46 charachters
  if (dataSize < 46){
    for(int i = dataSize; i < 47; i++){
      data[i] = 0x00;
    }
    dataSize = 46;
  }

  // Add Preamble Frame -> byte 0 to 6 -> 7 bytes
  for(int i = 0; i < 7; i++)EDFBuffer[i] = 0xAA;

  // Add Start Frame Delimeter -> byte 7 -> 1 byte
  EDFBuffer[7] = 0xAB;

  // Add Destination MAC address  -> byte 8 to 13 -> 6 bytes
  for(int i = 8; i < 14; i++)EDFBuffer[i] = destinationMAC[i-8];

  // Add Source MAC Address -> byte 14 to 19 -> 6 bytes
  for(int i = 14; i < 20; i++)EDFBuffer[i] = sourceMAC[i-14];

  // Add Length Frame -> bytes 20 and 21 -> 2 bytes
  EDFBuffer[20] = dataSize / 256; // First byte of size frame
  EDFBuffer[21] = dataSize % 256; // Second byte of size frame

  // Add Data Frame -> bytes 22 to dataSize+22 (max 1522) (min 68) -> 46 - 1500 bytes
  for(int i = 22; i < dataSize+23; i++)EDFBuffer[i] = data[i-22];

  // Calculate CRC32
  uint32_t crc = crc32(data, dataSize, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true, false);

  // Add CRC Frame -> bytes dataSize+22 to dataSize+26 -> 4 bytes
  EDFBuffer[dataSize+26] = (crc & 0x000000ff);
  EDFBuffer[dataSize+25] = (crc & 0x0000ff00) >> 8;
  EDFBuffer[dataSize+24] = (crc & 0x00ff0000) >> 16;
  EDFBuffer[dataSize+23] = (crc & 0xff000000) >> 24;

  // Calculate total frame size
  EDFSize = dataSize+27;

  DEBUG_PRINT("Encoded ethernet frame in HEX: \t");
  DEBUG_PRINTARRAYHEXB(EDFBuffer, EDFSize);
  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("");
}
