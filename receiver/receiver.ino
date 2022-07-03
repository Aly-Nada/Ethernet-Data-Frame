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

uint8_t destinationMAC[6];
uint8_t sourceMAC[6];
uint8_t data[maxDataSize];
uint8_t sizeBuffer[2];
int dataSize;
int data_integrity;

uint8_t input[maxDataSize];
int input_i = 0;
bool inputComplete = false;

SoftwareSerial mySerial(10, 11); // RX, TX


void setup() {
  Serial.begin(57600);
  mySerial.begin(4800);
}


void loop() {
  if (mySerial.available()) {
    uint8_t inChar = (uint8_t)mySerial.read();
    if (inChar == '\n') {
      inputComplete = true;
    }
    else{
      input[input_i] += inChar;
      input_i++;
    }
  }
  
  if (inputComplete) {  
    decodeEthernetDataFrame(input);
    
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

void decodeEthernetDataFrame(uint8_t EDFBuffer[maxDataSize+30]){  
  // Get Destination MAC address  -> byte 8 to 13 -> 6 bytes
  for(int i = 8; i < 14; i++)destinationMAC[i-8] = EDFBuffer[i];
  
  // Get Source MAC Address -> byte 14 to 19 -> 6 bytes
  for(int i = 14; i < 20; i++)sourceMAC[i-14] = EDFBuffer[i];
  
  // Get Length Frame -> bytes 20 and 21 -> 2 bytes
  sizeBuffer[0] = EDFBuffer[20];
  sizeBuffer[1] = EDFBuffer[21];
  dataSize = (EDFBuffer[20]<<8) + EDFBuffer[21];

  if (dataSize > maxDataSize){
    DEBUG_PRINTLN("ERROR: MORE THAN MAXIMUM DATA SIZE WAS RECEIVED !");
    return 0;
  }
  
  // Get Data Frame -> bytes 22 to dataSize+22 (max 1522) (min 68) -> 46 - 1500 bytes
  for(int i = 22; i < dataSize+23; i++)data[i-22] = EDFBuffer[i];

  // Calculate CRC32
  uint32_t crc = crc32(data, dataSize, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true, false);
  uint8_t calc_crc[4] = {0x00, 0x00, 0x00, 0x00};
  calc_crc[0] = (crc & 0x000000ff);
  calc_crc[1] = (crc & 0x0000ff00) >> 8;
  calc_crc[2] = (crc & 0x00ff0000) >> 16;
  calc_crc[3] = (crc & 0xff000000) >> 24;

  // Get CRC
  uint8_t in_crc[4] = {0x00, 0x00, 0x00, 0x00};
  in_crc[0] = EDFBuffer[dataSize+26];
  in_crc[1] = EDFBuffer[dataSize+25];
  in_crc[2] = EDFBuffer[dataSize+24];
  in_crc[3] = EDFBuffer[dataSize+23];
  
  // Check CRC
  data_integrity = 0;
  
  if(in_crc[0] == calc_crc[0] && in_crc[1] == calc_crc[1] && in_crc[2] == calc_crc[2] && in_crc[3] == calc_crc[3]){
    data_integrity = 1;
  }

  // Calculate total frame size
  int EDFSize = dataSize+26;
  
  DEBUG_PRINTLN("----------------------------------------------------------------------------");

  DEBUG_PRINT("Received Ethernet frame: \t");
  DEBUG_PRINTARRAYHEXB(EDFBuffer, EDFSize);
  DEBUG_PRINTLN("");

  DEBUG_PRINT("Destination MAC: \t\t");
  DEBUG_PRINTARRAYHEXB(destinationMAC, 6);
  DEBUG_PRINTLN("");
  
  DEBUG_PRINT("Source MAC: \t\t\t");
  DEBUG_PRINTARRAYHEXB(sourceMAC, 6);
  DEBUG_PRINTLN("");
  
  DEBUG_PRINT("Data Size: \t\t\t");
  DEBUG_PRINTLN(dataSize);
  
  DEBUG_PRINT("Data: \t\t\t\t");
  DEBUG_PRINTARRAYHEXB(data, dataSize);
  DEBUG_PRINTLN("");

  DEBUG_PRINT("Calculated CRC:\t\t\t");
  DEBUG_PRINTARRAYHEXB(calc_crc, 4);
  DEBUG_PRINTLN("");

  DEBUG_PRINT("Received CRC:\t\t\t");
  DEBUG_PRINTARRAYHEXB(in_crc, 4);
  DEBUG_PRINTLN("");
  
  DEBUG_PRINT("Data Integrity: \t\t");
  DEBUG_PRINTLN(data_integrity);

  DEBUG_PRINT("Decoded Data: \t\t\t");
  DEBUG_PRINTARRAYASCII(data, dataSize);
  DEBUG_PRINTLN("");
  
  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("");
}
