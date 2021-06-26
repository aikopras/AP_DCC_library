//******************************************************************************************************
//
// file:     support_isr.cpp
// purpose:  DCC receiving code for ATmega AVRs (16, 328, 2560, ...) using the Arduino libraries
// author:   Aiko Pras
// version:  2021-05-15 V1.0.2 ap initial version
//
// Howto:    Uses two interrupt: a rising edge in DCC polarity triggers INTx and Timer 2
//           Within the INTx ISR, Timer2 with a delay of 77us is started.
//           On Timer2 Overflow Match the level of DCC is evaluated and parsed.
//
//                           |<-----116us----->|
//
//           DCC 1: _________XXXXXXXXX_________XXXXXXXXX_________
//                           ^-INT1
//                           |----77us--->|
//                                        ^Timer2 ISR: reads zero
//
//           DCC 0: _________XXXXXXXXXXXXXXXXXX__________________
//                           ^-INT0
//                           |----------->|
//                                        ^Timer2 ISR: reads one
//
// Result:   1. The received message is collected in the struct "dccrec.tempMessage".
//           2. After receiving a complete message, data is copied to "dccMessage.data".
//           3. The flag "dccMessage.isReady" is set.
//
// Note: also other strategies for decoding are possible, such as measuring the time difference
// between the rising and falling DCC signal (see for example Nmra_Dcc-2)
//
//******************************************************************************************************
#include "Arduino.h"
#include "AP_DCC_library.h"
#include "sup_isr.h"


//******************************************************************************************************
// 1. Declaration of external objects
//******************************************************************************************************
// The dccMessage contains the "raw" DCC packet, as received by the Timer2 ISR in this file
// It is instantiated in, and used by, DCC_Library.cpp
extern DccMessage dccMessage;

// For certain types of decoders (such as occupancy detectors) the ADC hardware will be initiated
// To activate that code "VOLTAGE_DETECTION" must be defined within "AP_DCC_library.h"
#if defined(VOLTAGE_DETECTION)
#warning "HALLO"
AdcStart adcStart;
#endif


//******************************************************************************************************
// 2. Definitions and instantiation of local types and variables
//******************************************************************************************************
// Type definitions
typedef enum {
  WAIT_PREAMBLE = 0,
  WAIT_START_BIT,
  WAIT_DATA,
  WAIT_END_BIT
} state_t;

// Local variables
struct {
  state_t state;                              // Determines if we now receive the preamble, data etc.
  uint8_t bitCount;                           // Count number of preamble bits / if we have a byte
  uint8_t tempByte;                           // Bits received from the DCC input pin are stored here
  volatile uint8_t tempMessage[MaxDccSize];   // Once we have a byte, we store it in the temp message
  volatile uint8_t tempMessageSize;           // Here we keep track of the size, including XOR
} dccrec;                                     // The received DCC message is assembled here

struct {
  uint8_t port;                               // PA=1, PB=2, PC=3, PD=4 etc.
  uint8_t bit;                                // Bitmask for reading the input Port
  volatile uint8_t *portRegister;             // PINA=$39, PINB=$36, PINC=$34, PIND=$30
} dccIn;                                      // Direct port access, to read the DCC signal fast


