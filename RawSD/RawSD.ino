#include<SPI.h>

//#define DRDY 6 //data ready pin
#define SCLK  13
#define DO    12 //MISO or DO pin
#define DI    11 //MOSI or DI pin
#define CS    8 //device select pin
#define CD    7 //card-detect pin

//external trigger for oscilloscope debugging
//Note: only leave one uncommented to avoid o-scope confusion 
#define EXT_TRIG_PIN         2
#define EXT_TRIG             digitalWrite(EXT_TRIG_PIN, HIGH)

#define BLOCK_LEN              512 //number of bytes

#define MAX_WAIT_BYTES         32 //8bit cycles to wait for response

#define MAX_CMD_RESEND         8 //times to retry CMD-send on error
#define MAX_ILL_CMD            MAX_CMD_RESEND //illegal cmd error
#define MAX_CRC_ERR            MAX_CMD_RESEND //command crc error
#define MAX_ERASE_ERR          MAX_CMD_RESEND //erase-sequence error
#define MAX_ADDR_ERR           MAX_CMD_RESEND
#define MAX_PARAM_ERR          MAX_CMD_RESEND
//#define MAX_CMD_RESEND

/*Command argument definitions, formatted to be passed to sendCmd()
#define ARG_TEST           0xAB, 0xCD
#define ARG_CMD0           0x0000, 0x0000
//avoid an 'FF' check pattern if redefining CMD8 ARG (see getR7())
#define ARG_CMD8_33V_AA    0x0000, 0x01AA
#define ARG_CMD8           ARG_CMD8_33V_AA //as a default
#define ARG_CMD55          0x0000, 0x0000
//note: if ever using MMC, ACMD41 will likely be rejected; use CMD1
#define ARG_ACMD41_HCS     0x4000, 0x0000 //HCS (SDHC/SDXC) bit set
#define ARG_ACMD41_NOHCS   0x0000, 0x0000
#define ARG_ACMD41         ARG_ACMD41_HCS //default yes
*/

//different rewrite happened: now bytes not u_ints
//new ARG defs for sendCmd() rewrite, RES* masks for reserved bits
//#define RES_CMD0           0x

//Command argument definitions, formatted to be passed to sendCmd()
#define ARG_CMD0            0x00, 0x00, 0x00, 0x00
//avoid an 'FF' check pattern if redefining CMD8 ARG (see getR7())
#define ARG_CMD8_33V_AA     0x00, 0x00, 0x01, 0xAA
#define ARG_CMD8            ARG_CMD8_33V_AA //as a default
//removed since an address.
//#define ARG_CMD17_READTEST  0x00, 0x00, 0x20, 0x0C
#define ARG_CMD55           0x00, 0x00, 0x00, 0x00
#define ARG_CMD58           0x00, 0x00, 0x00, 0x00
//note: if ever using MMC, ACMD41 will likely be rejected; use CMD1
#define ARG_ACMD41_HCS      0x40, 0x00, 0x00, 0x00 //HCS (SDHC/SDXC) bit set
#define ARG_ACMD41_NOHCS    0x00, 0x00, 0x00, 0x00
#define ARG_ACMD41          ARG_ACMD41_HCS //default yes

//pre-computed 7-bit (not yet shifted) CRC definitions
#define CRC_CMD0            0x4A
#define CRC_CMD8_33V_AA     0x43 //2.7-3.6V and a 0xAA check pattern
#define CRC_CMD8            CRC_CMD8_33V_AA //as a default
//#define CRC_CMD17_READTEST  0x72
#define CRC_CMD17_NOCRC     0x00 //crc bits can be anything if CRC not enforced
#define CRC_CMD24_NOCRC     0x00
#define CRC_CMD55           0x32
#define CRC_CMD58           0x7E
#define CRC_ACMD41_HCS      0x3B
#define CRC_ACMD41_NOHCS    0x72
#define CRC_ACMD41          CRC_ACMD41_HCS //default HCS

//R1 error flag definitions
#define R1_IDLE             0x01 //idle flag
#define R1_ERASE_RST        0x02 //erase reset (not an error?)
#define R1_ILL_CMD          0x04 //illegal command
#define R1_CRC_ERR          0x08 //command crc error
#define R1_ERASE_SEQ_ERR    0x10 //erase-sequence error
#define R1_ADDR_ERR         0x20 //address error
#define R1_PARAM_ERR        0x40 //parameter error

//tokens
#define START_TOKEN_SINGLE  0xFE
#define START_TOKEN_MULT    0xFC

//some addresses
#define ADDR_OSNOTFOUND     0x0000200C
#define ADDR_FREESPACE      0x000191F9

unsigned long time0, time1;
//unsigned int i; //not sure about garbage collection on loop-end

