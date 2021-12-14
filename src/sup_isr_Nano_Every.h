//******************************************************************************************************
//
// file:     sup_isr_Nano_Every.h
// purpose:  DCC receiving code for the Arduino Nano Every
//           Note: Performance may be problematic. Install and use the MegaCoreX board instead.
//           See: https://github.com/MCUdude/MegaCoreX
// author:   Aiko Pras
// version:  2021-09-01 V1.0.0 ap Initial version
//
// Result:   1. The received message is collected in the struct "dccrec.tempMessage".
//           2. After receiving a complete message, data is copied to "dccMessage.data".
//           3. The flag "dccMessage.isReady" is set.
//
//  - DccPin: Any ATmega pin can be used to receive the DCC input signal
//            Polarity of the J/K signal is important.
//  - Timer:  One TCB timer. Default is TCB0, but the default can be changed by setting one of the
//            #defines (DCC_USES_TIMERB1, DCC_USES_TIMERB2 or DCC_USES_TIMERB3)
//            No dependancies exist with other timers, thus the TCA prescaler is NOT used.

// Howto: Uses two interrupt: Pin interrupt and Timer interrupt.
// A rising edge in DCC polarity triggers the Pin ISR, which starts TCBx in Periodic Interrupt mode.
// On TCBx  the level of DCC is evaluated and parsed.
// The Timer period is 67us (instead of77!), to keep enough headroom for the (high) overhead of 
// the Ardiuno attachInterupt() call, which is due to the different Interrupt system on ATmegaX
// processors much higher than on the traditional ATmega processors. 
//
//                           |<-----116us----->|
//
//           DCC 1: _________XXXXXXXXX_________XXXXXXXXX_________
//                           ^-INT1
//                           |----67us--->|
//                                        ^TCBx ISR: reads zero
//
//           DCC 0: _________XXXXXXXXXXXXXXXXXX__________________
//                           ^-INT0
//                           |----------->|
//                                        ^TCBx ISR: reads one
//
//******************************************************************************************************
#include <Arduino.h>
#include "sup_isr.h"


//******************************************************************************************************
// 1. Declaration of external objects
//******************************************************************************************************
// The dccMessage contains the "raw" DCC packet, as received by the Timer2 ISR in this file
// It is instantiated in, and used by, DCC_Library.cpp
extern DccMessage dccMessage;


//******************************************************************************************************
// 2. Defines that may need to be modified to accomodate certain hardware
//******************************************************************************************************
// Timer to use: TIMERB0 (TCB0), TIMERB1 (TCB1), TIMERB2 (TCB2) or TIMERB3 (TCB3)
// In many cases TCB0 or TCB1 are still available for own use.
// The default is TCB0, which is available on all processor variants.
// This can be overruled by setting one of the following defines:
// #define DCC_USES_TIMERB1
// #define DCC_USES_TIMERB2
// #define DCC_USES_TIMERB3

// GPIOR (General Purpose IO Registers) are used to store global flags and temporary bytes.
// In case the selected GPIORs conflict with other libraries, change to any any free GPIOR
// or use a volatile uint8_t variable
#define dccrecState GPIOR0                     // fast, but requires a free GPIOR
// volatile uint8_t dccrecState;               // slow, but more portable
#define tempByte GPIOR1                        // fast, but requires a free GPIOR
// volatile uint8_t tempByte;                  // slow, but more portable


//******************************************************************************************************
// 3. Defines, definitions and instantiation of local types and variables
//******************************************************************************************************
// Possible values for dccrecState
#define WAIT_PREAMBLE       (1<<0)
#define WAIT_START_BIT      (1<<1)
#define WAIT_DATA           (1<<2)
#define WAIT_END_BIT        (1<<3)

struct {
  uint8_t bitCount;                           // Count number of preamble bits / if we have a byte
  volatile uint8_t tempMessage[MaxDccSize];   // Once we have a byte, we store it in the temp message
  volatile uint8_t tempMessageSize;           // Here we keep track of the size, including XOR
} dccrec;                                     // The received DCC message is assembled here

struct {
  uint8_t port;                               // PA=1, PB=2, PC=3, PD=4 etc.
  uint8_t bit;                                // Bitmask for reading the input Port
  volatile uint8_t *portRegister;             // PINA=$39, PINB=$36, PINC=$34, PIND=$30
} dccIn;                                      // Direct port access, to read the DCC signal fast

static volatile TCB_t* _timer;                // Use a pointer to the timer


