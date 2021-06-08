# AP_DCC_Library #

This Arduino library decodes (NMRA) Digital Command Control (DCC) messages. It is primarily intended for Accesoory Decoders.

## History ##
This software is a further development of OpenDecoder 2, as developed by W. Kufer. It has been rewritten, such that it can be used for Arduino (Atmel AVR) environments. From a functional point of view, this code is comparable to (a subset of) the NMRA-DCC library. It has been tested on several Arduino boards, as well as on other boards with ATMega16 and 2560 processors.

Like the original OpenDecoder 2 Software and version 1.2 of the NMRA-DCC library, but in contrast to version 2 of the NMRA-DCC library, this code (still) uses a separate Timer (Timer2) to decode the DCC signal. A nice feature of this code is that it allows the DCC input (interrupt pin) to be freely chosen. However, to improve readability, maintainability and extendibility, this code includes many comments and may therefore be easier to adapt and modify to personal needs.

## Reference ##
This code follows the NMRA DCC standard S9.2 S9.2.1 and S9.2.3 as well as RCN211-RCN214 (in German, by [http://railcommunity.org](http://railcommunity.org)), which include more detailed descriptions as well as the differences in accessory decoder addresses.

## Usage ##
The main sketch should declare the following objects:
```
extern Dcc dcc;
extern Accessory accCmd;
extern Loco locoCmd;
extern CvAccess cvCmd;
```

`setup()` should call `dcc.begin(dccPin, ackPin)`.

The main loop() should call `dcc.input()` as often as possible. If there is input, `dcc.cmdType` tells what kind of command was received (such as `MyAccessoryCmd` or `MyLocoF0F4Cmd`).
Note that command stations will periodically retransmit certain commands, to ensure that, even in noisy environments, commands will be received. Such retransmissions will be filtered by this library. The main sketch therefore does not receive retransmissions.

## Example: Accessory commands monitor ##
```
#include <Arduino.h>
#include <AP_DCC_library.h>

const uint8_t dccPin = 20;       // DCC input is connected to Pin 20 (INT1)

extern Dcc dcc;                  // Main object
extern Accessory accCmd;         // Data from received accessory commands

void setup() {
  dcc.begin(dccPin);
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  Serial.println();
  // The decoder can listen to one, or a range of addresses. In this example
  // we display details for all decoders with addresses between 1 and 100.
  // Note: One decoder address supports 4 switches   
  accCmd.SetMyAddress(1, 100);    // Decoder 100 is switch 397..400
  delay(1000);
  Serial.println("Test DCC lib");
}

void loop() {
  if (dcc.input()) {
    switch (dcc.cmdType) {

      case Dcc::MyAccessoryCmd :
        if (accCmd.command == Accessory::basic)
          Serial.print(" Basic accessory command for my decoder address: ");
          else Serial.print(" Extended accessory command for my decoder address: ");
        Serial.println(accCmd.decoderAddress);
        Serial.print(" - Turnout: ");
        Serial.println(accCmd.turnout);
        Serial.print(" - Switch number: ");
        Serial.print(accCmd.outputAddress);
        if (accCmd.position == 1) Serial.println(" +");
          else Serial.println(" -");
        if (accCmd.activate) Serial.println(" - Activate");
        Serial.println();
      break;

      case Dcc::AnyAccessoryCmd :
        Serial.print(accCmd.decoderAddress);
        Serial.print(" (any)");
        Serial.println();
      break;

    }
  }
}
```

## Hardware ##
- Timer 2 is used
- A free to chose interrupt pin (dccpin) for the DCC input signal
- A free to chose digital output pin for the DCC-ACK signal. Only needed if SM programming is required.

## The DCC Class ##
This is the main class to receive and analyse DCC messages. It has three methods: `begin()`, `end()` and `input()`.

#### void begin(uint8_t dccPin, uint8_t ackPin=255) ####
Starts the timer T2 and initialises the DCC Interrupt Service Routine (ISR).
- dccPin is the interrupt pin for the DCC signal. Basically any available AVR interrupt pin may be selected.  
- ackPin is an optional parameter to specify the pin for the DCC Service Mode Acknowledgement signal (of 6ms). If this parameter is omitted, no DCC acknowledegements will be generated.

The interrupt routine that reads the value of the `dccPin` must be fast. The traditional Arduino `digitalRead()` function is relatively slow, certainly when compared to direct port reading, which can be done (for example) as follows: `DccBitVal = !(PIND & (1<<PD3);`
However, direct port reading has as disadvantage that the port and mask should be hardcoded, and can not be set by the main sketch, thus the user of this library. Therefore we take a slightly slower approach, and use a variable called `portInputRegister`, which points to the correct input port with the right bitmask. With tis approach we actually split the Arduino `digitalRead()` function into a slow initialisation part, which maps dccPin to `portInputRegister`, and a fast reading part, which grabs data from that port.

#### void end(void) ####
Stops the timer and DCC ISR. This is needed before a decoder can be soft-reset.

#### bool input(void) ####
Checks if a new DCC message is received. Returns true, if a new message is received.
The main loop() should call `dcc.input()` as often as possible. If there is input, `dcc.cmdType` informs main what kind of command was received. The following command types are possible:
````
  Unknown               - Start value, but should never be returned
  IgnoreCmd             - Command should be ignored
  ResetCmd              - We have received a reset command
  SomeLocoMovesFlag     - Loco speed > 0, but not for this decoder
  MyLocoSpeedCmd        - Loco speed and direction command, for this decoder
  MyEmergencyStopCmd    - Loco Emergency stop, for this decoder
  MyLocoF0F4Cmd         - F0..F4  command, for this decoder address(es)
  MyLocoF5F8Cmd         - F5..F8  command, for this decoder address(es)
  MyLocoF9F12Cmd        - F9..F12 command, for this decoder address(es)
  MyLocoF13F20Cmd       - F13..F20 command, for this decoder address(es)
  MyLocoF21F28Cmd       - F21..F28 command, for this decoder address(es)
  MyLocoF29F36Cmd       - F29..F36 command, for this decoder address(es)
  MyLocoF37F44Cmd       - F37..F44 command, for this decoder address(es)
  MyLocoF45F52Cmd       - F45..F52 command, for this decoder address(es)
  MyLocoF53F60Cmd       - F53..F60 command, for this decoder address(es)
  MyLocoF61F68Cmd       - F61..F68 command, for this decoder address(es)
  AnyAccessoryCmd       - Accessory command, but not for this decoder address(es)
  MyAccessoryCmd        - Accessory command, for this decoder address(es)
  MyPomCmd              - Programming on the Main (PoM)
  SmCmd                 - Programming in Service Mode (SM = programming track)
````
#### sendAck() ####
Create a 6ms DCC ACK signal, which is needed for Service Mode programming.


## The ACCESSORY Class ##
The NMRA Standard 9.2.1 and RCN-213 define two types of accessory commands:
1. Basic accessory command, used for switches and similar
2. Extended accessory command, used for signals, turntables and other, more complex devices

#### command (basic..extended) ####
The `command` attribute indicates whether a basic or an extended accessory command was received.
All command stations can generate basic accessory commands, but only a few support extended accesory commands. The LENZ LZV101 (with Xpressnet V3.6) for example can't, but the OpenDCC command station can generate extended accesory commands.

Commands for accessory decoders contain fields that can be interpreted in different ways, depending on the specific decoder that is being implemented.

_**1. Decoder based addressing (basic):**_

The most common type of accessory decoder is the switch decoder, which connects upto 4 switches. Such decoders understand basic accesory commands, and their fields can be interpreted as follows:
  #### decoderAddress (0..511) ####
    The address of the decoder (board). The NMRA standard defines 9-bits for decoder addresses, although some command stations (like the LENZ LZV101 with XpressNet V3.6) support only 8 bits (0..255).
  #### turnout (1..4) ####
  The `turnout` attribute tells which of the four switches is being targeted.

_**2. Output based addressing (basic and extended):**_
   #### outputAddress (1..2048) ####
   Instead of addressing decoders (that connect multiple switches), it is also possible to directly address individual switches or other outputs (relays, signals, turntables etc). Output addresses are 11 bits in length, and consists basically of the concatenation of the `decoderAddress` and the `switch` attributes.

#### position (0..1) (basic) ####
Most switches can be positioned via two coils or servos. The "position" attribute tells which of the two coils is being targetted. Unfortunately different command stations made different choices regarding the interpretation of 0 or 1. According to RCN-213, a value 0 is used for "diverging"  tracks (red), whereas the value 1 is used for "straight" tracks (green). Some command stations use '-' for 0, or '+' for 1 to indicate the position.

#### device (1..8) (basic) ####
In case of relays or other on/off devices usage of the `turnout` and `position` attributes may seem artificial. In such cases the `device` attribute may be used instead.

#### activate (0..1) (basic) ####
Whether the output should be actived (on) or deactivated (off).

#### signalHead (0..255) (extended) ####
The value contained in the extended accessory command. The NMRA S-9.2.1 standard allocates 5 bits, allowing values between 0 and 31, whereas the RCN-213 standard allocates 8 bits, thus 0..255. Can be used to display different signal heads, but might also be used for turntables or other, more complex devices.

#### RELATION BETWEEN ATTRIBUTES ####
In case of a basic accesory command:
* outputAddress (11 bits) = decoderAddress (9 bits) + turnout (2 bits)
* device (3 bits) = turnout (2 bits) + position (1 bit)
Since all attributes will get the correct values, the choice which attributes to use is upto the sketch writer.

#### ADDRESSING DETAILS ####
The broadcast outputAddress is 2047.

Different command station manufacturers made different choices regarding the exact coding of the address bits within the DCC packet. See "support_accesory.cpp" for details. In many cases these differences can be neglected, unless the decoder address will also be used for other purposes, such as calculating CV values, or generating feedback / POM addresses. The `myMaster` attribute can be set by the main sketch to `Lenz`, `OpenDcc` or `Roco` to deal with different command station behavior. The default value is `Lenz`.

An Accessory Decoder may listen to one or multiple decoder addresses, for example if it supports more than four switches or skips uneven addresses. After startup, a call should be made to `SetMyAddress()`. If the call includes a single parameter, that parameter represents the (single) address this decoder listens to. If the call includes two parameters, these parameters represent the range of addresses this decoder listens to.


## The LOCO Class ##
In `setup()`, the Loco object should be initialised with the range of loco addresses it listens to. For that purpose, a call should be made to `SetMyAddress()`. If the call includes a single parameter, that parameter represents the (single) loco address this decoder will listen to. If the call includes two parameters, these parameters represent the range of loco addresses this decoder listens to.

We analyse most commands, but no attempt is made to be complete. The focus is on those commands that may be useful for accessory decoders, that  listen to some loco commands to facilitate PoM. In addition, some functions are included that may be useful for safety decoders as well as function decoders (for switching lights within couches).

The following data can be obtained from the Loco class:
````
// Attributes that contain information from the received loco message
unsigned int address;              // 0..9999  - Received Loco addres
bool         longAddress;          // Was the received adress 14 bit, or 7-bit?
bool         trainsMoving;         // Flag: set if we receive for any decoder a speed command > 0
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

````

## The CvAccess Class ##
The behaviour of the decoder is determined by the setting of certain Configuration Variables (CVs) Commands to access these variables can be send in Service Mode (SM = Programming Track) or in Programming on the Main (PoM) mode. This decoder supports both modes.

According to S-9.2.1, PoM supports 2 methods to access Configuration Variables (CVs): Short form and Long form. Of these, only the long form is implemented by this Library

According to S-9.2.3, Service Mode supports 4 methods to access Configuration Variables (CVs): Direct Configuration, Address-Only, Physical Register, and Paged Addressing.
Of these, only Direct Configuration is implemented by this Library

There are several conditions to be satisfied before CV access commands can be accepted. In SM, a reset packet must be received before a CV access command may be accepted, and timeouts must be obeyed. Only the second command may be acted upon. This library ensures that all these conditions are met.

To determine the value of a specific CV, a Command Station usually sends 8 consecutive verify bit commands, one to check each individual bit of the 8-bit variable. After that, the Command Station may issue a verify byte command, to ensure no errors occurred.

The following data can be obtained from the Loco class:
````
unsigned int number;      // 1..1024 - CV number
uint8_t value;            // 0..255  - CV value
operation_t operation;    // verifyByte, writeByte, bitManipulation
````

In case of a bitManipulation operation, the following additional data can be obtained:
````
uint8_t writecmd;         // 0 = verify bit command, 1 = write bit command
uint8_t bitvalue;         // 0..1
uint8_t bitposition;      // 0..7
````
In addition, two methods may be invoked:
````
uint8_t writeBit(uint8_t data);  // write bitvalue on bitposition
bool verifyBit(uint8_t data);    // the received bitvalue on bitposition matches the old
````
