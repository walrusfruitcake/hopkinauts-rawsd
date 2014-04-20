#include "RawSD.h"
#include "MemoryFree.h"
#include <SPI.h>

//some addresses
#define ADDR_OSNOTFOUND     0x0000200C
#define ADDR_FREESPACE      0x000191F7 //should only be @ ~50MB pos

//byte dataBlock[BLOCK_LEN];

void setup() {
  Serial.begin(115200);
  Serial.println(freeMemory());
  RawSD sdComm;
  
  //time init
  
  sdComm.initSD();
  
  for (int i=0; i<BLOCK_LEN; i++) {
    sdComm.dataBlock[i] = (byte)'a';
  }
  
  //write test block to memory
  //time write
  
  Serial.println(sdComm.writeBlock(ADDR_FREESPACE));
  
  
  for (int i=0; i<BLOCK_LEN; i++) {
    sdComm.dataBlock[i] = 0x00;
  }
  
  //delay(10000);

  //for verification, read block into memory
  /*Serial.println(sdComm.readBlock(ADDR_FREESPACE));
    for (int i=0; i<BLOCK_LEN; i++) {
      Serial.print(((char)sdComm.dataBlock[i]));
  }*/
  
  //if (!digitalRead(CD)) { //if no card, don't run code
  while (!digitalRead(CD)) {
    //Serial.print(digitalRead(CD));
  }
  
  Serial.println("init okay");
  Serial.println(freeMemory());
}

void loop() {
  Serial.println("\nidle\n");
  delay(5000);
}
