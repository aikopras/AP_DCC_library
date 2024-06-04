//******************************************************************************************************
//
// file:     sup_isr_MegaCoreX_DxCore.h
// purpose:  DCC receive code for ATmegaX processors, such as ATmega 4808, 4809, AVR DA, AVR DB, ...
//           using a MegaCoreX or DxCore board
// author:   Aiko Pras
// version:  2021-09-01 V1.2.0 ap - Uses the Event system to connect the DCC input pin to a TCB timer.
//           2024-06-04 V1.2.1 ap - Error corrected in cases where the preamble had an uneven number
//                                  of halfbits. This error showed up with DxCore and the Z21 system.
//                                  Some comments are added for implementing RailCom feedback.
//
// Result:   1. The received message is collected in the struct "dccrec.tempMessage".
//           2. After receiving a complete message, data is copied to "dccMessage.data".
//           3. The flag "dccMessage.isReady" is set.
//
// Used hardware resources:
//  - DccPin: Any ATmega pin can be used to receive the DCC input signal
//            Polarity of the J/K signal is not important.
//  - Timer:  One TCB timer. Default is TCB0, but the default can be changed by setting one of the
//            #defines (DCC_USES_TIMERB1, DCC_USES_TIMERB2 or DCC_USES_TIMERB3)
//            No dependancies exist with other timers, thus the TCA prescaler is NOT used.
//  - Event:  One of the Event channels available in ATmegaX processors.
//            The software automatically selects an available channel.
//            Not every pin can be connected to every Event channel. If other software has already
//            claimed usage of some channels, it might be necessary to select a DCC input pin on 
//            another port. See also: https://github.com/MCUdude/MegaCoreX#event-system-evsys
//
// Howto: This part of the library takes advantage of the Event system peripheral that is implemented
// in the novel ATmegaX processors, and which is supported by MegaCoreX and DxCore.
// The DCC pin is used as input for an Event channel. The output of that Event channel triggers a TCB
// timer in 'Capture Frequency Measurement Mode'. If an event occurs in this Mode, the TCB
// captures the counter value (TCBn.CNT) and stores this value in TCBn.CCMP. The Interrupt flag is
// automatically cleared after the low byte of the Compare/Capture (TCBn.CCMP) register is read.
// The value in TCBn.CCMP is the number of ticks since the previous event, thus the time since the
// previous DCC signal transition. This time determines if the received DCC signal represents (half of)
// a zero or one bit.
// Since we do not want to introduce any dependency between our TCB timer and the TCA prescaler, 
// TCB is used at clock speed. This requires us to use TCB in 16 bit mode. If we would have used the
// TCA prescaler, TCB could have been run in 8-bit mode, saving us roughly 8 clock cycli.
// To conform to the RCN-210 standard, we measure the times for both first as well as second half bit.
// Therefore the TCB ISR is called twice for every DCC bit.
// Since the TCB can only trigger on positive or negative edges of the input signal, the trigger
// direction is inverted after each interrupt.
//
// This code uses the Event system of MegaCoreX and DxCore:
// https://github.com/MCUdude/MegaCoreX#event-system-evsys
// https://github.com/SpenceKonde/DxCore 
//
// Note: Railcom feedback has not been implemented.
// Implementation could be relatively easy by using an additional timer to determine the exact moment
// the railcom data should be send. This moment could be the moment that the Packet End Bit is detected
// (see sup_isr_assemble_packet.h). Once this railcom timer fires, a UART starts sending the railcom
// data. The Event and CCL peripherals can be used to connect the RailCom timer with the UART and the
// RailCom output pin.
//
//******************************************************************************************************
#include <Arduino.h>
#include <Event.h>
#include "sup_isr.h"


//******************************************************************************************************
// 1. Declaration of external objects
//******************************************************************************************************
// The dccMessage contains the "raw" DCC packet, as received by the TCB ISR in this file
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
// For example, in case of dccHalfBit, GPIOR saves roughly 8 clock cycli per interrupt.
// In case the selected GPIORs conflict with other libraries, change to any any free GPIOR
// or use a volatile uint8_t variable
#define dccrecState GPIOR0                     // fast: saves 3 clock cycli, but requires a free GPIOR
// volatile uint8_t dccrecState;               // slow, but more portable
#define tempByte GPIOR1                        // fast
// volatile uint8_t tempByte;                  // slow
#define dccHalfBit GPIOR2                      // fast
// volatile uint8_t dccHalfBit;                // slow


//******************************************************************************************************
// 3. Defines, definitions and instantiation of local types and variables
//******************************************************************************************************
// Values for half bits from RCN 210, section 5: http://normen.railcommunity.de/RCN-210.pdf
#define ONE_BIT_MIN F_CPU / 1000000 * 52
#define ONE_BIT_MAX F_CPU / 1000000 * 64
#define ZERO_BIT_MIN F_CPU / 1000000 * 90
#define ZERO_BIT_MAX F_CPU / 1000000 * 119