//******************************************************************************************************
// 3. Initialisation of the timer2 interrupt routine
//******************************************************************************************************
void init_timer2(void) {
  // Timer 2 (TCNT2) will be used in overflow mode.
  // If the TCNT2 overflows, the corresponding ISR(TIMER2_OVF_vect) will be called.
  // A prescaler is used to divide the clockspeed into a lower speed.
  // At init (but again after the timer overflow in the ISR) TCNT2 is preloaded with a start value
  // The timer is started within the DCC-IN ISR, and stopped in the Timer2 Overflow ISR
  //
  // The following Timer2 related registers exist:
  // - TCNT2:    Timer/Counter 2
  // - TCCR2A:   Timer/Counter Control Register A - should be zero for our purpose
  // - TCCR2(B): Timer/Counter Control Register B - should hold the prescaler. 0 = stop
  // - TIMSK2:   Timer Interrupt Mask Register - Bit 0 (TOIE2) determines we use the counter in Overflow mode
  //             TOIE = Timer Overflow Interrupt Enable
  // - There are also some other registers (like OCR2A and OCR2B), but these are not needed

  // Step 1: Determine the optimal value for the PRESCALER
  // If X-tal = 16.000.000 Hz, a Prescaler of 8 gives for T77US => 154
  // If X-tal = 11.059.200 Hz, a Prescaler of 8 gives for T77US => 106,4448
  #define T2_PRESCALER   8    // acceptable values are 1, 8, 64, 256, 1024
  #if   (T2_PRESCALER==1)
    #define T2_PRESCALER_BITS   ((0<<CS02)|(0<<CS01)|(1<<CS00))
  #elif (T2_PRESCALER==8)
    #define T2_PRESCALER_BITS   ((0<<CS02)|(1<<CS01)|(0<<CS00))
  #elif (T2_PRESCALER==64)
    #define T2_PRESCALER_BITS   ((0<<CS02)|(1<<CS01)|(1<<CS00))
  #elif (T2_PRESCALER==256)
    #define T2_PRESCALER_BITS   ((1<<CS02)|(0<<CS01)|(0<<CS00))
  #elif (T2_PRESCALER==1024)
    #define T2_PRESCALER_BITS   ((1<<CS02)|(0<<CS01)|(1<<CS00))
  #endif

  // Step 2: Define 77 microseconds
  // The idea is to check after 3/4 of a "one" signal has passed
  // This is a time window of 116 * 0,75 = 87us minus 10 us for safety
  #define T77US (F_CPU * 77L / T2_PRESCALER / 1000000L)
  // Check if prescaler gives a good value for T77US (thus fits for this X-Tal)
  #if (T77US > 254)
    #warning T77US too big, use either larger prescaler or slower processor
  #endif
  #if (T77US < 32)
    #warning T77US too small, use either smaller prescaler or faster processor
  #endif

  // Step 3: Set the Timer 2 registers
  // The timer will be used in overflow mode, so an interrupt occurs at MAX+1 (=256)
  // We preload the Timer Value to 256 - T77US in this init routine plus the Timer ISR
  // and not in the DCC Interrupt routine, where the timer is actually started.
  // Therefore, if there are glitches in the DCC Input signal that lead to multiple
  // DCC Interrupts, only the first interrupt matters.
  noInterrupts();              // disable all interrupts
  // Stop the timer and preload its value for the next cycle
  // TODO SHOULD THIS BE #if defined OR #ifdef
  #if defined(TCCR2)           // ATMEGA 8535, 16, 32, 64, 162 etc
  TCCR2 = 0;                   // 0 => timer is stopped
  TCNT2 = 256L - T77US;        // preload the timer
  TIMSK |= (1 << TOIE2);       // the timer is used in overflow interrupt mode
  #elif defined(TCCR2A)        // ATMEGA 328, 2560 etc.
  TCCR2A = 0;                  // should be zero for our purpose
  TCCR2B = 0;                  // 0 => timer is stopped
  TCNT2 = 256L - T77US;        // preload the timer
  TIMSK2 |= (1 << TOIE2);      // the timer is used in overflow interrupt mode
  #endif
  interrupts();                // enable all interrupts
}


//******************************************************************************************************
// 4. DCC Interrupt Routine
//******************************************************************************************************
void dcc_interrupt(void) {
  // After each DCC interrupt Timer 2 will be started
  #if defined(TCCR2)              // ATMEGA 8535, 16, 32, 64, 162 etc
  TCCR2 |= (T2_PRESCALER_BITS);   // Start Timer 2
  #elif defined(TCCR2B)           // ATMEGA 328, 2560 etc.
  TCCR2B |= (T2_PRESCALER_BITS);  // Start Timer 2
  #endif
}


