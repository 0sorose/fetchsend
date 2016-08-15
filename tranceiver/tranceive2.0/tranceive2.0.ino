//With multiple devices, each device would need to have a separate reading pipe

#include <SPI.h>
#include "RF24.h"
#include <printf.h>
#include <SD.h>

//pin definitions
#define  CSsd 4

//other definitions
#define

// Hardware configuration
// Set up nRF24L01 radio on SPI bus plus pins 7 & 8
RF24 radio(6,7);
                                        
// Use the same address for both devices
uint8_t address[] = { "radio" };

  // global variablea
File remote;
bool role = 0; // default comms role;
char ping = '@';
unsigned int pong = 0xFF01;

volatile uint32_t round_trip_timer = 0;


/********************** Setup *********************/
void setup(){

  Serial.begin(115200);
  Serial.println(F("Enter F to request file"));
  //printf_begin();

  // SD setup
  
  if (!SD.begin(4)) {
    Serial.println("SD failed not detected, Set up as Comms unit\n");
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
    unsigned int fileinfo = remote.available();
    Serial.print(fileinfo);
  }
  
  

  
  // Setup and configure rf radio
  radio.begin();

  // Use dynamic payloads to improve response time
  radio.enableDynamicPayloads();
  radio.openWritingPipe(address);             // communicate back and forth.  One listens on it, the other talks to it.
  radio.openReadingPipe(1,address); 
  radio.startListening();
  
  //radio.printDetails();                             // Dump the configuration of the rf unit for debugging

  attachInterrupt(0, check_radio, LOW);             // Attach interrupt handler to interrupt #0 (using pin 2) on BOTH the sender and receiver
}



void packsend(File source)
{
  while(micros() - round_trip_timer < 45000){
                  //delay between writes 
                }
                Serial.print(F("Sending Request\n"));
                ping = remote.read();
                radio.stopListening();                
                round_trip_timer = micros();
                radio.startWrite( &ping, sizeof(ping),0 );
                break; 
}

/********************** Main Loop *********************/
void loop() {

  if(Serial.available()){
    switch(toupper(Serial.read())){
      case 'F': 
                // Only allow 1 transmission per 45ms to prevent overlapping IRQ/reads/writes
                // Default retries = 5,15 = ~20ms per transmission max
               /* while(micros() - round_trip_timer < 45000){
                  //delay between writes 
                }
                Serial.print(F("Sending Request\n"));
                radio.stopListening();                
                round_trip_timer = micros();
                radio.startWrite( &ping, sizeof(ping),0 );
                break;  */  
                packsend(remote);
    }
  }  
}

/********************** Interrupt *********************/

void check_radio(void)                                // Receiver role: Does nothing!  All the work is in IRQ
{
  
  bool tx,fail,rx;
  radio.whatHappened(tx,fail,rx);                     // What happened?

 
  // If data is available, handle it accordingly
  if ( rx ){
    
    if(radio.getDynamicPayloadSize() < 1){
      // Corrupt payload has been flushed
      return; 
    }
    // Read in the data
    unsigned int received;
    radio.read(&received,sizeof(received));

    // If this is a ping, send back a pong
    //if(received == ping){
      radio.stopListening();
      // Normal delay will not work here, so cycle through some no-operations (16nops @16mhz = 1us delay)
      for(uint32_t i=0; i<130;i++){
         __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
      }
      //radio.startWrite(&pong,sizeof(pong),0);
      packsend(remote);
      Serial.print("pong");
    //}else    
    // If this is a pong, get the current micros()
    if(received == pong){
      round_trip_timer = micros() - round_trip_timer;
      Serial.print(F("Received Pong, Round Trip Time: "));
      Serial.println(round_trip_timer);
    }
  }

  // Start listening if transmission is complete
  if( tx || fail ){
     radio.startListening(); 
     Serial.println(tx ? F(":OK") : F(":Fail"));
  }  
}
