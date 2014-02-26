/** Library to read/write an SD card "raw," i.e., to use it as simple flash card
    It uses the SPI bus to communicate
*/

#ifndef RawSD_h
#define RAWSD_h

#include "Arduino.h"
#include <SPI.h>

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
//#define ADDR_OSNOTFOUND     0x0000200C
//#define ADDR_FREESPACE      0x000191F9

class RawSD {
  public:
    byte dataBlock[BLOCK_LEN];
    
    //default constructor
    RawSD();

    //function for initializing the SD card, titled Arduino API style
    boolean initSD();
    
    //returns 0 on success
    //side effect: stores in dataBlock array
    unsigned int readBlock(unsigned long blockAddr, unsigned int maxCycles);
    
    unsigned int readBlock(unsigned long blockAddr);
    
    //writes 512-byte dataBlock array to card
    unsigned int writeBlock(unsigned long blockAddr);
    
  private:    
    //unsigned long time0, time1;
    //unsigned int i; //not sure about garbage collection on loop-end
    
    //most-recently-sent cmd, arg, crc. used by sendCmd() handleError()
    byte currCmd[6];
    
    //byte response[17];
    byte responseByte;
    unsigned int respTwoBytes;
    //byte ocrBytes[4]; //register
    
    //sends a #define'd command
    void sendCmd(byte cmd, byte argByte0, byte argByte1,
      byte argByte2, byte argByte3, byte crc);
    
    //if called without passing crc
    void sendCmd(byte cmd, byte argByte0, byte argByte1,
      byte argByte2, byte argByte3);
    
    //response R1
    //number-of-cycles watchdog implemented: returns 0xFF if timed-out
    byte getR1(unsigned int maxCycles);
    byte getR1(); //default to #define'd value
    
    //currently prints OCR in order received
    //only 14 non-reserved bits in OCR. The two-bytes returned are
    //[power up status][CCS][UHS-II][1.8V][3.5-6V]..[2.7-8V][LV bit7]00
    unsigned int getR3();
    
    //returns 0xFF if R1 response-component of R7 has an error flag
    //unless: returns 0x01 if R1-reject with illegal-command error
    //^(NOT YET IMPLEMENTED)
    //else actual R7 response (4bit cmd.ver, 4bit volt.info, 8bit check)
    unsigned int getR7(unsigned int maxCycles);
    unsigned int getR7();
        
    //handle errors by delays/command resends.
    //pass R1 response byte
    //returns 0x00 if there were no errors, 0x01 if errors resolved
    //else return value = remaining error flags
    byte handleError(byte byteToCheck);
    
    //method being used to override SPI-library's DI-logic-level
    //This is necessary for the dummy clock cycles...
    //assuming 125 kHz, as to be set up with 16MHz/128
    unsigned int clockCycle(int count);
    
    //unsigned long microsDelay();
    
};

#endif
