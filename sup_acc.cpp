//******************************************************************************************************
//
// file:      sup_acc.cpp
// purpose:   Accessory decoder functions to support the DCC library
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
// 
// Accessory Digital Decoder Packet Formats:
// Basic (9bit address):     10AA-AAAA 1aaa-CTTP XXXX-XXXX
// Extended (11bit address): 10AA-AAAA 0aaa-0AA1 000D-DDDD XXXX-XXXX (NMRA S9.2.1)
// Extended (11bit address): 10AA-AAAA 0aaa-0AA1 dddD-DDDD XXXX-XXXX (RCN-213)
// - a = MSB of decoder address (in 1's complement)
// - A = LSB of decoder address
// - C = Activate
// - T = Turnout
// - P = Position
// - D = Data
// - X = XOR
//
// 
// ADDRESSING DETAILS
// ------------------
// We should distinguish between the addresses 1) entered on handhelds, 2) the address bits within the DCC
// accesory decoder packet, and 3) the address used within the decoder (decoderAddress and outputAddress).
//
// The address that can be entered on handhelds start with 1 and run till 2048 (although some systems, such
// as the Lenz LZV 100, use 1024 as maximum).
// 
//       +-----------------------+
//       |        Handheld       |
//       |                       |
//       |        1...2048       |
//       |                       |
//       +-----------+-----------+
//                   |
//                   |   Xpressnet / ...
//                   |
//       +-----------+-----------+                            +--------------------------+
//       |                       |                            |                          |
//       |                       |                            |    Accessory Decoder     |
//       |                       |             DCC            |                          |
//       |    Command Station    +----------------------------+  decoderAddress: 0..511  |
//       |                       |      MSB: aaa (0..7)       |  outputAddress: 1..2048  |
//       |                       |    LSB: AA-AAAA (0..63)    |    turnout / position    |
//       |                       |          TT (0..3)         |       (CV1 + CV9)        |
//       +-----------------------+                            +--------------------------+
//
// The address contained within a DCC accessory packet consists of three parts: 
// 1) the three Most Significant Bits (MSB - aaa)
// 2) the six Least Significant Bits (LSB -AA-AAAA)
// 3) the two Turnout Bits (TT) to select one the four turnouts that can be connected to a dcoder
//
// The RCN-213 standard (from the Rail Community) defines two kind of addresses:
// 1) decoder addresses, which are nine bits long (MSB + LSB)
// 2) output addresses, which are eleven bits long (MSB + LSB + TT).

// The NMRA S9.2.1 does not explicitly specify how the addresses that can be entered on 
// handhelds should be mapped on the addressing bits of the accessory command  message.
// Several strategies are possible to code handheld address 1:        (10AA-AAAA 1aaa-CTTP XXXX-XXXX)
// 1) All address bits within the DCC message get value "0"           (1000-0000 1111-C00P XXXX-XXXX)
// 2) The TT address bits within the DCC message get value "1"        (1000-0000 1111-C01P XXXX-XXXX)
// 3) The LSB address bits within the DCC message get value "1"       (1000-0001 1111-C00P XXXX-XXXX)
// As could be expected, different command stations implement different strategies:
// - The Roco 10764  command station uses the first strategy
// - The LENZ LZV100 command station (with XpressNet V3.6) use the third strategy
// - The OpenDCC Z1  command station (with XpressNet V3.6) use the third strategy
//
// The RCN213 describes these differences in more detail and states:
// - "Aus Gründen der Kompatibilität zu existierenden Zentralen ist die erste angesprochene Adresse
//    4 = 1000-0001 1111-D00R. Diese Adresse wird in Anwenderdialogen als Adresse 1 dargestellt". 
// - "Die vier ersten Adressen können am Ende des Adressbereichs angesprochen werden, wobei die Adresse
//    2047 als Adresse für den Notaus-Befehl übersprungen wird". 
// This means that strategy 3 conforms to the RCN213 standard, but the Roco 10764 does not.
//
// Another behavior of some command stations (in particlar those by Lenz) is that, if the LSB 
// value is 0, the MSB value is 1 to low. The reason for that behavior is (likely) that all possible
// address bit values should be used, in order to "not waste valuable address space". 
// RCN-213 describes this behavior as follows:
// - "Bei einigen existierenden Zentralen folgt auf die Adresse 255 nicht die Adresse 256, sondern es
//    werden die Adressen 0 bis 3 eingefügt. Danach geht es bei 260 weiter. Entsprechend bei Adresse 511,
//    767, 1023 usw."
//
// To illustrate this, lets take as example the combination of Lenz LH 100 / LZV 100 / Xpressnet V3.6:
//  LH100 =>    MSB    LSB    TT    MSB+LSB+TT
//    1   =>     0      1      0         4
//    2   =>     0      1      1         5
//    3   =>     0      1      2         6
//    4   =>     0      1      3         7
//    5   =>     0      2      0         8
//    .   =>     .      .      .                .
//  252   =>     0     63      3       255
//  253   =>     0      0      0         0     !!!
//  254   =>     0      0      1         1     !!!
//  255   =>     0      0      2         2     !!!
//  256   =>     0      0      3         3     !!!
//  257   =>     1      1      0       260
//    .   =>     .      .      .         .
//  508   =>     1     63      3       511
//  509   =>     1      0      0       256     !!!
//  510   =>     1      0      1       257     !!!
//  511   =>     1      0      2       258     !!!
//  512   =>     1      0      3       259     !!!
//  513   =>     2      1      0       516
//    .   =>     .      .      .
// 1020   =>     3     63      3      1023
// 1021   =>     3      0      0       768     !!!
// 1022   =>     3      0      1       769     !!!
// 1023   =>     3      0      2       770     !!!
// 1024   =>     3      0      3       771     !!!
//

