# <a name="AP_DCC_library"></a>AP_DCC_library #

This Arduino library decodes (NMRA) Digital Command Control (DCC) messages. It is primarily intended for accessory Decoders and runs on ATMega processors.

## Relation to the NmraDcc library ##
Like the NmraDcc library, this library runs on ATMega processors, such as the ATMega 328, which is used on the Arduino UNO board. Whereas the NmraDcc library also runs on other microcontrollers, such as the ESP-32 and the Raspberry Pi Pico, this library only runs ATMega processors. However, on newer ATMega processors, such as the 4809 (Arduino Nano Every, MegaCoreX) and the AVR128DA (DxCore), this library provides better performance and reliability. In addition, it is believed that this library is better structured and documented, making modifications easier. See [History and Differences](extras/History_Differences.md) to understand the motivation why this library has been developed and how it compares to the NmraDcc library.

___

## <a name="Dcc"></a>The Dcc Class ##
This is the main class to receive and analyse DCC messages. It has three methods: `attach()`, `detach()` and `input()`.

#### void attach(uint8_t dccPin, uint8_t ackPin=255) ####
Starts the timer T2 and initialises the DCC Interrupt Service Routine (ISR).
- dccPin is the interrupt pin for the DCC signal. Basically any available AVR interrupt pin may be selected.  
- ackPin is an optional parameter to specify the pin for the DCC Service Mode Acknowledgement signal (of 6ms). If this parameter is omitted, no DCC acknowledegements will be generated.

The interrupt routine that reads the value of the `dccPin` must be fast. The traditional Arduino `digitalRead()` function is relatively slow, certainly when compared to direct port reading, which can be done (for example) as follows: `DccBitVal = !(PIND & (1<<PD3);`
However, direct port reading has as disadvantage that the port and mask should be hardcoded, and can not be set by the main sketch, thus the user of this library. Therefore we take a slightly slower approach, and use a variable called `portInputRegister`, which points to the correct input port with the right bitmask. With tis approach we actually split the Arduino `digitalRead()` function into a slow initialisation part, which maps dccPin to `portInputRegister`, and a fast reading part, which grabs data from that port.

#### void detach(void) ####
Stops the timer and DCC ISR. This is needed before a decoder can be soft-reset.

#### bool input(void) ####
Checks if a new DCC message is received. Returns true, if a new message is received.
The main loop() should call `dcc.input()` as often as possible. If there is input, `dcc.cmdType` informs main what kind of command was received.

#### CmdType_t cmdType ####
Tells the main sketch what kind of command was received. The following command types are possible:
````
  Unknown               - Start value, but should never be returned
  IgnoreCmd             - Command should be ignored
  ResetCmd              - We have received a reset command
  SomeLocoSpeedFlag     - Loco speed == 0, but not for this decoder => end of Reset
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
#### void sendAck(void) ####
Create a 6ms DCC ACK signal, which is needed for Service Mode programming.

___

## <a name="Accessory"></a>The Accessory Class ##

The NMRA Standard 9.2.1 and RCN-213 define two types of accessory commands:
1. Basic accessory command, used for switches and similar
2. Extended accessory command, used for signals, turntables and other, more complex devices

#### Command_t command: (basic..extended) ####
The `command` attribute indicates whether a basic or an extended accessory command was received.
All command stations can generate basic accessory commands, but only a few support extended accessory commands. The LENZ LZV101 (with Xpressnet V3.6) for example can't, but the OpenDCC command station can generate extended accessory commands.

Commands for accessory decoders contain fields that can be interpreted in different ways, depending on the specific decoder that is being implemented.

#### _1. Decoder based addressing (basic):_ ####

The most common type of accessory decoder is the switch decoder, which connects upto 4 switches. Such decoders understand basic accessory commands, and their fields can be interpreted as follows:

#### unsigned int decoderAddress: (0..511) ####
The address of the decoder (board). The NMRA standard defines 9-bits for decoder addresses, although some command stations (like the LENZ LZV101 with XpressNet V3.6) support only 8 bits (0..255).

#### uint8_t turnout: (1..4) ####
The `turnout` attribute tells which of the four switches is being targeted.

#### _2. Output based addressing (basic and extended):_ ####

#### unsigned int outputAddress: (1..2048) ####
Instead of addressing decoders (that connect multiple switches), it is also possible to directly address individual switches or other outputs (relays, signals, turntables etc). Output addresses are 11 bits in length, and consists basically of the concatenation of the `decoderAddress` and the `switch` attributes.

#### uint8_t position: (0..1) (basic) ####
Most switches can be positioned via two coils or servos. The "position" attribute tells which of the two coils is being targetted. Unfortunately different command stations made different choices regarding the interpretation of 0 or 1. According to RCN-213, a value 0 is used for "diverging"  tracks (red), whereas the value 1 is used for "straight" tracks (green). Some command stations use '-' for 0, or '+' for 1 to indicate the position.

#### uint8_t device: (1..8) (basic) ####
In case of relays or other on/off devices usage of the `turnout` and `position` attributes may seem artificial. In such cases the `device` attribute may be used instead.