//most-recently-sent cmd, arg, crc. used by sendCmd() handleError()
byte currCmd[6];

//byte response[17];
byte responseByte;
unsigned int respTwoBytes;
//byte ocrBytes[4]; //register

byte dataBlock[BLOCK_LEN];

void setup() {
  Serial.begin(9600);
  
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
  
  Serial.println(digitalRead(CD));
  
  //if (!digitalRead(CD)) { //if no card, don't run code
  while (!digitalRead(CD)) {
    //Serial.print(digitalRead(CD));
  }
  //Serial.print(digitalRead(CD));
  //Serial.print("hi");
  delay(5000);
}

void loop() {
  Serial.print("\n\nhi\n");
  digitalWrite(CS, HIGH); //deselect SD card
  digitalWrite(DI, HIGH);
  digitalWrite(10, HIGH);
  digitalWrite(EXT_TRIG_PIN, LOW);
  
  //SPI.begin();
  //digitalWrite(DI, HIGH); //ensure DI still high
  
  delay(10);
  
  //"dummmy" clock cycles
  clockCycle(80);
  /*for (int i=0; i<14; i++) {
    SPI.transfer(0x00);
  }*/
  
  //EXT_TRIG; //see #defines above
  SPI.begin();
  digitalWrite(CS, LOW);
  //digitalWrite(DI, LOW);
  //delay(10);
  
  //currCmd = {0, ARG_CMD0, CRC_CMD0};
  sendCmd(0, ARG_CMD0, CRC_CMD0);
  
  
  /*initialization sequence telling SD card to go into SPI mode
  SPI.transfer(0x40);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x95); //checksum; no more checksums after this pt?
  */
  
  /*old response code
  int cnt=0;
  do {
    responseByte = SPI.transfer(0x00); //read byte in
    if (cnt >= MAX_WAIT_BYTES) { //correct response to init seq recvd
      Serial.println("init: no response");
      SPI.end();
      //delay(1000);
      return; //terminate loop() early
    }
    cnt++;
  } while (responseByte != 0x01);
  */
  
  responseByte = getR1();
  Serial.print("0x0");
  Serial.print(responseByte, HEX);
  Serial.println(" recvd");
  if (responseByte != 0x01) {
    Serial.println("init error");
    SPI.end();
    return;
  }
  
  //EXT_TRIG;
  sendCmd(8, ARG_CMD8, CRC_CMD8);
  
  respTwoBytes = getR7();
  Serial.println(respTwoBytes, HEX);
  
  //unsigned int cmd8[] = {ARG_CMD8};
  //unsigned int sentBytes;
  //compare both bytes to args
  //0x0FFF mask because currently ignoring cmd ver. (first 4 bits)
  if ((respTwoBytes & 0x0FFF) != 
    ( (((unsigned int)currCmd[3]) << 8) | (currCmd[4])) ) {
    Serial.print("volt.info error");
    Serial.println((((unsigned int)currCmd[3]) << 8) | (currCmd[4]));
    //Serial.print(currCmd[3],HEX);
    //Serial.println(currCmd[4],HEX);
  }
  
  
  //actual card init process monitored by repeatedly sending ACMD41 
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
    Serial.println(responseByte,BIN);
    
  } while (responseByte != 0x00); //until idle flag cleared
  
  //read OCR register to check power- and card-capacity-status bits
  sendCmd(58, ARG_CMD58, CRC_CMD58);  
  respTwoBytes = getR3();
  Serial.println(respTwoBytes, BIN);
  
  //if power, CCS, and 3.2-3.4V bits not set, exit
  if ((respTwoBytes & 0xC300)>>8 != 0xC3) {
    Serial.println("power/CCS init error");
  }
  
  /***initialization complete***/
  
  //test storage of 512 bytes
  /*volatile byte readBlock[BLOCK_LEN];
  for (int i=0; i<BLOCK_LEN; i++) {
    readBlock[i] = 0xAA+i;
  }
  for (int i=0; i<BLOCK_LEN; i++) {
    Serial.print(i);
    Serial.print(".");
    Serial.println(readBlock[i]);
  }*/
  
  //set up test block to be all ASCII 'U's
  for (int i=0; i<BLOCK_LEN; i++) {
    dataBlock[i] = 0x55;
  }
  //write test block to memory
  writeBlock(ADDR_FREESPACE);
  
  delay(10000);

  //for verification
  //read block into memory

  Serial.println(readBlock(ADDR_FREESPACE));
    for (int i=0; i<BLOCK_LEN; i++) {
      Serial.print(((char)dataBlock[i]));
  }
  
  
  
  /*CMD1: was a busy signal (0x01 received?)
  boolean busyRecvd;
  responseByte = SPI.transfer(0x00);
  busyRecvd = (responseByte==0x01);
  //continue with initialization by sending CMD1 until 0x00 response
  cnt=0;
  do {
    responseByte = SPI.transfer(0x00);
    
  } while (responseByte != 0x00);
  
  if ()
  */
  
  
  /* old init grab-several-potential-response-bytes code
  for(int i=0; i<17; i++) {
    response[i] = SPI.transfer(0x00);
  }
  
  for(int i=0; i<17; i++) {
    Serial.println(response[i],BIN);
  }*/
  SPI.end();
  delay(10000);
}

