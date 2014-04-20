#include "Arduino.h"
#include <SPI.h>
#include "RawSD.h"

//default constructor
RawSD::RawSD() {
  //pin config
  pinMode(10, OUTPUT); //reqd for Arduino SPI master implementation
  pinMode(CS, OUTPUT);
  pinMode(DI, OUTPUT);
  //pinMode(7, INPUT);
  pinMode(CD, INPUT_PULLUP);
  pinMode(EXT_TRIG_PIN, OUTPUT);

  digitalWrite(10, HIGH); //for Arduino SPI master implementation
  digitalWrite(CS, HIGH); //deselect SD card

  //SPI config
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV128);  
  SPI.setDataMode(SPI_MODE0);

  digitalRead(CD);
  while (!digitalRead(CD)) {
    //Serial.print(digitalRead(CD));
  }
  delay(10);
}

//************
//***public***
//************

//function for initializing the SD card, titled Arduino API style
boolean RawSD::initSD() {

//move init stuff here
//pin config
  pinMode(10, OUTPUT); //reqd for Arduino SPI master implementation
  pinMode(CS, OUTPUT);
  pinMode(DI, OUTPUT);
  //pinMode(7, INPUT);
  pinMode(CD, INPUT_PULLUP);
  pinMode(EXT_TRIG_PIN, OUTPUT);

  digitalWrite(10, HIGH); //for Arduino SPI master implementation
  digitalWrite(CS, HIGH); //deselect SD card

  //SPI config
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV128);  
  SPI.setDataMode(SPI_MODE0);

  digitalRead(CD);
  while (!digitalRead(CD)) {
    //Serial.print(digitalRead(CD));
  }
  delay(10);



  digitalWrite(CS, HIGH); //deselect SD card
  digitalWrite(DI, HIGH);
  digitalWrite(10, HIGH);
//  digitalWrite(EXT_TRIG_PIN, LOW);

  //SPI.begin();
  //digitalWrite(DI, HIGH); //ensure DI still high

  delay(10);

  //"dummmy" clock cycles
  clockCycle(80);
  /*for (int i=0; i<14; i++) {
    SPI.transfer(0xFF);
  }*/

  //EXT_TRIG; //see #defines above
  SPI.begin();
  digitalWrite(CS, LOW);
  //digitalWrite(DI, LOW);
  //delay(10);

  //currCmd = {0, ARG_CMD0, CRC_CMD0};
  sendCmd(0, ARG_CMD0, CRC_CMD0);

  responseByte = getR1();
//  Serial.print(responseByte, HEX);
  if (responseByte != 0x01) {
//    Serial.println("SD init error");
    SPI.end();
//TODO handle errors better?
    return false;
  }

  //EXT_TRIG;
  sendCmd(8, ARG_CMD8, CRC_CMD8);

  respTwoBytes = getR7();
//  Serial.println(respTwoBytes, HEX);

  //unsigned int cmd8[] = {ARG_CMD8};
  //unsigned int sentBytes;

  //compare both bytes to args
  //0x0FFF mask because currently ignoring cmd ver. (first 4 bits)
  if ((respTwoBytes & 0x0FFF) != 
    ( (((unsigned int)currCmd[3]) << 8) | (currCmd[4])) ) {
//TODO handle this error scenario without prints...
//    Serial.print("volt.info error");
//    Serial.println((((unsigned int)currCmd[3]) << 8) | (currCmd[4]));
    //Serial.print(currCmd[3],HEX);
    //Serial.println(currCmd[4],HEX);
  }


  // Actual card init process, monitored by repeatedly sending ACMD41 
  do {
    //sending ACMD_41, so need to send CMD_55 first
    //EXT_TRIG;
    sendCmd(55, ARG_CMD55, CRC_CMD55);    
    responseByte = getR1();
    //Serial.println(responseByte,BIN);
  //do {
    EXT_TRIG;
    sendCmd(41, ARG_ACMD41, CRC_ACMD41);
    responseByte = getR1();
//    Serial.println(responseByte,BIN);

  } while (responseByte != 0x00); //until idle flag cleared

  //read OCR register to check power- and card-capacity-status bits
  sendCmd(58, ARG_CMD58, CRC_CMD58);  
  respTwoBytes = getR3();
//  Serial.println(respTwoBytes, BIN);

  //if power, CCS, and 3.2-3.4V bits not set, exit
  if ((respTwoBytes & 0xC300)>>8 != 0xC3) {
//TODO actually handle...
    Serial.println("power/CCS init error");
  }

  /***initialization complete***/
  
  return true;
}


/*void diagRead() {
  sendCmd(
}*/