// In this software we introduce the "myMaster" attribute to allow the main sketch to select between:
// - OpenDCC: strategy 3 
// - Lenz: strategy 3 but compensation for handheld addresses around multiples of 256 
// - Roco: strategy 1 (standard) and strategy 3 (lenz) command stations.  
//
// Within the accessory decoder we distinguish between:
// - decoderAddress (0..511): MSB + LSB (plus some compensation in case of LENZ systems)
// - turnout (1..4): TT + 1 
// - outputAddress (1..2048): decoderAddress + turnout 
// - device (1..8): TT + P    
// Note that the decoder/output address can also be stored in CV1 + CV9
// 
//******************************************************************************************************
#include "Arduino.h"
#include "AP_DCC_library.h"
#include "sup_acc.h"
#include "sup_isr.h"
#include "sup_cv.h"

// Declaration of external objects
extern Accessory  accCmd;              // instantiated in DCC_Library.cpp, used by main sketch
extern CvAccess   cvCmd;               // instantiated in DCC_Library.cpp, used by main sketch
extern DccMessage dccMessage;          // instantiated in, and used by, DCC_Library.cpp
extern AccMessage accMessage;          // instantiated in, and used by, DCC_Library.cpp
extern CvMessage  cvMessage;           // Interface to sup_cv


// Constructor for the AccMessage class
AccMessage::AccMessage(){
  accCmd.myDecAddrFirst = 65535;       // Ensure that, if not initialised, no messages matches my address
  accCmd.myDecAddrLast  = 65535;
  decoderAddress_old = 65535;          // This address should not be found in any accessory message
  byte1_old = 0b00000000;              // This pattern should not occur in any accessory command
  byte2_old = 0b11111111;              // This pattern should not occur in an extended accessory command
}


bool AccMessage::IsMyAddress() {
  const unsigned int broadcast_address = 2047;
  return ((accCmd.decoderAddress >= accCmd.myDecAddrFirst) && (accCmd.decoderAddress <= accCmd.myDecAddrLast)
       || (accCmd.decoderAddress == broadcast_address));
}


//******************************************************************************************************
// Packet structure:
// {preamble} AAAA-AAAA [AAAA-AAAA] IIII-IIII [IIII-IIII] [IIII-IIII] EEEE-EEEE
//
// Normal Accessory Packets:
// Basic Accesory:    10AA-AAAA 1aaa-CTTP EEEE-EEEE
// Extended Accesory: 10AA-AAAA 0aaa-0AA1 000X-XXXX EEEE-EEEE
//
// PoM messages (long form)
// Basic Accesory:    10AA AAAA 1AAA-1AA0 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE
// Extended Accesory: 10AA-AAAA 0AAA-0AA1 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE
//
// - a = MSB of decoder address (in 1's complement)
// - A = LSB of decoder address
// - C = Activate (for POM: Command Type)
// - T = Turnout
// - P = Position
// - V = CV-VALUE
// - D = CV-Data
// - E = XOR Error Check
//
//******************************************************************************************************

