//******************************************************************************************************
//
// file:      sup_cv.cpp
// purpose:   Configuration Access Methods
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
// 
//
//******************************************************************************************************
//                             Accessing Configuration Variables
//******************************************************************************************************
// Instructions to access Configuration Variables are defined in:
// - S-9.2.1 - Extended Packet Formats
// - S-9.2.3 - Service Mode for DCC
// - RCN-214 - DCC-Protokoll Konfigurationsbefehle
// - RCN-216 - DCC-Protokoll Programmierumgebung
//
// Programming on the Main (PoM):
// S-9.2.1 describes the packet formats to be used during normal operation (Betriebsmodus).
// Some of these formats describe Programming on the Main (PoM).
// According to S-9.2.1 PoM supports 2 methods to access Configuration Variables (CVs):
// - Short form
// - Long form
// Of these, only the long form will be implemented by this Library
//
// Service Mode (SM):
// S-9.2.3 describes Service Mode Programming (Programmiermodus), which uses a special Programming Track
// According to S-9.2.3 Service Mode supports 4 methods to access Configuration Variables (CVs):
// - Direct Configuration
// - Address-Only
// - Physical Register, and
// - Paged Addressing.
// Of these, only Direct Configuration will be implemented by this Library
//
//
//******************************************************************************************************
//        Service Mode & Programming on the Main (Programmiermodus & Betriebsmodus) - Implemented
//******************************************************************************************************
// Configuration Variable Access Instructions - Long form (SM & POM):
// S-9.2.3 calls this direct mode.
// 1110-CCVV VVVV-VVVV DDDD-DDDD   - POM (Betriebsmodus)
// 0111-CCVV VVVV-VVVV DDDD-DDDD   - SM  (Programmiermodus)

// Addresses:
// <none>                 - Service Mode (Broadcast)                                   => size = 4 bytes
// 0AAA-AAAA              - Loco (7 bit address)                                       => size = 5 bytes
// 11AA-AAAA AAAA-AAAA    - Loco (14 bit address)                                      => size = 6 bytes
// 10AA AAAA 1AAA-1AA0    - Basic Accesory (11 bit address)                            => size = 6 bytes
// 10AA-AAAA 0AAA-0AA1    - Extended Accesory (11 bit address)                         => size = 6 bytes
//
// Instruction type:
// KK = 00 Reserved
// KK = 01 Verify byte
// KK = 11 Write byte
// KK = 10 Bit manipulation
//
// Bit manipulation:
// DDDD-DDDD => 111K-DBBB
// - K=0: Verify bit
// - k=1: write bit
// - D = bit value (0 or 1)
// - BBB: Bit position within the CV
//
//******************************************************************************************************
//                        Programming on the Main (Betriebsmodus) - Not implemented
//******************************************************************************************************
// Configuration Variable Access Instructions - Short form (POM):
// 1111-KKKK DDDD-DDDD
// 1111-KKKK DDDD-DDDD DDDD-DDDD
//
// Addresses:
// 0AAA-AAAA              - Loco (7 bit address)                                  => size = 4 or 5 bytes
// 11AA-AAAA AAAA-AAAA    - Loco (14 bit address)                                 => size = 5 or 6 bytes
//
// The short form allows direct access to a limited set of CVs, like Accelaration and De-acceleration.
// It can only be used in POM-mode (Betriebsmodus) for multi-function (=loco) decoders
// Due to these limitations, this form is not implemented by this library.
//
//******************************************************************************************************
//                           Service Mode (Programmiermodus) - Not implemented
//******************************************************************************************************
// 1) Service Mode Instruction Packets for Address-Only Mode
// 2) Service Mode Instruction Packets for Physical Register Addressing
// 3) Service Mode Instruction Packets for Paged CV Addressing
//
// 0111-CCVV DDDD-DDDD
//
// Addresses:
// <none>                 - Service Mode (Broadcast)                                   => size = 3 bytes
//
// S-9.2.3 defines Service Mode Instruction Packets for Physical Register Addressing
// This mode allows setting the address, Start Voltage, Acceleration, Deceleration/Braking,
// Speed steps etc. Therefore it is primarily useful for multi-function (=loco) decoders.
// The difference between this form and the Long form / Direct mode SM packet can only be determined
// by comparing the length: this form is 3 bytes long (XOR included) and the other is 4 byte long.
//
// S-9.2.3 also defines Service Mode Instruction Packets for Address-Only Mode. Such packets are
// in fact standard Register mode packets for CV1 access. They are described seperately, to allow
// them to be included in earlier versions of conformance clauses.
//
// S-9.2.3 finally defines Service Mode Instruction Packets for Paged CV Addressing. The goal is to
// allow access to all 1024 CVs, without adding an additional CV Address byte to the packet. This is
// implemented by having registers (CVs), which should first be filled with a value representing the
// offset within that page.
//
// According to RCN-214, these forms are outdated and should no longer be used.
// Therefore they are not implemented by this library.
//
//******************************************************************************************************
#include "Arduino.h"
#include "AP_DCC_library.h"
#include "sup_isr.h"
#include "sup_cv.h"