/*
//must first set sentCmd array to desired cmd, args, crc
void sendCmd() {
  // -OR the start and xmit bits w/ command number
  SPI.transfer(B01000000 | currCmd[0]);
  
  for(byte i=1; i<5; i++) {
    SPI.transfer(currCmd[i]);
  }
  
  // leftshift crc, -OR with end bit
  SPI.transfer( (currCmd[5] << 1) | 0x01);
  
}
*/

unsigned int initCard() {  
  //move loop code here
  return 0;
}

//returns 0 on success
//side effect: stores in dataBlock array
unsigned int readBlock(unsigned long blockAddr, unsigned int maxCycles) {
  unsigned int cnt;
  
  sendCmd(17, (byte)(blockAddr>>24), (byte)(blockAddr>>16),
    (byte)(blockAddr>>8), (byte)(blockAddr), CRC_CMD17_NOCRC);
  
  //Serial.println(getR1(),BIN);
  //actually ignore idle-bit this time
  if ((getR1() & 0xFE) | 0x00) {
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
     return 0x01;
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

unsigned int readBlock(unsigned long blockAddr) {
  return readBlock(blockAddr, MAX_WAIT_BYTES);
}

//writes 512-byte dataBlock array to card
unsigned int writeBlock(unsigned long blockAddr) {
  
  sendCmd(24, (byte)(blockAddr>>24), (byte)(blockAddr>>16),
    (byte)(blockAddr>>8), (byte)(blockAddr), CRC_CMD24_NOCRC);
  
  Serial.println(getR1(),BIN);
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
  
  //data response token
  responseByte = SPI.transfer(0x00);
  Serial.println(responseByte, BIN);
  //delay(1);
  
  do {
    responseByte = SPI.transfer(0x00);
  } while (responseByte==0x00);
  
  Serial.println("No longer busy");
  
  //if an error token sent (MISO line normally high, so no false+)
   /*if ((~responseByte & 0xE0) == 0xE0) {
     //Serial.println(responseByte, BIN);
     return 0x01;
   } else if (responseByte==0xFE) { //data token for cmd 17 recvd
     break;
   }*/
  
  return 0;
}

//sends a #define'd command
void sendCmd(byte cmd, byte argByte0, byte argByte1,
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
void sendCmd(byte cmd, byte argByte0, byte argByte1,
  byte argByte2, byte argByte3) {
  
  //not yet implemented
  
  return sendCmd(cmd, argByte0, argByte1, argByte2, argByte3, 0x00); //will fail
}

//response R1
//number-of-cycles watchdog inmplemented: returns 0xFF if timed-out
byte getR1(unsigned int maxCycles) {
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
byte getR1() {
  return getR1(MAX_WAIT_BYTES);
}

//currently prints OCR in order received
//only 14 non-reserved bits in OCR. The two-bytes returned are
//[power up status][CCS][UHS-II][1.8V][3.5-6V]..[2.7-8V][LV bit7]00
unsigned int getR3() {
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

//returns a ptr to a 4byte array of the OCR register returned by R3
//must call getR3lower() immediately returning

//for now, returns 0x0000, but toggles clock to complete R3
//returns the upper two bytes of the OCR register returned by R3
/*unsigned int getR3lower() {
  return 0x0000;
}*/


//returns 0xFF if R1 response-component of R7 has an error flag
//unless: returns 0x01 if R1-reject with illegal-command error
//^(NOT YET IMPLEMENTED)
//else actual R7 response (4bit cmd.ver, 4bit volt.info, 8bit check)
unsigned int getR7(unsigned int maxCycles) {
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

unsigned int getR7() {
  return getR7(MAX_WAIT_BYTES);
}


//handle errors by delays/command resends.
//pass R1 response byte
//returns 0x00 if there were no errors, 0x01 if errors resolved
//else return value = remaining error flags
byte handleError(byte byteToCheck) {
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

//ignore lines two and three: can send 0xFF to keep DI HIGH
//ignore fourth comment line: method necessary to avoid adding an
//override to SPI-library's DI-logic-level (for dummy clock cycles)
//written before realization that the SPI module can "send" 0x00
//assuming 125 kHz, as to be set up with 16MHz/128
unsigned int clockCycle(int count) {
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