/*
 unsigned int decoderAddress_old; // To store the previously received decoder addres
 uint8_t byte1_old;               // From the previously received accessory command
 uint8_t byte2_old;               // From the previously received accessory command
 */

Dcc::CmdType_t AccMessage::analyse(void) {
  // Step 1: Determine the decoderAddress received
  // At this stage we only determine the decoder address; the output address is determined in step 4
  // MSB: take Bits 6 5 4 from dccMessage.data[1] and invert
  // LSB: take bits 5 4 3 2 1 0 from dccMessage.data[0]
  uint16_t msb = ((~dccMessage.data[1] & 0b01110000) << 2);
  uint8_t  lsb =  (dccMessage.data[0] & 0b00111111);
  // Step 1B: Correct the received address to deal with differences in command stations (see above)
  switch (accCmd.myMaster) {
    case Accessory::Lenz:
      if (lsb == 0) {msb = msb + 64;}
      accCmd.decoderAddress = msb + lsb - 1;
      break;
    case Accessory::Roco:
      accCmd.decoderAddress = msb + lsb;
      break;
    default:
      accCmd.decoderAddress = msb + lsb - 1;
      break;
  }
  // Step 2: Immediately return if this message is not intended for this decoder.
  // In this case MAIN may only use the decoderAddress (which is useful for initialising the decoder).
  // We filter retrainsmissions
  if (!IsMyAddress()) {                                 // Decoder address not in my own range
    if (accCmd.decoderAddress == decoderAddress_old)    // Is this for the same address as the previous?
      return(Dcc::IgnoreCmd);                           // We already notified main before, so ignore
    else {                                              // This command is for a new address
      decoderAddress_old = accCmd.decoderAddress;       // Save it
      return(Dcc::AnyAccessoryCmd);                     // And inform main that there is a new address
    }
  }
  //
  // Step 3: Determine the other attributes (the message is addressed to this decoder)
  uint8_t byte1 = dccMessage.data[1];                   // This may now be stored in a register
  uint8_t byte2 = dccMessage.data[2];                   // This could be the error byte
  accCmd.turnout =  ((byte1 & 0b00000110) >> 1) + 1;    // 1..4 - Decoders have 4 switches
  accCmd.position =  (byte1 & 0b00000001);              // 0..1 - A switch has 2 position
  accCmd.device =    (byte1 & 0b00000111);              // 0..7 - Or: the decoder has 7 devices
  accCmd.activate = ((byte1 & 0b00001000) >> 3);        // 0..1 - Activate the coil, servo, relay, ...
  // Note that only activates are expected, and no deactivates
  //
  // Step 4: Determine the outputAddress
  // 0..2047 => address of individual switch or signal
  accCmd.outputAddress = (accCmd.decoderAddress * 4) + accCmd.turnout;
  //
  // Step 5: Determine the kind of accessory command. Possible options include:
  // - Basic accessory command, used for switches and relays
  // - Extended accessory command, used for signals and complex devices
  // - CV access on the main
  if (byte1 & 0b10000000) accCmd.command = Accessory::basic;
    else accCmd.command = Accessory::extended;
  // return directly from each case (Break therefore not needed)
  switch (dccMessage.size) {
    case 3:                                                     // length 3: basic accesory command or NOP
    if (byte1_old == byte1) return(Dcc::IgnoreCmd);             // This is a retransmission. Ignore
    byte1_old = byte1;                                          // No retransmission, so save for the next time
      if (byte1 & 0b10000000) {return(Dcc::MyAccessoryCmd); }   // Basic command. Only command generated by LENZ
    else return(Dcc::IgnoreCmd);                                // No Operation Commmand. See RCN-213
  case 4:                                                       // Extended command
    if ((byte1_old == byte1) && (byte2_old == byte2))           // Is this a retransmission?
      return(Dcc::IgnoreCmd);                                   // Ignore
    byte1_old = byte1;                                          // No retransmission, so save for the next time
    byte2_old = byte2;                                          // Same for second byte
    accCmd.signalHead = byte2;                                  // 0..255: the signal's value
    return(Dcc::MyAccessoryCmd);                                // Command intended for this decoder
  case 5:                                                       // CV Access Instruction - Short Form
    return(Dcc::IgnoreCmd);                                     // Meaningless for Accessory decoders
  case 6:                                                       // CV Access Instruction - Long Form
    // PoM Accessory commands are not supported by LENZ / Expressnet V3.6
    // Basic Accesory:    10AA AAAA 1AAA-1AA0 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE
    // Extended Accesory: 10AA-AAAA 0AAA-0AA1 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE
    // Retransmissions are not handled here, but in analysePom()
    if ((dccMessage.data[2] & 0b11110000) == 0b11100000) return(cvMessage.analysePoM());
    return(Dcc::IgnoreCmd);                                     // Unknown packet
  };
  return(Dcc::IgnoreCmd);                                       // Unknown packet
};



