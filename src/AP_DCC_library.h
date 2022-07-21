//******************************************************************************************************
//
// file:      AP_DCC_library.h
// purpose:   DCC library for ATmega AVRs (16, 328, 2560, ...)
// author:    Aiko Pras
// version:   2021-05-15 V1.0.2 ap initial version
//
// history:   This is a further development of the OpenDecoder 2 software, as developed by W. Kufer.
//            It has been rewritten such that it can be used for Arduino (Atmel AVR) environments.
//            For details, see the .cpp file
//
// usage:     The main sketch should declare the following objects:
//            - extern Dcc           dcc;     // The main DCC object
//            - extern Accessory     accCmd;  // To retrieve the data from accessory commands
//            - extern Loco          locoCmd; // To retrieve the data from loco commands  (7 & 14 bit)
//            - extern CvAccess      cvCmd;   // To retrieve the data from pom and sm commands
//            Setup() should call dcc.dccPin). dccPin is the interrupt pin for the DCC signal
//            The main loop() should call dcc.input() as often as possible. If there is input,
//            dcc.cmdType tells what kind of command was received (such as MyAccessoryCmd or MyLocoF0F4Cmd).
//
//            Note that command stations will periodically retransmit certain commands, to ensure
//            that, even in noisy environments, commands will be received. Such retransmissions
//            will be filtered within this library. The main sketch will not receive retransmissions.
//
// hardware:  - On traditional ATMega processors Timer 2 is used.
//              On novel (MegaCoreX, DxCore) ATMega processors, TCB0 is used (another TCB may be
//              selected by uncommenting the associated #define in sup_isr_MegaCoreX_DxCore.h
//              In addition, on these new processors, also a free Event channel and some General
//              PurPose Data Registers (GPIOR0 - GPIOR2) are used.
//            - a free to chose interrupt pin (dccpin) for the DCC input signal
//            - a free to chose digital output pin for the DCC-ACK signal
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#pragma once

// #define VOLTAGE_DETECTION             // Uncomment this line if this code should trigger ADC hardware
#define MaxDccSize         6             // DCC messages can have a length upto this value


//******************************************************************************************************
//                                            DCC Class
//******************************************************************************************************
// This is the main class to receive and analyse DCC messages. It has three methods: attach(), detach()
// and input(). The main loop() should call dcc.input() as often as possible. If there is input,
// dcc.cmdType informs main what kind of command was received.

class Dcc {
  public:
    typedef enum {                               // Which command types are possible?
      Unknown,                                   // Start value, but should never be returned
      IgnoreCmd,                                 // Command should be ignored
      ResetCmd,                                  // We have received a reset command
      SomeLocoSpeedFlag,                         // Loco speed = 0, but not for this decoder => end of Reset
      SomeLocoMovesFlag,                         // Loco speed > 0, but not for this decoder
      MyLocoSpeedCmd,                            // Loco speed and direction command, for this decoder
      MyEmergencyStopCmd,                        // Loco Emergency stop, for this decoder
      MyLocoF0F4Cmd,                             // F0..F4  command, for this decoder address(es)
      MyLocoF5F8Cmd,                             // F5..F8  command, for this decoder address(es)
      MyLocoF9F12Cmd,                            // F9..F12 command, for this decoder address(es)
      MyLocoF13F20Cmd,                           // F13..F20 command, for this decoder address(es)
      MyLocoF21F28Cmd,                           // F21..F28 command, for this decoder address(es)
      MyLocoF29F36Cmd,                           // F29..F36 command, for this decoder address(es)
      MyLocoF37F44Cmd,                           // F37..F44 command, for this decoder address(es)
      MyLocoF45F52Cmd,                           // F45..F52 command, for this decoder address(es)
      MyLocoF53F60Cmd,                           // F53..F60 command, for this decoder address(es)
      MyLocoF61F68Cmd,                           // F61..F68 command, for this decoder address(es)
      AnyAccessoryCmd,                           // Accessory command, but not for this decoder address(es)
      MyAccessoryCmd,                            // Accessory command, for this decoder address(es)
      MyPomCmd,                                  // Programming on the Main (PoM)
      SmCmd                                      // Programming in Service Mode (SM = programming track)
    } CmdType_t;
    CmdType_t cmdType;                           // What kind of DCC message did we receive?

    void attach(uint8_t dccPin,
                uint8_t ackPin=255);             // Start the timer and DCC ISR. Pins are Arduino Pin numbers
    void detach(void);                           // Stops the timer and DCC ISR
    bool input(void);                            // Analyze the DCC message received. Returns true, if new message
    void sendAck(void);                          // Create a 6ms DCC ACK signal (needed for Service Mode)


    uint8_t errorXOR;                            // The number of DCC packets with an incorrect checksum