//returns 0 on success
//side effect: stores in dataBlock array
unsigned int RawSD::readBlock(unsigned long blockAddr, unsigned int maxCycles) {
  unsigned int cnt;

  sendCmd(17, (byte)(blockAddr>>24), (byte)(blockAddr>>16),
    (byte)(blockAddr>>8), (byte)(blockAddr), CRC_CMD17_NOCRC);
  
  responseByte = getR1();
  //actually ignore idle-bit this time
  if ((responseByte & 0xFE) | 0x00) {
  
    Serial.println(responseByte, BIN);
    return 0x01;
  }

  cnt=0;
  do {
    //watchdog
    if (cnt >= maxCycles) {
     return 0xFF;
    }

   responseByte = SPI.transfer(0x00);
   //Serial.println(responseByte, BIN);

   //if an error token sent (MISO line normally high, so no false+)
   if ((~responseByte & 0xE0) == 0xE0) {
     //Serial.println(responseByte, BIN);
     return 0x02;
   } else if (responseByte==0xFE) { //data token for cmd 17 recvd
     break;
   }

   cnt++;
  } while(true);

  //if valid data token received, continuing receipt of data packet
  for (cnt=0; cnt<BLOCK_LEN; cnt++) {
    dataBlock[cnt] = SPI.transfer(0x00);
  }

  //read-in and currently ignore two CRC bytes
  //Serial.println(SPI.transfer(0x00));
  //Serial.println(SPI.transfer(0x00));

  return 0;
}

unsigned int RawSD::readBlock(unsigned long blockAddr) {
  return readBlock(blockAddr, MAX_WAIT_BYTES);
}

//writes 512-byte dataBlock array to card
unsigned int RawSD::writeBlock(unsigned long blockAddr) {

  sendCmd(24, (byte)(blockAddr>>24), (byte)(blockAddr>>16),
    (byte)(blockAddr>>8), (byte)(blockAddr), CRC_CMD24_NOCRC);

//TODO check contents of this R1 response
  getR1();
//  Serial.println(getR1(),BIN);
  //delay(1);

  //send start-block token
  SPI.transfer(START_TOKEN_SINGLE);

  //send data block
  for (int i=0; i<BLOCK_LEN; i++) {
    SPI.transfer(dataBlock[i]);
  }

  //two CRC bytes
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  
  //this loop for the data response token
  do {
    responseByte = SPI.transfer(0x00);
  } while (responseByte==0x00);
  
  Serial.println(responseByte, BIN);
  
  //now wait until no longer busy
  do {
    responseByte = SPI.transfer(0x00);
  } while (responseByte!=0xFF);

  Serial.println("No longer busy");

  Serial.println(responseByte, BIN);

//TODO wrap do-while around this verification code?
  //if an error token sent (MISO line normally high, so no false+)
   /*if ((~responseByte & 0xE0) == 0xE0) {
     //Serial.println(responseByte, BIN);
     return 0x01;
   } else if (responseByte==0xFE) { //data token for cmd 17 recvd
     break;
   }*/

  return 0;
}


//*************
//***private***
//*************

//sends a #define'd command
void RawSD::sendCmd(byte cmd, byte argByte0, byte argByte1,
  byte argByte2, byte argByte3, byte crc) {

  //store most-recently-sent command, args, and crc
  currCmd[0] = cmd;
  /*currCmd[1] = (byte) (argPart1 >> 8);
  currCmd[2] = (byte) (argPart1 & 0x00FF);
  currCmd[3] = (byte) (argPart2 >> 8);
  currCmd[4] = (byte) (argPart2 & 0x00FF);
  */
  currCmd[1] = argByte0;
  currCmd[2] = argByte1;
  currCmd[3] = argByte2;
  currCmd[4] = argByte3;
  currCmd[5] = crc;

  // -OR the start and xmit bits w/ command
  SPI.transfer(B01000000 | cmd);

  //can only transfer 1 byte atatime
  /*SPI.transfer((byte) (argPart1 >> 8));
  SPI.transfer((byte) (argPart1 & 0x00FF));
  SPI.transfer((byte) (argPart2 >> 8));
  SPI.transfer((byte) (argPart2 & 0x00FF));
  */

  for(byte i=1; i<5; i++) {
    SPI.transfer(currCmd[i]);
  }
  // shift left, -OR with end bit
  SPI.transfer( (crc << 1) | 0x01);

  return;
}


//if called without passing crc
void RawSD::sendCmd(byte cmd, byte argByte0, byte argByte1,
  byte argByte2, byte argByte3) {

  //not yet implemented

  return sendCmd(cmd, argByte0, argByte1, argByte2, argByte3, 0x00); //will fail
}

//response R1
//number-of-cycles watchdog implemented: returns 0xFF if timed-out
byte RawSD::getR1(unsigned int maxCycles) {
  byte rByte;
  unsigned int cnt;

  //while recvd byte doesn't *start* with 0b0, as per response spec
  cnt=0;
  do {
    //watchdog
    if (cnt >= maxCycles) {
      return 0xFF;
    }

    //read byte in
    rByte = SPI.transfer(0x00);
    //Serial.println(responseByte, BIN);
    cnt++;
  } while (rByte & B10000000);
  //delay(1000);
  return rByte;
}

//default to #define'd value
byte RawSD::getR1() {
  return getR1(MAX_WAIT_BYTES);
}