// #define ZERO_BIT_MAX 65535
// Change the defines for ZERO_BIT_MAX to enable (disable) zero-bit stretching.
// To avoid integer overflow, we don't allow 10000 microseconds as maximum, but 65535 (maxint)
// For a 16Mhz clock this is equivalent to 4096 microseconds. This might work on most systems.
// For a discussion of zero bit streching see: https://github.com/littleyoda/sigrok-DCC-Protocoll/issues/4

// Possible values for dccrecState
#define WAIT_PREAMBLE       (1<<0)
#define WAIT_START_BIT      (1<<1)
#define WAIT_DATA           (1<<2)
#define WAIT_END_BIT        (1<<3)

// Possible values for dccHalfBit
#define EXPECT_ZERO         (1<<0)
#define EXPECT_ONE          (1<<1)
#define EXPECT_ANYTHING     (1<<2)

struct {
  uint8_t bitCount;                           // Count number of preamble bits / if we have a byte
  volatile uint8_t tempMessage[MaxDccSize];   // Once we have a byte, we store it in the temp message
  volatile uint8_t tempMessageSize;           // Here we keep track of the size, including XOR
} dccrec;                                     // The received DCC message is assembled here

static volatile TCB_t* _timer;                // In init and detach we use a pointer to the timer


//******************************************************************************************************
// 4. Initialise timer and event system
//******************************************************************************************************
void DccMessage::initTcb(void) {
  // Step 1: Instead of calling a specific timer directly, in init and detach we use a pointer to the
  // selected timer. However, since pointers add a level of indirection, within the timer ISR the two
  // registers that we use (EVCTRL and CCMP) will be directly accessed via #defines
  #if defined(DCC_USES_TIMERB0)
    _timer = &TCB0;
    #define timer_EVCTRL TCB0_EVCTRL
    #define timer_CCMPL  TCB0_CCMPL       // 8 bit (not used in this implementation)
    #define timer_CCMP   TCB0_CCMP        // 16 bit
  #elif defined(DCC_USES_TIMERB1)
    _timer = &TCB1;
    #define timer_EVCTRL TCB1_EVCTRL
    #define timer_CCMPL  TCB1_CCMPL
    #define timer_CCMP   TCB1_CCMP
  #elif defined(DCC_USES_TIMERB2)
    _timer = &TCB2;
    #define timer_EVCTRL TCB2_EVCTRL
    #define timer_CCMPL  TCB2_CCMPL
    #define timer_CCMP   TCB2_CCMP
  #elif defined(DCC_USES_TIMERB3)
    _timer = &TCB3;
    #define timer_EVCTRL TCB3_EVCTRL
    #define timer_CCMPL  TCB3_CCMPL
    #define timer_CCMP   TCB3_CCMP
  #else  
    // fallback to TCB0 (every platform has it)
    _timer = &TCB0;
    #define timer_EVCTRL TCB0_EVCTRL
    #define timer_CCMPL  TCB0_CCMPL
    #define timer_CCMP   TCB0_CCMP
  #endif
  // Step 2: fill the registers. See the data sheets for details
  noInterrupts();
  // Clear the main timer control registers. Needed since the Arduino core creates some presets
  _timer->CTRLA = 0;
  _timer->CTRLB = 0;
  _timer->EVCTRL = 0;
  _timer->INTCTRL = 0;
  _timer->CCMP = 0;
  _timer->CNT = 0;
  _timer->INTFLAGS = 0;
  // Initialise the control registers
  _timer->CTRLA = TCB_ENABLE_bm;                    // Enable the TCB peripheral, clock is CLK_PER (=F_CPU)
  _timer->CTRLB = TCB_CNTMODE_FRQ_gc;               // Input Capture Frequency Measurement mode
  _timer->EVCTRL = TCB_CAPTEI_bm | TCB_FILTER_bm;   // Enable input capture events and noise cancelation
  _timer->INTCTRL |= TCB_CAPT_bm;                   // Enable CAPT interrupts
  interrupts();
}


void DccMessage::initEventSystem(uint8_t dccPin) {
  // Note: this code uses the new Event Library of MegaCoreX / DxCore
  noInterrupts();
  Event& myEvent = Event::assign_generator_pin(dccPin);
  #if defined(DCC_USES_TIMERB0)
    myEvent.set_user(user::tcb0_capt);
  #elif defined(DCC_USES_TIMERB1)
    myEvent.set_user(user::tcb1_capt);
  #elif defined(DCC_USES_TIMERB2)
    myEvent.set_user(user::tcb2_capt);
  #elif defined(DCC_USES_TIMERB3)
    myEvent.set_user(user::tcb3_capt);
  #else
    // fallback to TCB0 (every platform has it)
    myEvent.set_user(user::tcb0_capt);
  #endif
  myEvent.start();
  interrupts();
}


