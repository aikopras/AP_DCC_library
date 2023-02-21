//******************************************************************************************************
//
// file:      sup_isr.cpp
// purpose:   DCC receiving code for ATmega(X) processors
// author:    Aiko Pras
// version:   2021-05-15 V1.0.0 ap Initial version
//
// Purpose: Select the best DCC capture code for a specific processor and board.
//
// This version of the software runs only on ATmega processors, and not on (for example) the ESP32
//
// For traditional ATmega processors, such as used on the Arduino UNO and MEGA boards, we use an
// approach where a change of the DCC input signal triggers an ISR. The ISR sets a timer to 77us,
// and once the timer fires the DCC input pin's value is read to determine if we have a 0 or a 1.
// See sup_isr_Mega.h for details
//
// For the newer ATMegaX processors, such as the ATmega4808, 4809, AVR128DA and AVR128DB,
// we try to exploit the new Event system peripheral, which can offload  certain task to hardware
// and thereby considerably improves the quality of the captured DCC input signal. Timing has become
// very precise, which means that the code should be able to run in multi-protocol environments and
// conform to the timing requirements as defined by the Rail Community (RCN-210)/
// To exploit these, the MegaCoreX or DxCore boards should be installed by the ARduino IDE:
// https://github.com/MCUdude/MegaCoreX
// https://github.com/SpenceKonde/DxCore
// See sup_isr_MegaCoreX_DxCore.h for details
//
// If the ATMegaX 4809 processor is used on an Arduino Nano Every, but instead of the MegaCoreX board
// the 'standard' Arduino megaAVR board has been selected in the Arduino IDE, a similar decoding
// approach is used as with the traditional ATmega processors: a change of the DCC input signal
// triggers an ISR; the ISR sets a timer and once the timer fires the DCC input pin's value is read.
// Note that performance of this approach on new processors gives considerably more overhead than on
// the traditional processors. Therefore it is strongly adviced to switch to a MegaCoreX board.
//
//******************************************************************************************************
#include <Arduino.h>

#if defined(__AVR_MEGA__)
  // Only runs on Atmega (AVR) processors
  #if defined(__AVR_XMEGA__)
    // These are the new megaAVR 0, AVR DA, DB, DD (EA) and and MegaTiny processors
    #if defined (ARDUINO_AVR_NANO_EVERY)
      // This is the Arduino Nano Every, using an Arduino megaAVR board
      #include "sup_isr_Nano_Every.h"
      #elif defined(MEGACOREX)
        // megaAVR 0 but not NanoEvery
        #include "sup_isr_MegaCoreX_DxCore.h"
      #elif defined(_AVR_FAMILY)
        //AVR DA DB DD (EA) or MegaTiny CPU Core
        #include "sup_isr_MegaCoreX_DxCore.h"
    #endif   
  #else
    // These are the traditional ATmega processors
    // Examples are ATmega16A, 328 and 2560
    #include "sup_isr_Mega.h"
  #endif
#else
  #if defined(MIGHTYCORE)
    // The 8535 isn't defined as __AVR_MEGA__, but works
    #include "sup_isr_Mega.h"
  #endif
#endif
