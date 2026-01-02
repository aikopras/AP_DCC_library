//******************************************************************************************************
//
// file:     STM32.h
// purpose:  DCC receive code for STM32 processors
// author:   Aiko Pras
// version:  2026-01-02 V1.0.0 ap   Initial version
//
// Result:   1. The received message is collected in the struct "dccrec.tempMessage".
//           2. After receiving a complete message, data is copied to "dccMessage.data".
//           3. The flag "dccMessage.isReady" is set.
//
// Used hard and software resources:
// TIM4_CH1 on PB6 is chosen as a safe default for most STM32 MCUs.
// In case this timer / pin is not available or already used by other pieces of software,
// the user may select an alternative Timer, channel and pin mumber via the #defines below
// Use the stm32duino board definitions, available from
// https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
//
// Howto: This part of the library uses a general-purpose hardware timer (TIMx) on STM32 MCUs
// in Input Capture mode to decode DCC signals. The selected DCC pin must be connected to
// a timer channel capable of input capture. The timer is configured to capture on both edges.
//
// When a DCC signal transition occurs, the timer records the current counter value in the
// capture register (CCRn). By subtracting the previous captured value, we determine the
// time elapsed since the last transition, which encodes the DCC bit (half-bit width).
//
// The captured value is read inside an interrupt service routine (ISR). Each DCC bit consists
// of two half-bits, so the ISR is called twice per DCC bit. By toggling the edge detection
// after each capture, we ensure both rising and falling edges are measured.
// The measured times are then interpreted according to the RCN-210 standard to distinguish between
// zero and one bits.
//
// Using the timer at 4 MHz (0.25 µs per tick) provides a good compromise between timing resolution,
// portability, and the maximum time between two transitions (up to 12 ms for zero-bit stretching).
// This also ensures that 16-bit timers do not overflow.
//
// It is important to note that, as opposed to the other processor variants, for the STM32 we do
// not provide user configurable pins for the DCC input. The "dccPin" in the attach() function is
// thus ignored. The reason for this is that, due to the myriad of STM32 families and processors,
// it would be hard to write (and test!) such mapping function. Therefore, in this release, we decided
// to have #defines for the Timer, channel and DCC pin.  These #defines may, if needed,  by be changed
// the user of this library.
//
// Note: Railcom feedback has not been implemented (yet?).
//
//******************************************************************************************************
#include <Arduino.h>
#include <HardwareTimer.h>
#include "../sup_isr.h"

//******************************************************************************************************
// 1. Defines that may be changed to accomodate certain hardware
//******************************************************************************************************
// TIM4_CH1 on PB6 is chosen as a default for most STM32 MCUs.
// This default works on many F1, F4, L4, G0 families, but may not be available on all packages.
// Check the MCU datasheet and STM32 core pinmap to confirm that the selected pin supports
// TIMx_CHy input capture for your board.
// If the default pin would not be available, the following settings seem good alternatieves for TIM4:
// PB7 and channel 2 / PB8 and channel 3 / PB9 and channel 4
// If TIM4 would not be available, TIM3 may be selected with the following settings:
// PA6 and channel 1 / PA7 and channel 2 / PB0 and channel 3 / PB1 and channel 4
// Alternatively, TIM2 / TIM 5 may work as well. In that case you must change the datatypes for
// delta and ccr to uint32_t at the start of the Timer ISR
#define DCC_TIMER        TIM4      // TIM4 is available on most processors
#define TIMER_CHANNEL    1         // Channel 1 is the default
#define DCC_PIN          PB6       // PB6 connects to Channel 1

// In addition, the input signal may be filtered to ignore very short spikes or glitches
// in the range of tens of nanoseconds up to a few microseconds.
// We use 10 as default value. Depending on the STM32 family and timer clock, this
// typically filters spikes of roughly 0.5–0.7 µs.
// The maximum possible value is 15, which can filter spikes of about 2–3 µs.
// Note: The effective filter duration depends on the timer input clock (fDTS) and
// is independent of the counter prescaler.
// For longer glitches, additional hardware filtering (RC) may be necessary.
#define FILTER_VALUE     10


//******************************************************************************************************
// 2. Declaration of external objects
//******************************************************************************************************
// The dccMessage contains the "raw" DCC packet, as received by the TCB ISR in this file
// It is instantiated in, and used by, DCC_Library.cpp
extern DccMessage dccMessage;


//******************************************************************************************************
// 3. Defines, definitions and instantiation of local types and variables
//******************************************************************************************************
// Values for half bits from RCN 210, section 5: http://normen.railcommunity.de/RCN-210.pdf
#define ONE_BIT_MIN     (52 * 4)
#define ONE_BIT_MAX     (64 * 4)
#define ZERO_BIT_MIN    (90 * 4)
#define ZERO_BIT_MAX    (119 * 4)

