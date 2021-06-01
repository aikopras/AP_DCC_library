//******************************************************************************************************
//
// file:      sup_acc.h
// purpose:   Accessory decoder functions to support the DCC library
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#pragma once


class AccMessage {
  public:
    AccMessage();                    // The constructor, to initialise values
    Dcc::CmdType_t analyse(void);    // The standard method to analyse Accessory Commands

  private:
    bool IsMyAddress();              // Function to determine if the command is for this decoder
    unsigned int decoderAddress_old; // To store the previously received decoder addres
    uint8_t byte1_old;               // From the previously received accessory command
    uint8_t byte2_old;               // From the previously received accessory command

};