extern DccMessage   dccMessage;         // Class defined in sup_isr.h, instantiated in DCC_Library.cpp
extern CvAccess     cvCmd;              // instantiated in DCC_Library.cpp, used by main sketch


//******************************************************************************************************
//                                    Local class to detect duplicates
//******************************************************************************************************
class Backup {
public:
  Backup();                                   // Constructor, which initializes attributes
  bool identical(void);                       // To check if this is the second CV-Access command
  void copy();                                // To copy the current message to the backup
  uint8_t data[MaxDccSize];                   // The contents of the previous CV-Access message received
  uint8_t size;                               // 3 .. 6, including XOR
  uint8_t count;                              // To distinguish between the second or third etc. command
};

Backup backup;                                // Instantiate object message of class Message

Backup::Backup() {                            // Object constructor, to initialise attributes
  size = 0;
  count = 0;
}


void Backup::copy() {                         // To copy the current message to the backup
  for (uint8_t i=0; i < dccMessage.size; i++)
    backup.data[i] = dccMessage.data[i];
  backup.size = dccMessage.size;
  count = 1;
}


bool Backup::identical() {
  // This method checks if the current (CV-Access) message is identical to the previous.
  // If they are not identical, it will backup the current message, and return with false
  // If they are identical, check if this is the second (and not the third or later)
  // Works for SM as well as PoM messages
  bool result = false;
  // Ensure the size of the current and prevous messages are equal
  if (dccMessage.size == backup.size) {
    result = true;                              // Could be OK. Ensure that each byte is equal
    for (uint8_t i=0; i < dccMessage.size; i++)
      if (dccMessage.data[i] != backup.data[i]) result = false;
    if (result) {                               // Messages are identical
      backup.count ++;                          // Count the number of copies (ignore counter wrap)
      if (backup.count != 2) result = false;    // Third or later copy
    }
    else backup.copy();                         // Message contents are different. Save current message!
  }
  else backup.copy();                           // Message sizes are different. Save current message!
  return result;
}


