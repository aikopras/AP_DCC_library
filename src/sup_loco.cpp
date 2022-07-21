//******************************************************************************************************
//
// file:      sup_loco.h
// purpose:   Loco decoder functions to support the DCC library
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//            2022-07-21 V1.0.3 ap trainsMoving flag was removed, since we have SomeLocoMovesFlag
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#include "Arduino.h"
#include "AP_DCC_library.h"
#include "sup_isr.h"
#include "sup_loco.h"
#include "sup_acc.h"
#include "sup_cv.h"

// Declaration of external objects
extern Accessory    acc;                // instantiated in DCC_Library.cpp, used by main sketch
extern CvAccess     cvCmd;              // instantiated in DCC_Library.cpp, used by main sketch
extern Loco         locoCmd;            // instantiated in DCC_Library.cpp, used by main sketch
extern DccMessage   dccMessage;         // instantiated in, and used by, DCC_Library.cpp
extern LocoMessage  locoMessage;        // instantiated in, and used by, DCC_Library.cpp
extern CvMessage    cvMessage;          // Interface to sup_cv


//******************************************************************************************************
// Constructor for the LocoMessage class
LocoMessage::LocoMessage() {
  locoCmd.address = 65535;              // No adddress received yet
  locoCmd.longAddress = false;          // We start with a 7-bit address
  locoCmd.emergencyStop = false;        // Clear flag
  // Ensure that, if not initialised, no messages matches my address
  myLocoAddressFirst = 65535;
  myLocoAddressLast  = 65535;
  // Set speed to zero, direction to Forward and switch all functions off
  reset_speed();
}


void LocoMessage::reset_speed(void) {
  // reset_speed is called by the constructor at startup, but also after it receives a Reset packet
  // Intialise all speed and direction attributes
  locoCmd.speed = 0;
//  locoCmd.direction = Loco::Forward;
  locoCmd.forward = true;
  locoCmd.F0F4 = 0;
  locoCmd.F5F8 = 0;
  locoCmd.F9F12 = 0;
  locoCmd.F13F20 = 0;
  locoCmd.F21F28 = 0;
}


bool LocoMessage::IsMyAddress() {
  // The broadcast address for multi function (loco) decoders is 0
  // This adres is already handled by Dcc::analyze_broadcast_message, and therefore doesn't have
  // to be considered here.
  // const unsigned int broadcast_address = 0;
  return ((locoCmd.address >= myLocoAddressFirst) && (locoCmd.address <= myLocoAddressLast));
}


//******************************************************************************************************
// Basic Packets (7Bit addresses)
//    byte:        0          1
// {preamble} 0 0AAAAAAA 0 01DSSSSS 0 EEEEEEEE 1
// A = Address
// D = direction: 1 = forward
// S = Speed (28 steps)
// E = Error check
//
// Extended packets (14 bit addresses)
//    byte:        0          1          2          3          4
// {preamble} 0 11AAAAAA 0 AAAAAAAA 0 CCCDDDDD 0 EEEEEEEE 1
// {preamble} 0 11AAAAAA 0 AAAAAAAA 0 CCCDDDDD 0 DDDDDDDD 0 EEEEEEEE 1
// {preamble} 0 11AAAAAA 0 AAAAAAAA 0 CCCDDDDD 0 DDDDDDDD 0 DDDDDDDD 0 EEEEEEEE 1
//
// The CCC (or 01D) bits determine the kind of Loco command:
//   000 Decoder and Consist Control Instruction
//   001 Advanced Operation Instructions
//   010 Speed and Direction Instruction for reverse operation
//   011 Speed and Direction Instruction for forward operation
//   100 Function Group One Instruction
//   101 Function Group Two Instruction
//   110 Future Expansion
//   111 Configuration Variable Access Instruction
//
// For details, see S 9.2, RP 9.2.1 and/or RCN 211 and 212
// Note that especially RCN212 provides a nice overview of all possible commands.
//******************************************************************************************************
// analyse is the basic method to decode Loco messages and populate attributes of the LocoMessage object

