//******************************************************************************************************
//
// file:      AP_DCC_library.cpp
// purpose:   DCC library for ATmega AVRs (16, 328, 2560, ...)
// author:    Aiko Pras
// version:   2021-06-01 V1.0.2 ap initial version
//
// history:   This is a further development of the OpenDecoder 2 software, as developed by W. Kufer.
//            It has been rewritten, such that it can be used for Arduino (Atmel AVR) environments.
//            From a functional point of view, this code is comparable to (a subset of) the NMRA-DCC
//            library. However,it is believed that this code is easier to adapt and modify to personal needs.
//            This Library has also been tested on ATMega16 and 2560 processors.
//            Like the OpenDecoder 2 Software and version 1.2 of the NMRA-DCC library, this code
//            uses a separate Timer (Timer2) to decode the DCC signal. However, as opposed to some
//            other code bases, the DCC input (interrupt pin) can be freely chosen. In contrast to
//            to the NMRA-DCC version 2 library, this code doesn't use callback functions, in an
//            attempt to improve the readability of the main loop for cases where the action to be
//            taken should not depend on the trigger source. For example, the code for setting a switch
//            should be the same, irrespective whether the trigger is a DCC-message or a button push.
//            To improve readability, mainainability and extendibility, many comments were included
//            and an attempt is made to structure the code more clearly.
// reference: This code follows the NMRA DCC standard S9.2 S9.2.1 and S9.2.3 as well as RCN211-RCN214
//            (in German, by railcommunity.org), which include more detailed descriptions as well as
//            the differences in accesory decoder addresses.
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#include "Arduino.h"
#include "AP_DCC_library.h"
#include "sup_isr.h"
#include "sup_acc.h"
#include "sup_loco.h"
#include "sup_cv.h"

// The following objects must be visible for the main sketch. These objects are declared here,
// thus the main sketch doesn't need to bother about declaring these necessary objects itself.
// In addition, since these objects are defined here, there can only be a single instantiation
// of the Dcc class, and thus a single DCC interface per decoder.
Dcc           dcc;              // Interface to the main sketch
Accessory     accCmd;           // Interface to the main sketch for accessory commands
Loco          locoCmd;          // Interface to the main sketch for loco commands
CvAccess      cvCmd;            // Interface to the main sketch for CV commands (POM and SM)

// The following objects should NOT be used by the main sketch, but are instead used by this
// c++ file as interface to the various support files.
DccMessage    dccMessage;       // Interface to sup_isr
AccMessage    accMessage;       // Interface to sup_acc
LocoMessage   locoMessage;      // Interface to sup_loco
CvMessage     cvMessage;        // Interface to sup_cv


//******************************************************************************************************
//                    The methods that belong to the Dcc object are implemented here
//         Note that the Accesory and Loco objects do not include methods, but only attributes
//******************************************************************************************************
void Dcc::attach(uint8_t dccPin, uint8_t ackPin) {
  errorXOR = 0;
  dccMessage.attach(dccPin, ackPin);
  _ackPin = ackPin;
}


void Dcc::detach(void) {
  dccMessage.detach();
}


Dcc::CmdType_t Dcc::analyze_broadcast_message(void) {
  // The following cases should be considered:
  // 1) Reset packet: this also indicates the possible start of Service Mode programming
  // 2) Emergency stop for all loco's
  // 3) Normal stop for all loco's
  // => In my Lenz LZV100, XPresNet V3.6 and TrainController environment I only observed case 1.
  // Case 1: Reset packet (Rucksetz)
  // {preamble} 0 00000000 0 00000000 0 EEEEEEEE 1
  if (dccMessage.data[1] == 0) {
    // A three byte packet, where all eight bits within each of the three bytes contains
    // the value of "0", is defined as a Digital Decoder Reset Packet.
    // After reception of a reset packet, the decoder should check if the next packet will be a
    // service mode instruction packet. Such SM packet should be received within 20 milliseconds
    cvMessage.inServiceMode = true;
    cvMessage.SmTime = millis();
    // When a Digital Decoder receives a Reset Packet, it shall erase all volatile memory
    // (including any speed and direction data), and return to its normal power-up state.
    // If the Digital Decoder is operating a locomotive at a non-zero speed when it receives a
    // Digital Decoder Reset, it shall bring the locomotive to an immediate stop.
    locoMessage.reset_speed();
    return(ResetCmd);
  }
  // Case 2: Emergency stop for all loco's.
  // {preamble} 0 00000000 0 01DC0001 0 EEEEEEEE 1
  if ((dccMessage.data[1] & 0b01000001) == 0b0100001) {
    locoMessage.reset_speed();
    return(MyEmergencyStopCmd);
  }
  // Case 3: Normal stop for all loco's.
  // {preamble} 0 00000000 0 01DC0000 0 EEEEEEEE 1
  if ((dccMessage.data[1] & 0b01000001) == 0b0100000) {
    locoMessage.reset_speed();
    return(MyLocoSpeedCmd);
  }
  return(IgnoreCmd);
};


