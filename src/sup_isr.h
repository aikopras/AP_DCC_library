//******************************************************************************************************
//
// file:      sup_isr.h
// purpose:   DCC receiving code for ATmega(X) processors
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//            2021-07-27 V1.1.0 ap Modifications to support MegaX processors (timer TCB)
//            2021-09-02 V1.2.0 ap Restructure, to better support different ATmega processors
//                                 DCC signal detection is significantly improved if used with
//                                 an ATmegaX (ATmega4808, ATmega4809, AVR DA, AVR DB, ...) processor
//
// history:   This is a further development of the OpenDecoder 2 software, as developed by W. Kufer.
//            It implements the DCC receiver code, in particular the layer 1 (bit detection) and
//            layer 2 (packet construction) part. The code has been adapted for Arduino by A. Pras.
//
// Used hardware resources:
//  - Pin:    The DCC input signal (for traditional AtMega processors this must be an Interrupt pin)
//  - Timer:  To read the DCC input signal after 77us to determine if we receive a DCC 0 or 1
//            For traditional ATMega processors this must be Timer 2; for ATMegaX processors it
//            can be any available TCB timer.
// - Event:   For AtMegaX processors: an Event channel
// See the processor / board specific header file for detailed requirements
//
//******************************************************************************************************
#pragma once
#define MaxDccSize 6                              // DCC messages can have a length upto this value

// The DCC message that has just been received.
class DccMessage {
  public:
    volatile uint8_t isReady;                     // Flag that DCC message has been received and can be decoded
    volatile uint8_t size;                        // 3 .. 6, including XOR
    volatile uint8_t data[MaxDccSize];            // The contents of the last dcc message received

    void attach(uint8_t dccPin, uint8_t ackPin);  // Initialises the timer and DCC input Interrupt Service Routines
    void detach(void);                            // Stops the timer and DCC input ISRs, for example before a restart

  private:
    uint8_t _dccPin;                              // Here we store a local copy of the DCC input pin
    void initTcb(void);                           // Specific for the MegaCoreX/DxCore variant
    void initEventSystem(uint8_t dccPin);         // Specific for the MegaCoreX/DxCore variant
};



// For certain types of decoders (such as occupancy detectors) the voltage over certain resistors
// must be measured to determine if a track is occupied or not. There will only be a non-zero
// voltage over these resistors if the DCC signal is high. Therefore the AD conversion must start
// at such moment. The ISR routines can determine when such moments are there, thus the start of
// AD conversions is started from here. Note that "VOLTAGE_DETECTION" must be defined to activate
// the related code.
#if defined(VOLTAGE_DETECTION)
class AdcStart {
  public: volatile uint8_t newRequest;   // Flag to signal new ADC conversion should start
};
#endif
