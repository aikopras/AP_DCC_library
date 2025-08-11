//******************************************************************************************************
//
// file:      sup_isr.cpp
// purpose:   DCC receiving code for ATmega(X) processors
// author:    Aiko Pras
// version:   2021-05-15 V1.0.0 ap Initial version
//            2025-06-09 V2.0.0 ap Restructured and a Generic driver was added
//
// Purpose: Select the best DCC capture code for a specific processor and board.
//
// To enable this library to run on various processors and boards, different "drivers" have been written.
// With version 2 of this library, these "drivers" were moved to a separate directory, called "variants".
// For a description, see ../extras/Boards_Supported.md
//
//
//******************************************************************************************************
#include <Arduino.h>

#if defined(__AVR_MEGA__) && defined(TCNT2)
  // Traditional ATMega processors, that support TIMER2. Examples: UNO, NANO and MEGA
  #include "variants/Mega.h"
#elif defined(MIGHTYCORE)
  // The ATMega 8535 isn't defined as __AVR_MEGA__, but works as well
  #include "variants/Mega.h"
#elif defined(__AVR_DA__) || defined(__AVR_DB__) || defined(__AVR_DD__) || defined(__AVR_EA__) || defined(__AVR_EB__)
  // DxCore, all versions
  #include "variants/MegaCoreX_DxCore.h"
#elif defined(__AVR_TINY_0__) || defined(__AVR_TINY_1__) || defined(__AVR_TINY_2__)
  // MegaTinyCore (ATtiny serie 0, 1 and 2)
  #include "variants/MegaCoreX_DxCore.h"
#elif defined(MEGACOREX)
  // Arduino Nano Every (4809 and similar), using the MEGACOREX board (recommanded)
  #include "variants/MegaCoreX_DxCore.h"
#elif defined (ARDUINO_AVR_NANO_EVERY)
  // Arduino Nano Every (4809 and similar), using an Arduino megaAVR board (not recommended)
  #include "variants/Nano_Every.h"
#elif defined (ARDUINO_ARCH_ESP32)
  // ESP32 processors
  #include "variants/ESP32.h"
#else
  // This driver should work for all possible boards and processors, although sometimes sub-optimal 
  #include "variants/Generic.h"
#endif