Dcc::CmdType_t LocoMessage::analyse(void) {
  // Most messages that are received are Loco messages, since all locs known to the command station
  // will continiously be informed of latest speed and light information. The majority of loco
  // commands will therefore not be addressed to this decoder. To reduce load of the processor, we
  // decide as quickly as possible if the current message is interesting for this decoder.
  // If not, we return as immediately.
  //
  // Define some local variables for temporarary storage 
  uint8_t byte0 = dccMessage.data[0];
  uint8_t instructionByte;
  uint8_t dccData;                  // May be filled from data part in instruction or subsequent bytes 

  //
  // Step 1: Determine the loco address, and make already a copy of 
  // the instruction byte that defines the kind of command (CCC bits), as well as data
  if (byte0 & 0b10000000) {         // The first bit differentiates between basic and extended packets
    locoCmd.longAddress = true;     // It is an extended packet, with a 12 bit address (1..4096)
    locoCmd.address = ((byte0 & 0b00111111) << 8) | (dccMessage.data[1]);
    instructionByte = dccMessage.data[2];
    dccData = dccMessage.data[3];   // Initial data value. Can be changed later
  }
  else {
    locoCmd.longAddress = false;    // It is a basic packet, with a 7 bit address (1..127 (or 99))
    locoCmd.address = (byte0 & 0b01111111);
    instructionByte = dccMessage.data[1];
    dccData = dccMessage.data[2];   // Initial data value. Can be changed later
  }

  // Step 2: if we have a Loco Speed command, determine Speed and Direction
  // Since loco speed commands are the most common of all commands, they will be handled first.
  // Note that we check if this a speed command before we check if the packet is for this decoder,
  // since safety decoders may want to know if there is still some train running or not.
  // In case of a retransmission (speed and/or direction have not changed) the command will be ignored.
  uint8_t speed;                    // For 14/28 as well as 128 speed steps
  bool forward = false;
  bool emergencyStop = false;
  bool speedCommand = false;
  //
  // Step 2A: 14/28 Speed steps (See S 9.2 for details on how speed is coded)
  // Format: 01RG-GGGG - 14/28 speed steps
  if ((instructionByte & 0b11000000) == 0b01000000) { 
    speedCommand = true;
    if (instructionByte & 0b00100000) forward = true;
    speed = ((instructionByte & 0b00001111) << 1) + ((instructionByte & 0b00010000) >> 4);
    if (speed <= 3 ) {                                 // Stop or Emergency stop
      if (speed >=2 ) emergencyStop = true;            // Values 2 and 3 represent Emergency stop
      speed = 0;                                       // Values 0 ... 3 represent speed 0
      }
    else speed = speed - 3;                            // Step 1 is coded as 4
  }
  // Step 2B: 128 Speed steps
  // Format: 0011-1111 RGGG-GGGG - 128 Speed steps
  else if (instructionByte == 0b00111111) {
    speedCommand = true;
    if (dccData & 0b10000000) forward = true;
    speed = (dccData & 0b01111111);
    if (speed <= 1 ) {                                 // Stop or Emergency stop
      if (speed == 1 ) emergencyStop = true;           // Value 1 represent Emergency stop
      }
    else speed = speed - 1;                            // Step 1 is coded as 2
  }
  // Step 2C: If this is a speed command, it can still be a retransmission or emergency stop
  // Note that, in case we listen to multiple addresses, retransmission detection doesn't
  // really work, if the speed for one address differs from the other. 
  if (speedCommand) {
    if (IsMyAddress()) {                               // Three options now: LocoSpeed, EmergencyStop or Retransmission 
      if ((locoCmd.emergencyStop == emergencyStop) &&  // Retransmission?
         (locoCmd.speed == speed) && 
         (locoCmd.forward == forward)) {
        return(Dcc::IgnoreCmd);                        // It is a retransmission => ignore
      }
      if (emergencyStop) {                             // Emergency stop?
        locoCmd.speed = 0;      
        locoCmd.emergencyStop = true;
        return(Dcc::MyEmergencyStopCmd);
      }
      locoCmd.speed = speed;                           // This is a speed and direction command
      locoCmd.emergencyStop = false;
      locoCmd.forward = forward;
      return(Dcc::MyLocoSpeedCmd);                     // Ready, so return
    }
    else {
      // The loco speed command is not for this decoder, but we still check if the speed > 0.
      // The reason is that safety decoders may need to know if there are still trains moving.
      // If the speed > 0, we return with the SomeLocoMovesFlag. Otherwise with the
      // SomeLocoSpeedFlag, which can be used to detect the end of a RESET (Halt) period.
      if (speed > 0) return(Dcc::SomeLocoMovesFlag);
      return(Dcc::SomeLocoSpeedFlag);
    }
  }
  
  // Step 3: Ignore all remaining loco commands, unless they are intended for this decoder
  // Note that SPEED commands allready returned
  if (IsMyAddress() == false) return (Dcc::IgnoreCmd);

  // **************************************************************************************
  // From now on the fast majority of loco messages have been filtered, since the remaining
  // messages are all addressed to this loco. We'll further focus on commands that are
  // available in Lenz (LZV100) environments with XPressNet V3.6, since these can be tested.

  // Step 4: Check for Configuration Variable Access Instruction
  // We implement Programming of the Main (PoM), to allow changing of the feedback decoder's CV values
  // We only implement the long form of CV Access Instructions (see RP9.2.1)
  // This is also the only form of PoM supported by the XPressNet V3.6 specification
  // Format: 1110-xxxx
  if ((instructionByte & 0b11110000) == 0b11100000) return(cvMessage.analysePoM());

  // Step 5: Check for a Reset packet
  // When a Digital Decoder receives a Reset Packet, it shall erase all volatile memory
  // (including any speed and direction data), and return to its normal power-up state.
  // If the Digital Decoder is operating a locomotive at a non-zero speed when it receives a
  // Digital Decoder Reset, it shall bring the locomotive to an immediate stop.
  // Format: 0000-0000
  if (instructionByte == 0b00000000) {
    locoMessage.reset_speed();
    return(Dcc::ResetCmd);
  }

  // The next steps we analyse Instruction for Function Groups.
  // In case of accessory decoders, Instruction for Function Groups can be (mis)used
  // to change the position of switches.
  // In case of function decoders, Instruction for Function Groups can be used
  // to switch the light in cars
  // To detect possible retransmissions,  we first save the data in the local dccData

  // Step 6: Check if we have an Instruction for Function Group One (F0-F4)
  // F1..F4, to allow the setting of switches and relays via loco functions F1..F4
  // F1 is the least significant bit (bit 0). Note that in bit 4 we also return F0 (FL).
  // Format: 100D-DDDD
  if ((instructionByte & 0b11100000) == 0b10000000) {
    dccData = (instructionByte & 0b00011111);
    if (locoCmd.F0F4 == dccData) return(Dcc::IgnoreCmd);   // Retransmission?? => Ignore
    locoCmd.F0F4 = dccData;
    return(Dcc::MyLocoF0F4Cmd);
  }
  
  // Step 7: Check if we have an Instruction for Function Group Two (F5-F12)
  // Format: 101S-DDDD. If S is set, F5-F8, else F9-F12
  if ((instructionByte & 0b11100000) == 0b10100000) {             // F5-F12
    dccData = (instructionByte & 0b00001111);
    if ((instructionByte & 0b00010000) == 0b00010000) {           // F5-F8
      if (locoCmd.F5F8 == dccData) return(Dcc::IgnoreCmd);   // Retransmission?? => Ignore
      locoCmd.F5F8 = dccData;
      return(Dcc::MyLocoF5F8Cmd);
    }
    else {                                                        // F9-F12
    if (locoCmd.F9F12 == dccData) return(Dcc::IgnoreCmd);    // Retransmission?? => Ignore
    locoCmd.F9F12 = dccData;
    return(Dcc::MyLocoF9F12Cmd);
    }
  }

  // Step 8: Check if we have an Instruction for Function Groups F13-F68. See RCN212 for details
  // Format: 1101-XXXX DDDD-DDDD
  if ((instructionByte & 0b11110000) == 0b11010000) {
    uint8_t functionGroup = (instructionByte & 0b00001111);
    if (locoCmd.longAddress)  dccData = dccMessage.data[3];
      else dccData = dccMessage.data[2];
    switch (functionGroup) {
      case 0b00001110:                                                // F13-F20
        if (locoCmd.F13F20 == dccData) return(Dcc::IgnoreCmd);   // Retransmission?? => Ignore
        locoCmd.F13F20 = dccData;
        return(Dcc::MyLocoF13F20Cmd);
      case 0b00001111:                                                 // F21-F28
        if (locoCmd.F21F28 == dccData) return(Dcc::IgnoreCmd);    // Retransmission?? => Ignore
        locoCmd.F21F28 = dccData;
        return(Dcc::MyLocoF21F28Cmd);
      case 0b00001000:                                                 // F29-F36
        if (locoCmd.F29F36 == dccData) return(Dcc::IgnoreCmd);    // Retransmission?? => Ignore
        locoCmd.F29F36 = dccData;
        return(Dcc::MyLocoF29F36Cmd);
      case 0b00001001:                                                 // F37-F44
        if (locoCmd.F37F44 == dccData) return(Dcc::IgnoreCmd);    // Retransmission?? => Ignore
        locoCmd.F37F44 = dccData;
        return(Dcc::MyLocoF37F44Cmd);
      case 0b00001010:                                                 // F45-F52
        if (locoCmd.F45F52 == dccData) return(Dcc::IgnoreCmd);    // Retransmission?? => Ignore
        locoCmd.F45F52 = dccData;
        return(Dcc::MyLocoF45F52Cmd);
      case 0b00001011:                                                 // F53-F60
        if (locoCmd.F53F60 == dccData) return(Dcc::IgnoreCmd);    // Retransmission?? => Ignore
        locoCmd.F53F60 = dccData;
        return(Dcc::MyLocoF53F60Cmd);
      case 0b00001100:                                                 // F61-F68
        if (locoCmd.F61F68 == dccData) return(Dcc::IgnoreCmd);    // Retransmission?? => Ignore
        locoCmd.F61F68 = dccData;
        return(Dcc::MyLocoF61F68Cmd);
    }
  }

  
  // Return with an Ignore Command
  return (Dcc::IgnoreCmd);
  // Note that we did NOT analyse all possible messages
  // For example, the following messages could be useful in certain case:
  // - 0001-xxxx Consist Control Instruction
}