  private:
    CmdType_t analyze_broadcast_message(void);
    uint8_t _ackPin;                            // Set by attach(), and used by sendAck()
};

//******************************************************************************************************
//                                            ACCESSORY COMMANDS
//******************************************************************************************************
// The NMRA Standard 9.2.1 and RCN-213 define two types of accessory commands:
// 1) Basic accessory command, used for switches and similar
// 2) Extended accessory command, used for signals, turntables and other, more complex devices
// *** command (basic..extended) ***
// The "command" attribute indicates whether a basic or an extended accessory command was received.
// All command stations can generate basic accessory commands, but only a few support extended accesory
// commands. The LENZ LZV101 (with Xpressnet V3.6) for example can't, but the OpenDCC command station
// can generate extended accesory commands.
//
// Commands for accessory decoders contain fields that can be interpreted in different ways, depending
// on the specific decoder that is being implemented.
//
// OPTION 1: Decoder based addressing (basic):
// The most common type of accessory decoder is the switch decoder, which connects upto 4 switches.
// Such decoders understand basic accesory commands, and their fields can be interpreted as follows:
// *** decoderAddress (0..511) ***
// The address of the decoder (board). The NMRA standard defines 9-bits for decoder addresses, although
// some command stations (like the LENZ LZV101 with XpressNet V3.6)  support only 8 bits (0..255).
// *** turnout (1..4) ***
// The "turnout" attribute tells which of the four switches is being targetted.
//
// OPTION 2: Output based addressing (basic and extended)
// *** outputAddress (1..2048) ***
// Instead of addressing decoders (that connect multiple switches), it is also possible to directly
// address individual switches or other outputs (relays, signals, turntables etc). Output addresses
// are 11 bits in length, and consists basically of the concatenation of the "decoderAddress" and
// the "switch" attributes.
//
// *** position (0..1) *** (basic)
// Most switches can be positioned via two coils or servos. The "position" attribute tells which of
// the two coils is being targetted. Unfortunately different command stations made different choices
// regarding the interpretation of 0 or 1. According to RCN-213, a value 0 is used for "diverging"
// tracks (red), whereas the value 1 is used for "straight" tracks (green). Some command stations
// use '-' for 0, or '+' for 1 to indicate the position.
//
// *** device (1..8) *** (basic)
// In case of relays or other on/off devices usage of the "turnout" and "position" attributes may seem
// artificial. In such cases the "device" attribute may be used instead.
//
// *** activate (0..1) *** (basic)
// Whether the output should be actived (on) or deactivated (off).
//
// *** signalHead (0..255) *** (extended)
// The value contained in the extended accessory command. The NMRA S-9.2.1 standard allocates 5 bits,
// allowing values between 0 and 31, whereas the RCN-213 standard allocates 8 bits, thus 0..255
// Can be used to display different signal heads, but might also be used for turntables or other,
// more complex devices.
//
// RELATION BETWEEN ATTRIBUTES
// In case of a basic accesory command:
// - outputAddress (11 bits) = decoderAddress (9 bits) + turnout (2 bits)
// - device (3 bits) = turnout (2 bits) + position (1 bit)
// Since all attributes will get the correct values, the choice which attributes to use is upto
// the sletch writer.
//
// ADDRESSING DETAILS
// The broadcast outputAddress is 2047.
//
// Different command station manufacturers made different choices regarding the exact coding
// of the address bits within the DCC packet. See "sup_acc.cpp" for details.
// In many cases these differences can be neglected, unless the decoder address will also be
// used for other purposes, such as calculating CV values, or generating feedback / POM addresses.
// The "myMaster" attribute can be set by the main sketch to "Lenz", "OpenDcc" or "Roco"
// to deal with different command station behavior. The default value is "Lenz".
//
// An Accesory Decoder may listen to one or multiple decoder addresses, for example if it supports more than
// four switches or skips uneven addresses. After startup, a call should be made to setMyAddress().
// If the call includes a single parameter, that parameter represents the (single) address this decoder
// will listen to. If the call includes two parameters, these parameters represent the range of addresses
// this decoder will listen to.
//
//******************************************************************************************************
const uint8_t Roco = 0;     // Roco 10764 with Multimouse
const uint8_t Lenz = 1;     // LENZ LZV100 with Xpressnet V3.6 - Default value
const uint8_t OpenDCC = 2;  // OpenDCC Z1 with Xpressnet V3.6


class Accessory {
  public:
  
    // Decoder specific attributes should be initialised in setup()
    void setMyAddress(unsigned int first, unsigned int last = 65535);
    uint8_t myMaster = Lenz;

    // The next attributes inform the main sketch about the contents of the received accessory command
    typedef enum {
      basic,
      extended
    } Command_t;
    Command_t command;                   // What type of accessory command is received (basic / extended)

