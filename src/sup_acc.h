//******************************************************************************************************
//
// file:      sup_acc.h
// purpose:   Accessory decoder functions to support the DCC library
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//            2025-08-10 V1.0.3 ap Added functions and booleans for CV29 bit 6 (output addressing)
//                                 and bit 3 (Railcom)
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#pragma once


class AccMessage {
  public:
    AccMessage();                     // The constructor, to initialise values
    Dcc::CmdType_t analyse(void);     // The standard method to analyse Accessory Commands

    // Address range. Initialised by Accessory::setMyAddress(... first, ... last = 65535);
    unsigned int myAccAddrFirst;      // 0..510  - First accessory decoder address this decoder will listen too
    unsigned int myAccAddrLast;       // 0..510  - Last accessory decoder address this decoder will listen too

    // Decoder type (CV29). 
    void setOutputAddressing(void);   // the decoder uses output addressing (CV29 bit 6 = 1)
    void clearOutputAddressing(void); // the decoder uses decoder addressing (CV29 bit 6 = 0, default)
    void startRailcom(void);          // the decoder implements / uses railcom (CV29 bit 3 = 1)
    void stopRailcom(void);           // railcom is not implemented / used (CV29 bit 3 = 0, default)

  private:
    bool IsMyAddress();               // Function to determine if the command is for this decoder
    bool outputAddressing;            // If true, the decoder uses output (instead of decoder) addressing
    bool railcom;                     // If true, the decoder implements railcom
    unsigned int decoderAddress_old;  // To store the previously received decoder addres
    uint8_t device_old;               // as well as the device
    uint8_t byte1_old;                // From the previously received accessory command
    uint8_t byte2_old;                // From the previously received accessory command

};
