//libraries
  #include <SD.h>
  #include <SPI.h>
  #include "RF24.h"

//pin definitions
  #define MOSI 11
  #define MISO 12
  #define IRQ 2
  #define CLK 13
  #define CSsd 4
  #define CSrf 7
  #define CE 6
  
//other definitions
  #define channel 105
  #define frameno 16
  #define remoteint 0xFFFE
  #define masterkey 0x5D;
  #define pipetocom 0xCC5E  //  remote writes, comms listens
  #define pipetorem 0xCC5D  //  comms writes, remote listens


//global variables
bool role; // 1 is data 0 is comms
  //bool onetime = true;
File remote;
RF24 radio(6,7);
unsigned int fileinfo = 0;
unsigned int framecount;



struct packet
{
  uint32_t address;
  char data[2][4]; //8 bytes in 2 groups of 32 bits (4 bytes) for crc
  uint8_t key;
  uint8_t crc[2];
  uint16_t fileinfo;
  uint16_t part;
};

packet frames[frameno];
bool rxacn[frameno];



void packframes() //read data from the SD and calculate checksums;
{
  //Serial.print("packing frames\n");
  for(byte n = 0; n < frameno; n++)
  {
    //header
    frames[n].address = pipetocom;
    frames[n].key = masterkey;
    frames[n].fileinfo = fileinfo;
    frames[n].part = fileinfo - remote.available();

    //data
    for(byte a = 0; a < 2; a++)
    {
      for(byte b = 0; b < 4; b++)
      {
        if (!rxacn[n] && remote.available())
        {
          frames[n].data[a][b] = remote.read();
          //Serial.print(frames[n].data[a][b]);           //test: prints characters as read from card
        }
        else
        {
          //Serial.print("failed\n");
          frames[n].data[a][b] = 0x03; // ascii end of text marker
          break;
        }
        //Serial.print(frames[n].data[a][b]);             //  test    prints char loaded into packet
      }
    }
    //trailer
    crcgen( frames[n].crc, frames[n].data, frames[n].key);
    //if (!crccheck(frames[n].crc, frames[n].data, frames[n].key)) Serial.println("CRC ERROR!!");
    //else Serial.println("CRC gen success!");
    
    rxacn[n] = false;  // reset acknowlegement
    
    //delay(3000);
  } //FOR EACH FRAME END
  Serial.println("Frames packed!");
}




void crcgen(byte crc[2], char data[2][4], uint8_t key)
{
  uint32_t process;
  uint32_t keymod = key;
  uint32_t highbit = 0x80000000;
  keymod = keymod << 23;
  while(keymod < highbit) keymod = keymod << 1; //shift key to be most significant
  //Serial.print("\nKey = ");
  //Serial.println(keymod, HEX);
  for(byte a=0; a<2; a++)
  {
    process = 0;
    
    for(byte b=0; b<4; b++)  // load 32 bit process space
    {
      process = process << 8;
      process = process | data[a][b];
      //Serial.println(data[a][b], HEX);
    }
    //Serial.print("LOAD: ");
    //Serial.println(process, HEX);
    for(byte c=0; c<32; c++)
      {
        if(process < highbit) // most sugnificant bit is 0, shift up
        {
          process = process << 1;
        }
        else
        {
          process = process ^ keymod;
          process = process << 1;
        }
        //Serial.println(process,HEX);      //test: prints each step
        //delay(200);
      }
      process = (process >> 24);
      //Serial.println(process, HEX);
      crc[a] = (byte)process;
      //*crc[a] = (process >> 23);
      //Serial.print("CRC: ");
      //Serial.println(crc[a], HEX);
  }
}




bool crccheck( uint8_t crc[2], char data[2][4], uint8_t key )
{
  uint32_t process;
  uint32_t highbit = 0x80000000;
  //Serial.print("Highbit: ");
  //Serial.println(highbit);
  byte mod;
  uint32_t keymod = key;
  keymod = keymod << 23;
  while(keymod < highbit) keymod = keymod << 1; //shift key to be most significant
  for (byte a=0; a<2; a++ )
  {
    process = 0;
    for (byte b=0; b<4; b++)
    {
      process = process << 8;
      process = process | data[a][b];
    }

    //Serial.print("KEY: ");
    //Serial.println(keymod, BIN);
    
    //Serial.print("CRC: ");
    //Serial.println(crc[a], BIN);
    
    //Serial.print("PLD: ");
    //Serial.println(process, BIN);
    for (byte b=0; b<32; b++)
    {
      if (process < highbit) 
      {
        process = process << 1;
      }
      else 
      {
        process = process ^ keymod;
        process = process << 1;
      }

      
      mod = crc[a] & 0x80;
      mod = mod >> 7;
      process = process | mod;
      crc[a] = crc[a] << 1;
      //Serial.println(process, BIN);
      //delay(200);
       
    }
  }
  //Serial.println(process, HEX);
  return !(process);
}