/* SAVE VOOR DE ZEKERHEID
 
 Dcc::CmdType_t AccMessage::analyse(void) {
 // Step 1: Determine the decoderAddress received
 // At this stage we only determine the decoder address; the output address is determined in step 4
 // MSB: take Bits 6 5 4 from dccMessage.data[1] and invert
 // LSB: take bits 5 4 3 2 1 0 from dccMessage.data[0]
 uint16_t msb = ((~dccMessage.data[1] & 0b01110000) << 2);
 uint8_t  lsb =  (dccMessage.data[0] & 0b00111111);
 // Step 1B: Correct the received address to deal with differences in command stations (see above)
 switch (accCmd.myMaster) {
 case Accessory::Lenz:
 if (lsb == 0) {msb = msb + 64;}
 accCmd.decoderAddress = msb + lsb - 1;
 break;
 case Accessory::Roco:
 accCmd.decoderAddress = msb + lsb;
 break;
 default:
 accCmd.decoderAddress = msb + lsb - 1;
 break;
 }
 // Step 2: Immediately return if this message is not intended for this decoder.
 // In this case MAIN may only use the decoderAddress (which is useful for initialising the decoder).
 if (!IsMyAddress()) return(Dcc::AnyAccessoryCmd);  // Decoder address not in my own range
 //
 // Step 3: Determine the other attributes (the message is addressed to this decoder)
 uint8_t byte1 = dccMessage.data[1];
 accCmd.turnout =  ((byte1 & 0b00000110) >> 1) + 1;    // 1..4 - Decoders have 4 switches
 accCmd.position =  (byte1 & 0b00000001);              // 0..1 - A switch has 2 position
 accCmd.device =    (byte1 & 0b00000111);              // 0..7 - Or: the decoder has 7 devices
 accCmd.activate = ((byte1 & 0b00001000) >> 3);        // 0..1 - Activate the coil, servo, relay, ...
 // Note that only activates are expected, and no deactivates
 //
 // Step 4: Determine the outputAddress
 // 0..2047 => address of individual switch or signal
 accCmd.outputAddress = (accCmd.decoderAddress * 4) + accCmd.turnout;
 //
 // Step 5: Determine the kind of accessory command. Possible options include:
 // - Basic accessory command, used for switches and relays
 // - Extended accessory command, used for signals and complex devices
 // - CV access on the main
 if (dccMessage.data[1] & 0b10000000) accCmd.command = Accessory::basic;
 else accCmd.command = Accessory::extended;
 // return directly from each case (Break therefore not needed)
 switch (dccMessage.size) {
 case 3:
 if (dccMessage.data[1] & 0b10000000)                        // Basic command. Only command generated by LENZ
 return(Dcc::MyAccessoryCmd);                              // Command intended for this decoder
 else return(Dcc::IgnoreCmd);                                // No Operation Commmand. See RCN-213
 case 4:                                                       // Extended command
 accCmd.signalHead = dccMessage.data[2];                     // 0..255: the signal's value
 return(Dcc::MyAccessoryCmd);                                // Command intended for this decoder
 case 5:                                                       // CV Access Instruction - Short Form
 return(Dcc::IgnoreCmd);                                     // Meaningless for Accessory decoders
 case 6:                                                       // CV Access Instruction - Long Form
 // Note: PoM Accessory commands are not supported by LENZ / Expressnet V3.6
 // Basic Accesory:    10AA AAAA 1AAA-1AA0 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE
 // Extended Accesory: 10AA-AAAA 0AAA-0AA1 1110-CCVV VVVV-VVVV DDDD-DDDD EEEE-EEEE
 if ((dccMessage.data[2] & 0b11110000) == 0b11100000) return(cvMessage.analysePoM());
 return(Dcc::IgnoreCmd);                                     // Unknown packet
 };
 return(Dcc::IgnoreCmd);                                       // Unknown packet
 };

 */