//******************************************************************************************************
// 5. attach() / detach()
//******************************************************************************************************
// The DCC input signal should be connected to dccPin
// The interrupt routine that reads the value of that pin must be fast.
// The traditional Arduino digitalRead() function is relatively slow, certainly when compared to direct
// port reading, which can be done (for example) as follows: DccBitVal = !(PIND & (1<<PD3);
// However, direct port reading has as disadvantage that the port and mask should be hardcoded,
// and can not be set by the main sketch, thus the user of this library.
// Therefore we take a slightly slower approach, and use a variable called "portInputRegister",
// which points to the right input port, and "bit", which masks the selected input port.
// To use "portInputRegister" and "bit", we basically split the Arduino digitalRead() function into:
// 1) an initialisation part, which maps dccPin to "portInputRegister" and "bit". This part may be slow
// 2) The actual reading from the port, which is fast.
//
// Comparison between approaches:               Flash  RAM  Time  Delta
//   value = (PINC & bit);                        6     1   1,09    -
//   value = (*portRegister & bit);              14     1   1,54   0,45
//   value = (*portInputRegister(port) & bit);   28     2   2,37 . 1,28
//   value = digitalRead(pin);                   88     0   3,84   2,75
//           Note: Flash & RAM in bytes / Time & Delta in microseconds


void DccMessage::attach(uint8_t dccPin, uint8_t ackPin) {
  // Initialize the local variables
  _dccPin = dccPin;
  dccrec.state = WAIT_PREAMBLE ;
  dccrec.bitCount = 0 ;
  dccrec.tempByte = 0 ;
  // initialise the global variables (the DccMessage attributes)
  dccMessage.size = 0;
  dccMessage.isReady = 0;
  // In case of occupancy detection decoders we may start AD conversion
  #if defined(VOLTAGE_DETECTION)
  adcStart.newRequest = 0;
  #endif
  // Initialize Timer2 before the DCC ISR, since that ISR will start the timer
  init_timer2();
  // Intialise the DCC interupt routine
  pinMode(dccPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(dccPin), dcc_interrupt, RISING );
  // Map from dccPin to a pointer towards the port, as well as a bitmask
  dccIn.port = digitalPinToPort(dccPin);
  dccIn.bit = digitalPinToBitMask(dccPin);
  dccIn.portRegister = portInputRegister(dccIn.port);
  // Initialise the DCC Acknowledgement port, which is needed in Service Mode
  // If the main sketch doesn't specify this pin, the value 255 is provided as
  // (invalid) default.
  if (ackPin < 255) pinMode(ackPin, OUTPUT);
}


void DccMessage::detach(void) {
  // Detach the ISR
  detachInterrupt(digitalPinToInterrupt(_dccPin));
  // Stop the Timer 2
  #if defined(TCCR2)           // ATMEGA 8535, 16(A), 162 etc
  TCCR2 = 0;                   // 0 => timer is stopped
  #elif defined(TCCR2B)        // ATMEGA 328, 2560 etc
  TCCR2B = 0;                  // 0 => timer is stopped
  #endif
}