//******************************************************************************************************
// 5. attach() / detach()
//******************************************************************************************************
void DccMessage::attach(uint8_t dccPin, uint8_t ackPin) {
  // Initialize the local variables
  _dccPin = dccPin;
  tempByte = 0;
  dccrecState = WAIT_PREAMBLE;
  dccrec.bitCount = 0;
  // initialise the global variables (the DccMessage attributes)
  dccMessage.size = 0;
  dccMessage.isReady = 0;
  // Initialize the peripherals: (TCBx) timer and the Event system.
  initTcb();
  initEventSystem(dccPin);
  // Initialise the DCC Acknowledgement port, which is needed in Service Mode
  // If the main sketch doesn't specify this pin, the value 255 is provided as
  // (invalid) default.
  if (ackPin < 255) pinMode(ackPin, OUTPUT);
}


void DccMessage::detach(void) {
  noInterrupts();
  // Clear all TCB timer settings
  // For "reboot" (jmp 0) it is crucial to set INTCTRL = 0
  _timer->CTRLA = 0;
  _timer->CTRLB = 0;
  _timer->EVCTRL = 0;
  _timer->INTCTRL = 0;
  _timer->CCMP = 0;
  _timer->CNT = 0;
  _timer->INTFLAGS = 0;
  // Stop the Event channel  interrupts();
  interrupts();
}


//******************************************************************************************************
// 6. The Timer ISR, which implements the DCC Receive Routine
//******************************************************************************************************
// Execution of this DCC Receive code typically takes between 3 and 8 microseconds.
// Select the corresponding ISR
#if defined(DCC_USES_TIMERB0)
  ISR(TCB0_INT_vect) {
#elif defined(DCC_USES_TIMERB1)
  ISR(TCB1_INT_vect) {
#elif defined(DCC_USES_TIMERB2)
  ISR(TCB2_INT_vect) {
#elif defined(DCC_USES_TIMERB3)
  ISR(TCB3_INT_vect) {
#elif defined(DCC_USES_TIMERB4)
    ISR(TCB4_INT_vect) {
#else
  // fallback to TCB0 (every platform has it)
  ISR(TCB0_INT_vect) {
#endif
  
  timer_EVCTRL ^= TCB_EDGE_bm;                         // Change the event edge at which we trigger
  uint16_t  delta = timer_CCMP;                        // Delta holds the time since the previous interrupt 
  uint8_t DccBitVal;

  if ((delta >= ONE_BIT_MIN) && (delta <= ONE_BIT_MAX)) {
    if (dccHalfBit & EXPECT_ONE) {                     // This is the second part of the 1 bit
      dccHalfBit = EXPECT_ANYTHING; 
      DccBitVal = 1;
    }
    else if (dccHalfBit & EXPECT_ANYTHING) {           // This is the first part of the 1 bit
      dccHalfBit = EXPECT_ONE;
      return;
    }
    else {                                             // We expected a 0, but received 1 => abort
      timer_EVCTRL ^= TCB_EDGE_bm;                     // Likely J/K should be changed
      dccHalfBit = EXPECT_ANYTHING;
      dccrecState = WAIT_PREAMBLE;
      dccrec.bitCount = 0;
      return;
    }
  }
  else if ((delta >= ZERO_BIT_MIN) && (delta <= ZERO_BIT_MAX)) {
    if (dccHalfBit & EXPECT_ZERO) {                    // This is the second part of the 0 bit
      dccHalfBit = EXPECT_ANYTHING;
      DccBitVal = 0;
      }
    else if (dccHalfBit & EXPECT_ANYTHING) {           // This is the first part of the 0 bit
      dccHalfBit = EXPECT_ZERO;
      return;
    }
    else {                                             // We expected a 1, but received 0
      // Modified 2024/06/03 to correct an issue with the Z21 command station, which could
      // send, depending of the polarity of J and K, an uneven number of preamble half bits.
      // So we received a 0-halfbit, although we expected the second part of a 1-halfbit.
      // This can happen if the preamble has an uneven number of 1-halfbits, for example after
      // a RailCom CutOut. In such case this would be the first half of the Packet Start-bit
      if (dccrecState & WAIT_START_BIT) {              // are we still in the preamble?
        dccHalfBit = EXPECT_ZERO;
        return;
      }
      else {                                           // This should not happen.
        dccHalfBit = EXPECT_ANYTHING;
        dccrecState = WAIT_PREAMBLE;
        dccrec.bitCount = 0;
        return;
      }
    }
  }
  else {
    // We ignore other halfbits, to avoid interference with orther protocols.
    //
    // Here we may detect the RailCom cutout period (see RCN-217), to test if the command station
    // has activated RailCom.
    // Note that, if the DCC signal is connected to the decoder via a single optocoupler (such as 6N137),
    // the Cutout Start bit (of 26..32us) may or may not be seen by the microcontroller (= this
    // software), depending on the polarity of the DCC signal.
    // Therefore the best moment to start a RailCom timer is likely the piece of code where the
    // Packet End Bit is detected (in sup_isr_assemble_packet.h)
    return;
  }
    
  #include "sup_isr_assemble_packet.h"
}
