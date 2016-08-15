#include <SPI.h>
//#include <SD.h>
#include "Arduino.h"
#include "RF24.h"

//pin definitions
  #define MOSI 11
  #define MISO 12
  #define IRQ 2
  #define CLK 13
  #define CSsd 8
  #define CSrf 7
  #define CE 6

//set integer definitions
  #define chan1 20
  #define chan2 40
  #define chan3 80
  #define chan4 100
  #define chan5 110
  #define chan6 120
  #define chan7 60
  #define frameno 8
  #define remoteint 0xFFFE

//File remote;
//File prvate;

RF24 radio(CE,CSrf);

    //global variables
unsigned int fileinfo;
int curr_frame = 0;
const uint64_t pipe[2] = { 0x0AA0F00F000F, 0x0AA0F00F000C };
bool rec = true;  //         ^^remote send   ^^comms send
bool trig = false;
bool ack[frameno];
unsigned int remoteaccess = remoteint;


struct packet
{
  byte adress;
  unsigned int partno;
  unsigned int fileinfo;
  byte data[8];
  byte checksum;
  byte key;
};

packet post[8];

void setup()
{
  pinMode(CSsd, OUTPUT);
  pinMode(CSrf, OUTPUT);

  Serial.begin(19200);  // 19200 memes per meme
  
  // SD setup
 /* 
  if (!SD.begin(4)) {
    Serial.println("SD failed!");
    return;
  }
  Serial.println("SD connected!");

  if (!SD.exists("remote_data.txt")) {
    Serial.println("files missing.");
  }
  remote = SD.open("remote_data.txt", FILE_READ);
  fileinfo = remote.available();
*/
  //radio setup

  attachInterrupt(0, radioIRQ, LOW); 

  radio.begin();
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_MAX);
  if(radio.setDataRate(RF24_1MBPS));
  radio.setChannel(chan5);
  radio.disableCRC();
  radio.openWritingPipe(pipe[1]);
  radio.openReadingPipe(1,pipe[0]);
  radio.powerUp();
  radio.startListening();
}


bool crcconf(byte crcdata[8], byte key, byte crcin)  // key must be 4 bits or less 
{
  unsigned long crc[2] = {0,0};
  byte crcstr[2] = {0xF0,0x0F};
  unsigned long process=0;
  byte i;
  byte j;
  byte bitshift;
  for (i = 0; i < 4; i++)                                       
  {
    crc[0] |= crcdata[i];                                
    crc[0] = crc[0] << 8;                               
    crc[1] |= crcdata[(i+4)];                                 
    crc[1] = crc[1] << 8;
  }
  for (i = 0; i < 2; i++)
  {
    crcstr[i] = crcstr[i] & crcin;
    if(i > 0) crcstr[i] = crcstr[i] << 4;
    process = key;
    process = process << 23;
    while(process < (1<<31)) process = process << 1;
    for (j = 0; j < 32; j++)
    {
      
      if (crc[i] < (1<<31)) 
      {
        crc[i] = crc[i] << 1;
        //shift in bit from crcstr
        bitshift = (crcstr[i] & (1<<7));
        crcstr[i] = crcstr[i] << 1;
        bitshift = bitshift >> 7;
        crc[i] = crc[i] | bitshift;
      }
      else
      {
        crc[i] = crc[i] ^ process;
        crc[i] = crc[i] << 1;
      }
      
    }
    
  }
  
  //return (chksm[0]<<4)|(constrain(chksm[1], 0 , 15));
}

void radioIRQ()
{
  bool tx, fail, rx;
  radio.whatHappened(tx,fail,rx);

  if(fail) Serial.print("Send failed\n");

  if(rx || radio.available())
  {

    packet rcv;
    radio.read(&rcv, sizeof(rcv));
    if(crcconf(rcv.data, rcv.key, rcv.checksum))
    {
      radio.stopListening();
      radio.write(&rcv.partno, sizeof(rcv.partno));
      radio.startListening();
      for(int u = 0; u < 8; u++)
      {
        Serial.print(rcv.data[u]);
      }
    }
    
    /*unsigned int partno = 0;
    radio.read(&partno, sizeof(partno));
    for (int i = 0; i < 8; i++)
    {
      if(partno == post[i].partno)
      {
        ack[i] = true;
        break;
      }
    }
    if (partno == remoteint)
    {
      if(!streamfile(remote, fileinfo));
    }*/
  }
}

/*struct packet                  
{
  byte adress;
  unsigned int partno;
  unsigned int fileinfo;
  byte data[8];
  byte checksum;
  byte key;
};*/

void loop() 
{
  
  if(millis() > 3000 && millis() <3010)
  {
    Serial.write("...\n");
    radio.stopListening();
    Serial.write("...\n");
    if(radio.write(&remoteaccess, sizeof(remoteaccess))) 
    {
      Serial.print("Access request sent.");
    }
    else Serial.print("radio failure");
    
    radio.startListening();
    while(millis() < 3010);
  }
 /* packet post[8];
  unsigned int filesize = remote.available();
  for(int k = 0; k < 8; k++){
    post[k] = {0, 0, 0, 0, 0, random(16)};
    post[k].fileinfo = filesize;
    post[k].partno = (filesize - remote.available());
    for(int l = 0; l < 8; l++)
    {
      post[k].data[l]= remote.read();
    }
    post[k].checksum = crcgen(post[k].data, post[k].key);
   
  }*/
  //radio.send()
}