//******************************************************************************************************
//                                             analyseSM()
//******************************************************************************************************
Dcc::CmdType_t CvMessage::analyseSM(void) {
  // {Preamble} 0111-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE         - Long form. In SM there is no address
  uint8_t byte1 = dccMessage.data[0];
  uint8_t byte2 = dccMessage.data[1];
  uint8_t byte3 = dccMessage.data[2];
  if ((millis() - SmTime) >= SmTimeOut) {                       // Timeout?
    inServiceMode = false;
    backup.size = 0;
    backup.count = 0;
    return (Dcc::Unknown);                                      // We don't know yet what packet this is
  }
  if ((byte1 == 0b00000000) && (byte2 == 0b00000000)) {         // Reset packet in SM?
    SmTime = millis();
    return(Dcc::IgnoreCmd);
  }
  if (byte1 == 0b11111111) {                                    // Idle packet in SM?
    SmTime = millis();
    return(Dcc::IgnoreCmd);
  }
  if ((byte1 & 0b11110000) == 0b01110000) {                     // SM packet?
    SmTime = millis();                                          // Reopen TimeInterval for next message
    if (dccMessage.size == 4) {                                 // Long Form (Direct mode)?
      if (backup.identical()) {                                 // Is this the second SM message?
        switch ((byte1 & 0b00001100) >> 2) {                    // CC bits
          case 0: cvCmd.operation = CvAccess::reserved; break;
          case 1: cvCmd.operation = CvAccess::verifyByte; break;
          case 2: cvCmd.operation = CvAccess::bitManipulation; break;
          case 3: cvCmd.operation = CvAccess::writeByte; break;
        }
        cvCmd.number = ((byte1 & 0b00000011) << 8) + byte2 + 1; // Start with CV1 (= 00 0000-0000)
        cvCmd.value = byte3;
        if (cvCmd.operation == CvAccess::bitManipulation) {
          // Bit munipulation data is contained in byte3
          // 111K-DBBB
          // K = 0 verify, K = 1 write
          // D = value
          // BBB = bitposition
          cvCmd.writecmd    = (byte3 & 0b00010000) >> 4;
          cvCmd.bitvalue    = (byte3 & 0b00001000) >> 3;
          cvCmd.bitposition = byte3 & 0b00000111;
        }
        return(Dcc::SmCmd);
      }
    }
    return(Dcc::IgnoreCmd);                                     // It is a SM message, but not the second
  }
  return(Dcc::IgnoreCmd);                                       // We should never reach here ...
}


//******************************************************************************************************
//                                             analysePoM()
//******************************************************************************************************
Dcc::CmdType_t CvMessage::analysePoM(void) {
  // Note: we only implement the long form. The short form is NOT implemented
  // 0AAA-AAAA           1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE      Loco (7 bit address)
  // 11AA-AAAA AAAA-AAAA 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE      Loco (14 bit address)
  // 10AA AAAA 1AAA-1AA0 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE      Basic Accesory (11 bit address)
  // 10AA-AAAA 0AAA-0AA1 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE      Extended Accesory (11 bit address)
  //
  // Check if this is the second PoM message
  uint8_t offset = 1;                                         // 1 address byte for loco 7 bit address
  if (dccMessage.size == 6) offset = 2;                       // 2 address bytes for others
  if (backup.identical()) {                                   // Is this the second PoM message?
    uint8_t byte1 = dccMessage.data[offset];
    uint8_t byte2 = dccMessage.data[offset + 1];
    uint8_t byte3 = dccMessage.data[offset + 2];
    switch ((byte1 & 0b00001100) >> 2) {                      // CC bits
      case 0: cvCmd.operation = CvAccess::reserved; break;
      case 1: cvCmd.operation = CvAccess::verifyByte; break;
      case 2: cvCmd.operation = CvAccess::bitManipulation; break;
      case 3: cvCmd.operation = CvAccess::writeByte; break;
    }
    cvCmd.number = ((byte1 & 0b00000011) << 8) + byte2 + 1;   // Start with CV1 (= 00 0000-0000)
    cvCmd.value = byte3;
    if (cvCmd.operation == CvAccess::bitManipulation) {
      // Bit munipulation data is contained in byte3
      // 111K-DBBB
      // K = 0 verify, K = 1 write
      // D = value
      // BBB = bitposition
      cvCmd.writecmd    = (byte3 & 0b00010000) >> 4;
      cvCmd.bitvalue    = (byte3 & 0b00001000) >> 3;
      cvCmd.bitposition = byte3 & 0b00000111;
    }    
    return(Dcc::MyPomCmd);
  }
  return(Dcc::IgnoreCmd);                                     // It is a PoM message, but not the second
}