#### uint8_t activate: (0..1) (basic) ####
Whether the output should be activated (on) or deactivated (off).

#### uint8_t signalHead: (0..255) (extended) ####
The value contained in the extended accessory command. The NMRA S-9.2.1 standard allocates 5 bits, allowing values between 0 and 31, whereas the RCN-213 standard allocates 8 bits, thus 0..255. Can be used to display different signal heads, but might also be used for turntables or other, more complex devices.

#### _RELATION BETWEEN ATTRIBUTES_ ####
In case of a basic accessory command:
* outputAddress (11 bits) = decoderAddress (9 bits) + turnout (2 bits)
* device (3 bits) = turnout (2 bits) + position (1 bit)
Since all attributes will get the correct values, the choice which attributes to use is upto the sketch writer.

#### void setMyAddress(unsigned int first, unsigned int last = 65535) ####
Should be initialised in setup() of the main sketch. This is the decoder address, not the switch address!
An Accessory Decoder may listen to one or multiple decoder addresses, for example if it supports more than four switches or skips uneven addresses. If the call includes a single parameter, that parameter represents the (single) address this decoder will listen to. If the call is made with two parameters, these parameters represent the range of addresses this decoder listens to.


#### uint8_t myMaster = Lenz; ####
Different command station manufacturers made different choices regarding the exact coding of the address bits within the DCC packet. See "support_accessory.cpp" for details. In many cases these differences can be neglected, unless the decoder address will also be used for other purposes, such as calculating CV values, or generating feedback / POM addresses. The `myMaster` attribute may be set by setup() of the main sketch to  `Lenz` (1), `OpenDcc` (2) or `Roco` (0) to deal with different command station behaviour. The default value is `Lenz`.

#### _Broadcast Address_ ####
The broadcast outputAddress is 2047.

___

## <a name="Loco"></a>The Loco Class ##

#### void setMyAddress(unsigned int first, unsigned int last = 65535) ####
In the main sketch `setup()` should call `setMyAddress`, to initialise the Loco object with the range of loco addresses it will listen to. If the call includes a single parameter, that parameter represents the (single) loco address this decoder will listen to. If the call includes two parameters, these parameters represent the range of loco addresses this decoder will listen to.

We analyse most commands, but no attempt is made to be complete. The focus is on those commands that may be useful for accessory decoders, that  listen to some loco commands to facilitate PoM. In addition, some functions are included that may be useful for safety decoders as well as function decoders (for switching lights within couches).

The following data can be obtained from the Loco class:
````
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

````
----

## <a name="CvAccess"></a>The CvAccess Class ##
The behaviour of the decoder is determined by the setting of certain Configuration Variables (CVs) Commands to access these variables can be send in Service Mode (SM = Programming Track) or in Programming on the Main (PoM) mode. This decoder supports both modes.

According to S-9.2.1, PoM supports 2 methods to access Configuration Variables (CVs): Short form and Long form. Of these, only the long form is implemented by this Library

According to S-9.2.3, Service Mode supports 4 methods to access Configuration Variables (CVs): Direct Configuration, Address-Only, Physical Register, and Paged Addressing.
Of these, only Direct Configuration is implemented by this Library

There are several conditions to be satisfied before CV access commands can be accepted. In SM, a reset packet must be received before a CV access command may be accepted, and timeouts must be obeyed. Only the second command may be acted upon. This library ensures that all these conditions are met.

To determine the value of a specific CV, a Command Station usually sends 8 consecutive verify bit commands, one to check each individual bit of the 8-bit variable. After that, the Command Station may issue a verify byte command, to ensure no errors occurred.

The following data can be obtained from the CvAccess class:
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
___
___


## Usage ##
The main sketch should declare the following objects:
```
extern Dcc dcc;
extern Accessory accCmd;
extern Loco locoCmd;
extern CvAccess cvCmd;
```

`setup()` should call `dcc.attach(dccPin, ackPin)`.

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
  dcc.attach(dccPin);
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  Serial.println();
  // The decoder can listen to one, or a range of addresses. In this example
  // we display details for all decoders with addresses between 1 and 100.
  // Note: One decoder address supports 4 switches   
  accCmd.setMyAddress(1, 100);    // Decoder 100 is switch 397..400
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
- On traditional ATMega processors: Timer 2 is used
- On novel ATMega processors: TCB0 is used (another TCB timer may be selected by uncommenting the related define in [sup_isr_MegaCoreX_DxCore.h](src/sup_isr_MegaCoreX_DxCore.h)
- A free to chose interrupt pin (dccpin) for the DCC input signal
- A free to chose digital output pin for the DCC-ACK signal. Only needed if SM programming is required.


## Reference ##
This code follows the NMRA DCC standard S9.2 S9.2.1 and S9.2.3 as well as RCN211-RCN214 (in German, by [http://railcommunity.org](http://railcommunity.org)), which include more detailed descriptions as well as the differences in accessory decoder addresses.


# Support pages #
- [Performance on MegaCoreX and DxCore microcontrollers](extras/Performance_MegacoreX.md)
- [History and motivation behind this library](extras/History_Differences.md)
