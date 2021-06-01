//******************************************************************************************************
//
// file:      support_isr.h
// purpose:   DCC receiving code for ATmega AVRs (16, 328, 2560, ...) using the Arduino libraries
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//
// history:   This is a further development of the OpenDecoder 2 software, as developed by W. Kufer.
//            It implements the DCC receiver code, in particular the layer 1 (bit detection) and
//            layer 2 (packet construction) part. The code has been adapted for Arduino by A. Pras.
//            Note that this code his simularities with version 1 of the NmraDcc V1.2.1 library. 
//            An important difference is that readability of this code should be clearer.
//
// Used hardware resources:
//  - INTx:   The DCC input signal (any available hardware INT)
//  - Timer2: To read the DCC input signal after 77us to determine if we receive a DCC 0 or 1
//
// The Arduino and the (standard) mightycore boards support the following Interrupts and pins:
//  Interrupt   Port   Pin   Where 
//    INT0      PD2      2   All standard Arduino boards
//    INT1      PD3      3   All standard Arduino boards
//    INT0      PD0     21   MEGA
//    INT1      PD1     20   MEGA
//    INT2      PD2     19   MEGA
//    INT3      PD3     18   MEGA
//    INT4      PE4      2   MEGA
//    INT5      PE5      3   MEGA
//    INT0      PD2     10   Mightycore - ATmega 8535/16/32/164/324/644/1284
//    INT1      PD3     11   Mightycore - ATmega 8535/16/32/164/324/644/1284
//    INT2      PB2      2   Mightycore - ATmega 8535/16/32/164/324/644/1284
//
// Note that the user shuld define "MaxDccSize"
//
//******************************************************************************************************
#pragma once

// The DCC message that has just been received.
class DccMessage {
  public:
    volatile uint8_t isReady;                     // Flag that DCC message has been received and can be decoded

    volatile uint8_t data[MaxDccSize];            // The contents of the last dcc message received
    volatile uint8_t size;                        // 3 .. 6, including XOR     

    void begin(uint8_t dccPin, uint8_t ackPin);   // Initialises the timer and DCC input Interrupt Service Routines
    void end(void);                               // Stops the timer and DCC input ISRs, for example before a restart

  private:
    uint8_t _dccPin;                              // Here we store a local copy of the DCC input pin
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