//currently prints OCR in order received
//only 14 non-reserved bits in OCR. The two-bytes returned are
//[power up status][CCS][UHS-II][1.8V][3.5-6V]..[2.7-8V][LV bit7]00
unsigned int RawSD::getR3() {
  unsigned int rTwoBytes;
  responseByte = getR1();

  //verify/retry. note that handleError() includes calls to getR1()
  if (handleError(responseByte) >> 1) { //ignoring corrected errors
    return 0xFF; //not recovered
  }

  //storing bits 31-29 and 24, of first byte received
  rTwoBytes = ((unsigned int)(SPI.transfer(0x00) & 0xE1)) << 8;
  rTwoBytes |= rTwoBytes << 4; //bit 24 now third from MSB

  //next byte, care about all bits (supported-voltage ranges)
  rTwoBytes |= ((unsigned int)SPI.transfer(0x00)) << 4;
  //OCR bit15 is last of the voltage range bits (2.7)
  rTwoBytes |= (SPI.transfer(0x00) & 0x80) >> 4;

  //bit 7 is some reserved bit for low voltage range
  rTwoBytes |= (SPI.transfer(0x00) & 0x80) >> 5;

  return rTwoBytes;
}

//returns 0xFF if R1 response-component of R7 has an error flag
//unless: returns 0x01 if R1-reject with illegal-command error
//^(NOT YET IMPLEMENTED)
//else actual R7 response (4bit cmd.ver, 4bit volt.info, 8bit check)
unsigned int RawSD::getR7(unsigned int maxCycles) {
  unsigned int rTwoBytes;
  unsigned int cnt;

  //as in the spec sheet, R7 includes R1 as part of response. 
  responseByte = getR1();
  //Serial.println(getR1(), BIN);

  //verify/retry. note that handleError() includes calls to getR1()
  if (handleError(responseByte) >> 1) { //ignoring corrected errors
    return 0xFF; //not recovered
  }

  /*verify, ignoring erase-reset flag (not actual error w.r.t.CMD8)
  if ((responseByte & ~0x02) != 0x01) {
    return 0xFF;
  }*/

  //read and shift, only caring about first 4 bits so far (cmd ver.)
  rTwoBytes = ((unsigned int) SPI.transfer(0x00)) << 4;
  //rTwoBytes looks like: 0000 xxxx 0000 0000

  //receive throway byte
  SPI.transfer(0x00);

  //append next four bits (voltage part of response)
  rTwoBytes = rTwoBytes |
    ((unsigned int)(SPI.transfer(0x00) & 0x0F) << 4);
  //now 0000 xxxx yyyy 0000

  //"left-align" shift
  rTwoBytes = rTwoBytes << 4;


  //receive last 8 bits (check pattern)
  rTwoBytes = rTwoBytes | ((unsigned int)SPI.transfer(0x00));

  return rTwoBytes;
}

unsigned int RawSD::getR7() {
  return getR7(MAX_WAIT_BYTES);
}


//handle errors by delays/command resends.
//pass R1 response byte
//returns 0x00 if there were no errors, 0x01 if errors resolved
//else return value = remaining error flags
byte RawSD::handleError(byte byteToCheck) {
  //will be attempting the fewest number of resends possible
  unsigned int fewestResends;
  byte stickyBits = 0x00;

  //if an erase-reset or erase-sequence error, set sticky bits
  stickyBits |= byteToCheck & (R1_ERASE_RST | R1_ERASE_SEQ_ERR);

  /*if an erase-sequence error, inform caller
  if (byteToCheck & R1_ERASE_SEQ_ERR) {
    return
  }*/

  //if no actual errors except erase
  if ((responseByte & stickyBits & ~R1_IDLE) == 0x00) {
    return stickyBits;
  }

  //retry
  for (int i=0; i<fewestResends; i++) {
    sendCmd(currCmd[0],
      currCmd[1], currCmd[2], currCmd[3], currCmd[4], currCmd[5]);
    responseByte = getR1();

    //if an erase-reset or erase-sequence error, set sticky bits
    stickyBits |= responseByte & (R1_ERASE_RST | R1_ERASE_SEQ_ERR);

    //ignore sticky bits for erase reset/erase seq err, as set above
    responseByte &= ~stickyBits;

    //ignoring idle flag
    if ((responseByte & ~R1_IDLE) == 0x00) {
      return 0x01; //error corrected
    }
  }

  //if errors not corrected, return latest response byte w stickies
  return (responseByte | stickyBits);

  return 0x01;
}

//method being used to override SPI-library's DI-logic-level
//This is necessary for the dummy clock cycles...
//assuming 125 kHz, as to be set up with 16MHz/128
unsigned int RawSD::clockCycle(int count) {
  digitalWrite(SCLK, LOW);
  //time0 = micros();

  for (int i=0; i<count; i++) {
      volatile boolean a = true;
      //delayMicroseconds(1);
      digitalWrite(SCLK, HIGH);

      a = false;
      //delayMicroseconds(1);
      digitalWrite(SCLK, LOW);
  }

  //return (micros() - time0);
  return 0;
}


/*unsigned long microsDelay() {
  SPI.begin();
    
  //boolean initState = digitalRead(7);
  time0 = micros();
  SPI.transfer(0x0A);
  SPI.transfer(0x0A);
  
  //while (digitalRead(7)==initState) { }
  time1 = micros();
  
  SPI.end();
  return (time1 - time0);
}*/

