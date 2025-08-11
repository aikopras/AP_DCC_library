//******************************************************************************************************
//
// file:     ESP32.h
// purpose:  DCC receiving code for ESP32 processors, using standard Arduino calls. 
// author:   Aiko Pras (see note)
// version:  2025-08-10 V1.0.0 ap initial version
//
// note:     This code is derived from Generic.h. 
//           Therefore this library triggers on rising OR falling inputs (full bit). 
//           Half-bits are not captured, and timing may not be precise enough for RCN210. 
//
// Result:   1. The received message is collected in the struct "dccrec.tempMessage".
//           2. After receiving a complete message, data is copied to "dccMessage.data".
//           3. The flag "dccMessage.isReady" is set.
//
// Used hardware resources:
//  - INTx:   The DCC input signal (any available pin that is able to generate a hardware interrupt)
//
// Howto: Uses only one interrupt at the rising or falling edge of the DCC signal.
// The time between two edges is measured to determine the bit value. 
// The theoretical values are:
// - 104..128:           1 bit
// - 180..200 / 232:     0 bit (bit stretching is not supported)
// - 180..10000:         0 bit (bit stretching is supported )
// - 454..520 (roughly): Railcom 
// Since the ISR will be unable to measure these times very precise, and the resolution of micros()
// is 4us, some MARGINs are needed.
//
//******************************************************************************************************
#include <Arduino.h>
#include <esp_timer.h>
#include "../sup_isr.h"

//******************************************************************************************************
// 1. Time in micro seconds
//******************************************************************************************************
#define MARGIN      10                    // We accept some margin
#define MIN1        (104 - MARGIN)        // RCN210
#define MAX1        (128 + MARGIN)        // RCN210
#define MIN0        (180 - MARGIN)        // RCN210
#define MAX0        (232 + MARGIN)        // RCN210, without bit stretching
#define MINRAILCOM  (454 - MARGIN)
#define MAXRAILCOM  (520 + MARGIN)        // Around this value
#define MAXSTRETCH  (10000)               // RCN210, with bit stretching


//******************************************************************************************************
// 2. Declaration of external objects
//******************************************************************************************************
// The dccMessage contains the "raw" DCC packet, as received by the Timer2 ISR in this file
// It is instantiated in, and used by, DCC_Library.cpp
extern DccMessage dccMessage;


//******************************************************************************************************
// 3. Defines, definitions and instantiation of local types and variables
//******************************************************************************************************
// Type definitions
// Possible values for dccrecState
#define WAIT_PREAMBLE       (1<<0)
#define WAIT_START_BIT      (1<<1)
#define WAIT_DATA           (1<<2)
#define WAIT_END_BIT        (1<<3)

// Local variables
volatile struct {
  uint8_t bitCount;                           // Count number of preamble bits / if we have a byte
  uint8_t tempMessage[MaxDccSize];            // Once we have a byte, we store it in the temp message
  uint8_t tempMessageSize;                    // Here we keep track of the size, including XOR
} dccrec;                                     // The received DCC message is assembled here

// Variables used by ISR
volatile uint8_t dccrecState;
volatile uint8_t tempByte;                    // used in sup_isr_assemble_packet.h
volatile uint8_t DccBitVal;

volatile bool IsrRising = true;               // We start with this trigger edge
static uint8_t isrPin;                        // Needed within the ISR, to change the trigger mode


//******************************************************************************************************
// 4. DCC Interrupt Routine
//******************************************************************************************************
void IRAM_ATTR dcc_interrupt(void) {
  unsigned int microsNow;
  unsigned int microsDiff;
  static unsigned int microsLast = 0;       // Static, to keep its value between invocations
  microsNow = (uint32_t) esp_timer_get_time(); // Resolution of 1 us
  microsDiff = microsNow-microsLast;
  microsLast = microsNow;
  switch (microsDiff) {
    //case    0 ... (MIN1 - 1): 
    //return;                               // Ignore this pulse, so return immediately
    case MIN1 ... (MAX1 - 1): 
      DccBitVal = 1; 
    break;
    case MAX1 ... (MIN0 - 1):  
      // Should not happen. Maybe we should change the edge of the triggering
      // It seems there is no need to detach first
      if (IsrRising) attachInterrupt(digitalPinToInterrupt(isrPin), dcc_interrupt, FALLING );
                else attachInterrupt(digitalPinToInterrupt(isrPin), dcc_interrupt, RISING ); 
      IsrRising = !IsrRising;  
      dccrecState = WAIT_PREAMBLE ;         // drop all bits we already received for this packet 
      dccrec.bitCount = 0 ;                 // and start from scratch
    break;
    case MIN0 ... (MAX0 - 1):               // this setting does not support 0-bit stretching 
      DccBitVal = 0; 
    break;
    //case MAX0 ... (MINRAILCOM - 1):       // treat 0-bit stretching as an error
      // DccBitVal = 0;                     // 0-bit stretching is not supported
    //return;
    //case MINRAILCOM ... (MAXRAILCOM - 1): // Railcom
    //return;
    //case MAXRAILCOM ... (MAXSTRETCH):     // treat 0-bit stretching as an error 
      // DccBitVal = 0;                     // 0-bit stretching is not supported
    //return;
    default: 
    return;
  }
  #include "../sup_isr_assemble_packet.h"   // make from bits a packet
}


//******************************************************************************************************
// 5. attach() / detach()
//******************************************************************************************************
void DccMessage::attach(uint8_t dccPin, uint8_t ackPin) {
  // Initialize the local variables
  _dccPin = dccPin;                      // class variable. Needed for detach()
  isrPin = dccPin;                       // needed by ISR, to change interrupt mode: RISING <-> FALLING
  dccrecState = WAIT_PREAMBLE ;
  dccrec.bitCount = 0 ;
  tempByte = 0 ;
  // initialise the global variables (the DccMessage attributes)
  dccMessage.size = 0;
  dccMessage.isReady = 0;
  // Intialise the DCC interupt routine
  pinMode(dccPin, INPUT_PULLUP);
  if (IsrRising) attachInterrupt(digitalPinToInterrupt(dccPin), dcc_interrupt, RISING);
   else attachInterrupt(digitalPinToInterrupt(dccPin), dcc_interrupt, FALLING);  
  // Initialise the DCC Acknowledgement port, which is needed in Service Mode
  // If the main sketch doesn't specify this pin, the value 255 is provided as
  // (invalid) default.
  if (ackPin < 255) pinMode(ackPin, OUTPUT);
}


void DccMessage::detach(void) {
  // Detach the ISR
  detachInterrupt(digitalPinToInterrupt(_dccPin));
}

