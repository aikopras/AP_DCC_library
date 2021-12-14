//******************************************************************************************************
//
// file:      sup_cv.h
// purpose:   Configuration Access Methods
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#pragma once


class CvMessage {
  public:
    Dcc::CmdType_t analyseSM(void);      // Analyse Service Mode CV access Commands
    Dcc::CmdType_t analysePoM(void);     // Analyse Programming on the Main CV access Commands

    bool inServiceMode;                  // Flag is set after a broadcast reset is received
  
    unsigned long SmTime;                // New SM packets should arrive within a certain time
    const unsigned long SmTimeOut = 40;  // The timeout for SM packets is 20ms. We allow some extra ms
};