//******************************************************************************************************
// 6. The Timer2 ISR, which implements the DCC Receive Routine
//******************************************************************************************************
// Timer2 Interrupt Routine: read the value of the DCC signal
// Execution of this DCC Receive code typically takes between 3 and 8 microseconds.
ISR(TIMER2_OVF_vect) {
  // Read the DCC input value, if the input is low then its a 1 bit, otherwise it is a 0 bit
  uint8_t DccBitVal;
  DccBitVal = !(*dccIn.portRegister & dccIn.bit);
  // DccBitVal = !(PIND & (1<<PD3);      // Direct port access: fast but hardcoded
  // DccBitVal = !digitalRead(DCC_IN);   // Standard Arduino, flexible but slow

  // Stop the timer and preload its value for the next cycle
  #if defined(TCCR2)           // ATMEGA 8535, 16(A), 162 etc
  TCCR2 = 0;                   // 0 => timer is stopped
  #elif defined(TCCR2B)        // ATMEGA 328, 2560 etc
  TCCR2B = 0;                  // 0 => timer is stopped
  #endif
  TCNT2 = 256L - T77US;        // preload the timer

  // Next lines are needed for occupancy decoders (Gleis Besetzt Meldung - GBM)
  // Start new ADC in case the ADC read process is ready and some time (like 1ms) have passed.
  // We start new AD conversions in case the Microcontroller input line is high
  // This is when we have a DCC 0 bit
  #if defined(VOLTAGE_DETECTION)
  if (adcStart.newRequest) {
    if (DccBitVal == 0) {
      ADCSRA |= (1 << ADSC);         // Start the new ADC measurements
      adcStart.newRequest = 0;
    }
  }
  #endif

  dccrec.bitCount++;

  switch( dccrec.state )
  {
  // According to NMRA standard S9.2, a packet consists of:
  // - Preamble
  // - Packet Start Bit
  // - Address Data Byte:
  // - Data Byte Start Bit + Data Byte [0 or more times]
  // - Packet End Bit


  case WAIT_PREAMBLE:
    // The preamble to a packet consists of a sequence of "1" bits.
    // A digital decoder must not accept as a valid, any preamble
    // that has less then 10 complete one bits
    if( DccBitVal )                                   // a "1" bit is received
    {
      if( dccrec.bitCount > 10 )
        dccrec.state = WAIT_START_BIT;
    }
    else
    {
      dccrec.bitCount = 0;                            // not a valid preamble.
    }
    break;

  case WAIT_START_BIT:
    // The packet start bit is the first bit with a value of "0"
    // that follows a valid preamble. The packet start bit terminates the preamble
    // and indicates that the next bits are an address data byte
    if( !DccBitVal )                                  // a "0" bit is received
    {
      dccrec.state = WAIT_DATA;
    dccrec.tempMessageSize = 0;
      // Initialise all fields
      uint8_t i;
      for(i = 0; i< MaxDccSize; i++ )
        dccrec.tempMessage[i] = 0;
      dccrec.bitCount = 0;
      dccrec.tempByte = 0;
    }
    break;

  case WAIT_DATA:
    dccrec.tempByte = (dccrec.tempByte << 1);         // move all bits left
    if (DccBitVal)
      dccrec.tempByte |= 1;                           // add a "1"
    if( dccrec.bitCount == 8 )                        // byte is complete
    {
      if(dccrec.tempMessageSize == MaxDccSize )       // Packet is too long - abort
      {
        dccrec.state = WAIT_PREAMBLE;
        dccrec.bitCount = 0;
      }
      else
      {
        dccrec.state = WAIT_END_BIT;                  // Wait for next byte or end of packet
        dccrec.tempMessage[dccrec.tempMessageSize++ ] = dccrec.tempByte;
      }
    }
    break;

  case WAIT_END_BIT:
    // The next bit is either a Data Byte Start Bit or a Packet End Bit
    // Data Byte Start Bit: precedes a data byte and has the value of "0"
    // Packet End Bit: marks the termination of the packet and has a value of "1"
    if( DccBitVal ) // End of packet?
    {
      // Complete packet received and no errors
      uint8_t i;
      uint8_t bytes_received;
      bytes_received = dccrec.tempMessageSize;
      for (i=0; i < bytes_received; i++) {
        dccMessage.data[i] = dccrec.tempMessage[i];
        }
      dccMessage.size = bytes_received;
      dccrec.state = WAIT_PREAMBLE;
      // tell the main program we have a new valid packet
      noInterrupts();
      dccMessage.isReady = 1;
      interrupts();
    }
    else  // Get next Byte
    dccrec.state = WAIT_DATA;
    dccrec.bitCount = 0;                              // prepare for the next byte
    dccrec.tempByte = 0;
  }
}