void unpack() // verify checksums
{
  
}




void transloop()
{
  bool acnswtch = true;
  while(acnswtch)             // breaks if all frames acnowleged
  {                           //sends frames one by one, with a tiny delay for receiver to process
    
    for (byte d = 0; d < frameno; d++)
    {
      if(rxacn[d])
      {
        radio.stopListening();
        radio.write(&frames[d] , sizeof(packet));
        radio.startListening();
      }
    }
    
    for (byte e = 0; e < frameno; e++)  //I think this is pretty clever
    {
      if(!rxacn[e])       // sets to continue only if a frame is unacnknlowleged
      {
        acnswtch = false;
      }
    }
    acnswtch = !acnswtch;
  }
}




void irq()
{
  bool rx,tx,fail;
  radio.whatHappened(tx, fail, rx);
  Serial.println("\n---- ----  IRQ!  ---- ----\n");
  if(rx)
  {
    Serial.println("RX detect");
    if(role)       //         //        // data unit
    {
      uint16_t party;
      radio.read(&party, sizeof(uint16_t));
      
      for(byte d = 0; d < frameno; d++)
      {
       if(party = frames[d].part)
       {
         rxacn[d] = true;  //set acnknowlegement
         Serial.print("Part number: "); Serial.println(party, HEX);
       }
       else Serial.println("unknown part rx!");
       Serial.print("Frames Sent:   ");
       Serial.println((++framecount));
      }
    }
    else          //        //        // comms unit
    {
      packet laframe;
      radio.read(&laframe, sizeof(packet));
      for(byte x = 0; x < 2; x++)
      {
        for(byte y = 0; y < 4; y++)
        {
          Serial.print(laframe.data[x][y]);
        }
      }
    }
  }
  

   if(tx)
   {
    Serial.println("TX success");
    
   }

   if(fail)
   {
    Serial.println("TX failure");
    
   }

   Serial.println("\n---- ---- IRQ END ---- ----\n");
}




void setup() 
{
//General setup
    attachInterrupt(0, irq, LOW);

    Serial.begin(57600);

    for(byte j = 0; j < frameno; j++)
    {
      rxacn[j] = false;
    }


//SD setup & role assignment

   if (!SD.begin(4)) 
   {
     Serial.println("SD failed not detected, Set up as Comms unit\n");
     Serial.println(F("Enter F to request file"));
     role = 0;
   }
   else
   {
     role = 1;
     Serial.println("SD connected! Set as database\n");
     if (!SD.exists("data.txt")) 
     {
        Serial.println("data file not found\n");
      }
      remote = SD.open("data.txt", FILE_READ);
      fileinfo = remote.available();
      Serial.print(fileinfo);
      Serial.print(" char bytes in file.\n\n");
      if(fileinfo%8 > 0)
      {
        Serial.print("Sending "); Serial.print((fileinfo/8)+1); Serial.println(" frames.");
      }
      else
      {
        Serial.print("Sending "); Serial.print(fileinfo/8); Serial.println(" frames.\n");
      }
    }


//Radio set up
  radio.setPALevel(RF24_PA_HIGH);
  //if(!radio.setDataRate(RF24_1MBPS)) Serial.println("Data Rate Failure!"); 
  radio.setChannel(channel);
  radio.startListening();
  Serial.println("Radio Initalised!\n");
  Serial.print("Transmission Power:   ");
  Serial.println(radio.getPALevel());
  Serial.print("Data Rate:            ");
  Serial.println(radio.getDataRate());
  Serial.print("Transmission Channel: ");
  Serial.println(channel);
    //Serial.println(micros());
//Setup ends here
}




void loop() 
{
  if(remote.available())
  {
    packframes();
    delay(5);
    transloop();
    delay(2);
  }  
}