    unsigned int decoderAddress;         // 0..511  - Received decoder addres. 511 is the broadcast address
    unsigned int outputAddress;          // 1..2048 - The address of an individual switch or signal
    uint8_t device;                      // 1..8    - For on/off devices, such as relays
    uint8_t turnout;                     // 1..4    - For switches, which have two states (coils / servo's)
    uint8_t position;                    // 0..1    - The turnout position (straight-curved, green-red, -/+)
    uint8_t activate;                    // 0..1    - If the relay or coil should be activated or deactivated
    uint8_t signalHead;                  // 0..255  - In case of an extended accessory command, the signal's value
};


//******************************************************************************************************
//                                               LOCO COMMANDS
//******************************************************************************************************
// After startup, the Loco object should be initialised with the range of loco addresses it will listen
// too. For that purpose a call should be made to setMyAddress(). If the call includes a single
// parameter, that parameter represents the (single) loco address this decoder will listen to.
// If the call includes two parameters, these parameters represent the range of loco addresses this
// decoder will listen to.
//
// We analyse most commands, but no attempt is made to be complete.
// The focus is on those commands that may be useful for accesory decoders, that  listen to some
// loco commands to facilitate PoM. In addition, some functions are included that may be usefull for
// safety decoders as well as function decoders (for switchin lights within couches).
//
//******************************************************************************************************
class Loco {
  public:
    // Decoder specific attributes. Should be initialised in setup()
    void setMyAddress(unsigned int first, unsigned int last = 65535);

    // Attributes that contain information from the received loco message
    unsigned int address;              // 0..9999  - Received Loco addres
    bool         longAddress;          // Was the received adress 14 bit, or 7-bit?
    bool         emergencyStop;        // Flag: emargency stop for this decoder
    uint8_t      speed;                // 0..28 / 0..127
    bool         forward;              // True = Forward / False = Reverse
    uint8_t      F0F4;                 // 0..31. Least significant bit is F1. F0 is bit 4
    uint8_t      F5F8;                 // 0..15. Least significant bit is F5
    uint8_t      F9F12;                // 0..15. Least significant bit is F9
    uint8_t      F13F20;               // 0..255. Least significant bit is F13
    uint8_t      F21F28;               // 0..255. Least significant bit is F21
    uint8_t      F29F36;               // 0..255. Least significant bit is F29
    uint8_t      F37F44;               // 0..255. Least significant bit is F37
    uint8_t      F45F52;               // 0..255. Least significant bit is F45
    uint8_t      F53F60;               // 0..255. Least significant bit is F53
    uint8_t      F61F68;               // 0..255. Least significant bit is F61
};


//******************************************************************************************************
//                           CV-ACCESS (SM AND POM) FOR LOCO AND ACCESSORY DECODER
//******************************************************************************************************
// The behavior of the decoder is determined by the setting of certain Configuration Variables (CVs)
// Commands to access these variables can be send in Service Mode (SM = Programming Track) or in
// Programming on the Main (PoM) mode. This decoder supports both modes.
//
// According to S-9.2.1, PoM supports 2 methods to access Configuration Variables (CVs): Short form
// and Long form. Of these, only the long form is implemented by this Library
//
// According to S-9.2.3, Service Mode supports 4 methods to access Configuration Variables (CVs):
// Direct Configuration, Address-Only, Physical Register, and Paged Addressing.
// Of these, only Direct Configuration is implemented by this Library
//
// There are several conditions to be satisfied before CV access commands can be accepted.
// In SM, a reset packet must be received before a CV access command may be accepted, and timeouts
// must be obeyed. Only the second command may be acted upon. This library ensures that all these
// conditions are met.
//
// To determine the value of a specific CV, a Command Station usualy sends 8 consecutive verify bit
// commands, one to check each individual bit of the 8-bit variable. After that, the Command Station
// may issue a verify byte command, to ensure no errors occured.
//
//******************************************************************************************************
class CvAccess {
  public:
    typedef enum {
      reserved,                          // CC = 00
      verifyByte,                        // CC = 01
      bitManipulation,                   // CC = 10
      writeByte                          // CC = 11
    } operation_t;
    operation_t operation;

    unsigned int number;                 // 1..1024 - CV number
    uint8_t value;                       // 0..255  - CV value

    // bitManipulation
    uint8_t writecmd;                    // 0 = verify bit command, 1 = write bit command
    uint8_t bitvalue;                    // 0..1
    uint8_t bitposition;                 // 0..7

    uint8_t writeBit(uint8_t data);      // write bitvalue on bitposition
    bool verifyBit(uint8_t data);        // the received bitvalue on bitposition matches the old

};
