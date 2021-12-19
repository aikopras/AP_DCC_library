//******************************************************************************************************
//
// file:      sup_loco.h
// purpose:   Loco decoder functions to support the DCC library
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#pragma once


class LocoMessage {
  public:
    LocoMessage();                      // The constructor, which calls reset_speed()
    void reset_speed(void);             // Reset speed to 0, direction to forward and functions to off
    Dcc::CmdType_t analyse(void);       // The standard method to analyse Loco Commands
  
    // Address range. Initialised by Loco::setMyAddress(... first, ... last = 65535);
    unsigned int myLocoAddressFirst;    // First loco address this decoder listens to
    unsigned int myLocoAddressLast;     // Last loco address. Usually same as first loco address

  private:
    void DetermineSpeedAndDirection();
    bool IsMyAddress();
};
