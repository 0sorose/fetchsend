#include <SPI.h>
#include <SD.h>
#include "Arduino.h"
#include "RF24.h"

//pin definitions
  #define MOSI 11
  #define MISO 12
  #define IRQ 2
  #define CLK 13
  #define CSsd 4
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

File remote;
File prvate;

RF24 radio(CE,CSrf);

    //global variables
unsigned int fileinfo;
int curr_frame = 0;
const uint64_t pipe[2] = { 0x0AA0F00F000F, 0x0AA0F00F000C };
bool rec = true;  //         ^^remote send   ^^comms send
bool trig = false;
bool ack[frameno];


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

  Serial.begin(19200);  // 9600 memes per meme
  
  // SD setup
  
  if (!SD.begin(CSsd)) {
    Serial.println("SD failed!");
    return;
  }
  Serial.println("SD connected!");

  if (!SD.exists("data.txt")) {
    Serial.println("files missing.");
  }
  remote = SD.open("data.txt", FILE_READ);
  fileinfo = remote.available();
  Serial.print(fileinfo);

  //radio setup

  attachInterrupt(0, radioIRQ, LOW); 

  radio.begin();
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_MAX);
  if(radio.setDataRate(RF24_1MBPS));
  radio.setChannel(chan5);
  radio.disableCRC();
  radio.openWritingPipe(pipe[0]);
  radio.openReadingPipe(1,pipe[1]);
  radio.powerUp();
  radio.startListening();
}



int crcgen(byte crcdata[8], byte key)  // key must be 4 bits or less
{
  unsigned long crc[2] = {0,0};
  byte chksm[2] = {0,0};
  unsigned long process=0;
  byte i;
  byte j;
  for (i = 0; i < 4; i++)
  {
    crc[0] |= crcdata[i];
    crc[0] = crc[0] << 8;
    crc[1] |= crcdata[(i+4)];
    crc[1] = crc[1] << 8;
  }
  for (i = 0; i < 2; i++)
  {
    process = key;
    process = process << 23;
    while(process < (1<<31)) process = process << 1;
    for (j = 0; j < 32; j++)
    {
      
      if (crc[i] < (1<<31)) crc[i] = crc[i] << 1;
      else
      {
        crc[i] = crc[i] ^ process;
        crc[i] = crc[i] << 1;
      }
      
    }

    crc[i] = crc[i] >> 27;
    chksm[i] = crc[i];
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
    unsigned int partno = 0;
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
    }
  }
}


/*void sendpacket(packet postin[8], byte framein)
{
   
} */

/*struct packet
{
  byte adress;
  unsigned int partno;
  unsigned int fileinfo;
  byte data[8];
  byte checksum;
  byte key;
};*/ 

bool streamfile(File stream, int filesize)
{
  while(filesize - stream.available() > 0)
  {
    for(int k = 0; k < 8; k++)
    {
      if (ack[k]) 
      {
        post[k] = {0, 0, 0, 0, 0, random(16)};
        post[k].fileinfo = filesize;
        post[k].partno = (filesize - stream.available());
        for(int l = 0; l < 8; l++)
        {
          post[k].data[l]= stream.read();
          Serial.print("\nread byte");
        }
        post[k].checksum = crcgen(post[k].data, post[k].key);
        Serial.print(post[k].checksum);

      }
      else
      {
        radio.stopListening();

        if(radio.write(&post[k], sizeof(post[k]))) Serial.print("sending packet");
        radio.startListening();
      }  
    }
  }
  return true;
}

void loop() 
{ 
 // unsigned int filesize = remote.available();
 // while(!radio.available() );
 if(streamfile(remote, fileinfo)) Serial.print("\nComplete");
 delay(5000);
}