bool Dcc::input(void) {
  bool packet_received = false;
  if (dccMessage.isReady) {
    uint8_t myxor = 0;
    cmdType = Unknown;
    // Check if the DCC packet has a correct checksum.
    for (uint8_t i=0; i<dccMessage.size; i++) myxor = myxor ^ dccMessage.data[i];
    if (myxor) {
      errorXOR ++;
      cmdType = IgnoreCmd;
    }
    else {
      // Check if we are in service mode (programming on the programming track)
      if (cvMessage.inServiceMode) cmdType = cvMessage.analyseSM();
      // Returned cmdType may be SmCmd, IgnoreCmd or remain Unknown
      if (cmdType == Unknown) {
        // We are decoding a normal DCC packet - See for steps RP 9.2.1
        // Leave Service Mode. We may enter again after a broadcast reset
        cvMessage.inServiceMode = false;
        uint8_t firstbyte = dccMessage.data[0];
        if      (firstbyte == 0b00000000) cmdType = analyze_broadcast_message();  // Reset or (Emergency) Stop
        else if (firstbyte <= 0b01111111) cmdType = locoMessage.analyse();        // 7-bit address
        else if (firstbyte <= 0b10111111) cmdType = accMessage.analyse();         // Accessory command
        else if (firstbyte <= 0b11100111) cmdType = locoMessage.analyse();        // 14-bit address
        else if (firstbyte <= 0b11111110) cmdType = IgnoreCmd;                    // Reserved in DCC for Future Use
        else cmdType = IgnoreCmd;                                                 // 255: Idle Packet
      }
    }
    // Clear the dccMessage flag
    noInterrupts();
    dccMessage.isReady = 0;
    interrupts();
    packet_received = true;
  }
  return packet_received;
}


//******************************************************************************************************
//                                Create a DCC basic acknowledge signal
//******************************************************************************************************
// According to NMRA RP 9.2.3, Basic acknowledgment is defined by the Digital Decoder providing
// an increased load (positive-delta) on the programming track of at least 60 mA for 6 ms +/-1 ms.
// To allow complete testing of the library, the acknowledgement function must be included.
void Dcc::sendAck(void) {
  if (_ackPin == 255) return;     // ackPin was not specified by the main sketch, so return
  digitalWrite(_ackPin, HIGH);
  // Program does busy wait for 6ms. Interrupts will still be served
  // Busy wait is needed, since at some places in the code RESET will follow immediately
  delay(6);
  digitalWrite(_ackPin, LOW);
}


//******************************************************************************************************
//                                      The Accessory Class
//******************************************************************************************************
void Accessory::setMyAddress(unsigned int first, unsigned int last) {
  // This method should be called in setup() of the main sketch
  // If called with a single parameter, that parameter will be the decoder's Accessory address.
  // In that case "last" has the default value 65535 (maxint)
  // If called with two parameters, the decoder listens to all addresses between first and last
  accMessage.myAccAddrFirst = first;
  if (last == 65535) accMessage.myAccAddrLast = first;
    else accMessage.myAccAddrLast = last;
}


//******************************************************************************************************
//                                            The Loco Class
//******************************************************************************************************
void Loco::setMyAddress(unsigned int first, unsigned int last) {
  // This method should be called in setup() of the main sketch
  // If called with a single parameter, that parameter will be the decoder's Loco address.
  // In that case "last" has the default value 65535 (maxint)
  // If called with two parameters, the decoder listens to all addresses between first and last
  locoMessage.myLocoAddressFirst = first;
  if (last == 65535) locoMessage.myLocoAddressLast = first;
    else locoMessage.myLocoAddressLast = last;
}


//******************************************************************************************************
//                         Configuration Variable (CV) Access Commands - bit manipulation
//******************************************************************************************************
uint8_t CvAccess::writeBit(uint8_t data) {
  if (bitvalue == 1) return(bitSet(data, bitposition));
    else return(bitClear(data, bitposition));
}


bool CvAccess::verifyBit(uint8_t data) {
  if (bitRead(data, bitposition) == bitvalue) return true;
  return false;
}