//******************************************************************************************************
// 4. Initialisation of the TCB timer interrupt routine
//******************************************************************************************************
void init_timer(void) {
  // Step 1: Calculate from TIME_US the right value for TOP
  // The clock for the timer (CLK_PER) runs at the same speed as the clock for the CPU (CLK_CPU).
  // This will in most cases by 16Mhz or 20Mhz.
  // With 20Mhz a clock pulse takes 50ns, in which case TOP will become 154 to get 77us.
  // With 16Mhz a clock pulse takes 62,5ns, in which case TOP will become 123.
  // The code will automatically calculate the setting of TOP to the CPU clock.
  // For details, see: TB3214-Getting-Started-with-TCB-DS90003214-2.pdf
  // Use 66us instead of 77us!!
  #define TIME_US 66                        // Time (in microseconds) between timer interrupts
  #define TOP F_CPU / 1000000  * TIME_US    //
  #if (F_CPU / 1000000  * TIME_US >= 65535)
    #error TOP will overflow. Select a smaller TIME_US
  #endif
  #if (F_CPU / 1000000  * TIME_US <= 500)
    #error TIME_US is too small, giving a low TOP thus too much load. Select a larger TIME_US
  #endif

  // Step 2: Instead of calling a specific timer directly, we use a pointer to the selected timer
  #if defined(WE_USE_TIMERB0)
    _timer = &TCB0;
  #elif defined(WE_USE_TIMERB1)
    _timer = &TCB1;
  #elif defined(WE_USE_TIMERB2)
    _timer = &TCB2;
  #elif defined(WE_USE_TIMERB3)
    _timer = &TCB3;
  #else
    // fallback to TCB0 (every platform has it)
    _timer = &TCB0;
  #endif

  // Step 3: fill the registers.
  noInterrupts();
  // Stop the timer
  _timer->CTRLA &= ~TCB_ENABLE_bm;
  // Set the counter back to zero
  _timer->CNT = 0;
  // Reset both CTRL registers, to ensure the TCB is in Periodic Interrupt mode
  // and the clock is CLK_PER (the 16/20Mhz clock) .
  _timer->CTRLA = 0;
  _timer->CTRLB = 0;
  // Enable CAPT interrupts by writing a ‘1’ to the ENABLE bit in the Interrupt Control (TCBn.INTCTRL) register.
  _timer->INTCTRL |= TCB_ENABLE_bm;
  // Write a TOP value to the Compare/Capture (TCBn.CCMP) register.
  _timer->CCMP = TOP;
  // Enable the counter by writing a ‘1’ to the ENABLE bit in the Control A (TCBn.CTRLA) register.
  interrupts();
}


//******************************************************************************************************
// 4. DCC Input pin Interrupt Routine
//******************************************************************************************************
void dcc_interrupt(void) {
  // After each DCC interrupt the Timer will be restarted
  _timer->CNT = 0;                           // reset count
  _timer->CTRLA |= TCB_ENABLE_bm;            // start timer
}


//******************************************************************************************************
// 5. attach() / detach()
//******************************************************************************************************
// The DCC input signal should be connected to the dccPin
// The interrupt routine that reads the value of that pin must be fast.
// The traditional Arduino digitalRead() function is relatively slow, certainly when compared to direct
// port reading, which can be done (for example) as follows: DccBitVal = !(PIND & (1<<PD3);
// However, direct port reading has as disadvantage that the port and mask should be hardcoded,
// and can not be set by the main sketch, thus the user of this library.
// Therefore we take a slightly slower approach, and use a pointer called "portInputRegister",
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
//
// Advantages of direct port access:
// - Nearly as fast as direct port reading
// - As opposed to `(PINC & bit)` constructs, works without modification also for MegaX processors.
// Disadvantages of direct port access:
// - The functions digitalPinToPort(), digitalPinToBitMask() and portInputRegister()
//   are internal and non-documented Arduino functions, and may therefore be replaced.

void DccMessage::attach(uint8_t dccPin, uint8_t ackPin) {
  // Initialize the local variables
  _dccPin = dccPin;
  tempByte = 0;
  dccrecState = WAIT_PREAMBLE;
  dccrec.bitCount = 0 ;
  // initialise the global variables (the DccMessage attributes)
  dccMessage.size = 0;
  dccMessage.isReady = 0;
  // Initialize Timer before the DCC ISR, since that ISR will start the timer.
  init_timer();
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
  noInterrupts();
  detachInterrupt(digitalPinToInterrupt(_dccPin));
  // Clear all TCB timer settings
  // For "reboot" (jmp 0) it is crucial to set INTCTRL = 0
  _timer->CTRLA = 0;
  _timer->CTRLB = 0;
  _timer->INTCTRL = 0;
  _timer->CCMP = 0;
  _timer->CNT = 0;
  _timer->INTFLAGS = 0;
  interrupts();
}


//******************************************************************************************************
// 6. The Timer ISR, which implements the DCC Receive Routine
//******************************************************************************************************
// Timer Interrupt Routine: read the value of the DCC signal
// Execution of this DCC Receive code typically takes between 3 and 8 microseconds.
// Select the corresponding ISR
#if defined(WE_USE_TIMERB0)
ISR(TCB0_INT_vect) {
#elif defined(WE_USE_TIMERB1)
ISR(TCB1_INT_vect) {
#elif defined(WE_USE_TIMERB2)
ISR(TCB2_INT_vect) {
#elif defined(WE_USE_TIMERB3)
ISR(TCB3_INT_vect) {
#else
// fallback to TCB0 (every platform has it)
ISR(TCB0_INT_vect) {
#endif
  // Read the DCC input value, if the input is low then its a 1 bit, otherwise it is a 0 bit
  uint8_t DccBitVal;     
  DccBitVal = !(*dccIn.portRegister & dccIn.bit);
  // Stop TCBx and preload its value for the next cycle
  _timer->INTFLAGS |= TCB_CAPT_bm;      // We had an interrupt
  _timer->CTRLA &= ~TCB_ENABLE_bm;      // Clear Timer Enable
  _timer->CNT = 0;                      // CNT will not reset automatically
  // Add the captured byte to the message 
  #include "sup_isr_assemble_packet.h"
}
