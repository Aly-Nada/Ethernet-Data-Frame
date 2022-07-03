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

SoftwareSerial mySerial(10, 11); // RX, TX

// For Sending -----------------------------------------------------------------
uint8_t EDFBuffer[maxDataSize+30];
int EDFSize = 0;

uint8_t my_MAC[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
uint8_t other_device_MAC[6] = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB};

uint8_t user_input[maxDataSize];
int user_input_i = 0;
bool user_inputComplete = false;

// For Receiving ---------------------------------------------------------------
uint8_t destinationMAC[6];
uint8_t sourceMAC[6];
uint8_t data[maxDataSize];
uint8_t sizeBuffer[2];
int dataSize;
int data_integrity;

uint8_t incoming[maxDataSize];
int incoming_i = 0;
bool incomingComplete = false;


// Setup -----------------------------------------------------------------------
void setup() {
  Serial.begin(57600);
  mySerial.begin(4800);
}


// Loop ------------------------------------------------------------------------
void loop() {

  // Wait for user input  ------------------------------------------------------
  if (Serial.available()) {
    uint8_t inChar = (uint8_t)Serial.read();
    if (inChar == '\n') {
      user_inputComplete = true;
    }
    else{
      user_input[user_input_i] += inChar;
      user_input_i++;
    }
  }

  // Send user input when complete ---------------------------------------------
  if (user_inputComplete) {
    encodeEthernetDataFrame(other_device_MAC, my_MAC, user_input, user_input_i);

    for(int i = 0; i<EDFSize; i++) mySerial.write(EDFBuffer[i]);
    mySerial.write('\n');

    user_inputComplete = false;
    for(int i = 0; i < user_input_i+1; i++)user_input[i] = 0;
    user_input_i = 0;
  }

  // Wait for message from other device ----------------------------------------
  if (mySerial.available()) {
    uint8_t inChar = (uint8_t)mySerial.read();
    if (inChar == '\n') {
      incomingComplete = true;
    }
    else{
      incoming[incoming_i] += inChar;
      incoming_i++;
    }
  }

  // Print received message when complete --------------------------------------
  if (incomingComplete) {
    decodeEthernetDataFrame(incoming);

	Serial.print ("Received from [");
	for(int i = 0; i<6; i++) {
	  Serial.print ("0x");
	  Serial.print (sourceMAC[i], HEX);
	  Serial.print (" ");
    }
	Serial.print ("] : ");
	for(int i = 0; i<dataSize; i++) Serial.write(data[i]);
	Serial.println();

    incomingComplete = false;
    for(int i = 0; i < incoming_i+1; i++)incoming[i] = 0;
    incoming_i = 0;
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

// For Encoding ----------------------------------------------------------------

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


// For Decoding ----------------------------------------------------------------

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
    //return 0;
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

  DEBUG_PRINTLN("---------------------------------------------------------------");

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