// #define ZERO_BIT_MAX    (12000 * 4)
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

static HardwareTimer *MyTim;                  // Pointer to the timer being used
static volatile uint32_t CcrPrevious;         // Holds the previous captured counter value (CCR snapshot)

volatile uint8_t dccrecState;                 // State machine - interface with include file
volatile uint8_t tempByte;                    // interface with include file
volatile uint8_t dccHalfBit;                  // local variable, used in ISR


//******************************************************************************************************
// 4. Support routine for filtering spikes
//******************************************************************************************************
// Helper function to configure the input capture filter (ICxF bits in CCMRx register).
// Parameters: channel (1-4), value (0-15, where 0=no filter, higher = more filtering)
static inline void setICFilter(uint8_t channel, uint8_t value) {
  uint32_t mask, pos;
  if      (channel == 1) { mask = TIM_CCMR1_IC1F; pos = TIM_CCMR1_IC1F_Pos; }
  else if (channel == 2) { mask = TIM_CCMR1_IC2F; pos = TIM_CCMR1_IC2F_Pos; }
  else if (channel == 3) { mask = TIM_CCMR2_IC3F; pos = TIM_CCMR2_IC3F_Pos; }
  else                   { mask = TIM_CCMR2_IC4F; pos = TIM_CCMR2_IC4F_Pos; }
  // Determine if we operate on the CCMR1 or CCMR2 register
  volatile uint32_t *ccmr = (channel <= 2) ? &DCC_TIMER->CCMR1 : &DCC_TIMER->CCMR2;
  *ccmr = (*ccmr & ~mask) | ((value & 0x0F) << pos);
}


//******************************************************************************************************
// 5. attach() / detach()
//******************************************************************************************************
void DccTimerCallBack(void);                  // forward declaration to avoid compiler errors

void DccMessage::attach(uint8_t dccPin, uint8_t ackPin) {
  // Initialize the local variables
  _dccPin = dccPin;
  tempByte = 0;
  dccrecState = WAIT_PREAMBLE;
  dccrec.bitCount = 0;
  // initialise the global variables (the DccMessage attributes)
  dccMessage.size = 0;
  dccMessage.isReady = 0;
  // Initialize the timer peripheral
  MyTim = new HardwareTimer(DCC_TIMER);                // instatiate a new timer object
  uint32_t timerClock = MyTim->getTimerClkFreq();      // What is the clock frequency?
  uint32_t factor = timerClock / 4000000;              // What prescaler factor do we need for 0,25 us
  if (factor == 0) factor = 1;                         // For clocks < 4 Mhz
  MyTim->setPrescaleFactor(factor);                    // Set the prescaler
  MyTim->setMode(TIMER_CHANNEL, TIMER_INPUT_CAPTURE_BOTHEDGE, DCC_PIN);
  MyTim->attachInterrupt(TIMER_CHANNEL, DccTimerCallBack);
  setICFilter(TIMER_CHANNEL, FILTER_VALUE);            // filter short spikes from ns to a few us
  MyTim->resume();                                     // start the timer
  CcrPrevious = 0;                                     // Despite reset, the first capture value is useless
  // Initialise the DCC Acknowledgement port, which is needed in Service Mode
  // If the main sketch doesn't specify this pin, the value 255 is provided as
  // (invalid) default.
  if (ackPin < 255) pinMode(ackPin, OUTPUT);
}


void DccMessage::detach(void) {
   // Stop the timer and detach ISR
   if (MyTim) {
    MyTim->detachInterrupt(TIMER_CHANNEL);             // detach the ISR
    MyTim->pause();                                    // stop the timer
    delete MyTim;                                      // free the allocated memory
    MyTim = nullptr;                                   // avoid dangling pointer
  }
}


//******************************************************************************************************
// 6. The Timer ISR, which implements the DCC Receive Routine
//******************************************************************************************************
// Execution of this DCC Receive code typically takes between 2 and 5 microseconds.
void DccTimerCallBack(void) {
  uint16_t delta;                        // Delta holds the time since the previous interrupt
  uint16_t ccr;                          // ccr holds the current value in the timer's CCR register
  //uint32_t delta;                      // change if a 32 bit timer is used, such as TIM2 or TIM 5
  //uint32_t ccr;                        // change if a 32 bit timer is used, such as TIM2 or TIM 5
  #if TIMER_CHANNEL == 1
    ccr = DCC_TIMER->CCR1;
  #elif TIMER_CHANNEL == 2
    ccr = DCC_TIMER->CCR2;
  #elif TIMER_CHANNEL == 3
    ccr = DCC_TIMER->CCR3;
  #elif TIMER_CHANNEL == 4
    ccr = DCC_TIMER->CCR4;
  #endif
  delta = ccr - CcrPrevious;
  CcrPrevious = ccr;

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
