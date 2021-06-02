# AP_DCC_Library #

This Arduino library decodes (NMRA) Digital Command Control (DCC) messages. It is primarily intended for Accesoory Decoders.

## History ##
This software is a further development of OpenDecoder 2, as developed by W. Kufer. It has been rewritten, such that it can be used for Arduino (Atmel AVR) environments. From a functional point of view, this code is comparable to (a subset of) the NMRA-DCC library. However,it is believed that this code is easier to adapt and modify to personal needs. This Library has been tested on several Arduino boards, as well as on other boards with ATMega16 and 2560 processors. 
Like the OpenDecoder 2 Software and version 1.2 of the NMRA-DCC library, this code uses a separate Timer (Timer2) to decode the DCC signal. However, as opposed to some other code bases, the DCC input (interrupt pin) can be freely chosen. In contrast to  to the NMRA-DCC version 2 library, this code doesn't use callback functions, in an attempt to improve the readability of the main loop for cases where the action to be  taken should not depend on the trigger source. For example, the code for setting a switch should be the same, irrespective whether the trigger is a DCC-message or a button push. To improve readability, mainainability and extendibility, many comments were included and an attempt is made to structure the code more clearly.

## Reference ##
This code follows the NMRA DCC standard S9.2 S9.2.1 and S9.2.3 as well as RCN211-RCN214 (in German, by [http://railcommunity.org](http://railcommunity.org)), which include more detailed descriptions as well as the differences in accesory decoder addresses.

## Usage ##
The main sketch should declare the following objects:
- extern Dcc dcc;
- extern Accessory accCmd;
- extern Loco locoCmd;
- extern CvAccess cvCmd;

Setup() should call dcc.begin(dccPin, ackPin).

The main loop() should call dcc.input() as often as possible. If there is input, dcc.cmdType tells what kind of command was received (such as MyAccessoryCmd or MyLocoF0F4Cmd).
Note that command stations will periodically retransmit certain commands, to ensure that, even in noisy environments, commands will be received. Such retransmissions will be filtered within this library. The main sketch therefore does not receive retransmissions.



```
#include <Arduino.h>
#include <AP_DCC_library.h>

const uint8_t dccPin = 20;       // The DCC input signal is connected to Pin 20 of the ATMega2560 (INT1)
const uint8_t ackPin = 8;        // The DCC Acknowledge signal is connected to digital Pin 8 of the ATMega2560 (PH5)
                                 // DCC Acknowledgements are needed for Service Mode (SM) programming

extern Dcc dcc;                  // This is the main library object, and instantiated in DCC_Library.cpp
extern Accessory accCmd;         // All results from accessory commands can be found in this object
extern Loco locoCmd;             // All results from loco commands can be found in this object
extern CvAccess cvCmd;           // All results from CV-Access commands (PoM and SM) can be found in this object

void setup() {
  dcc.begin(dccPin, ackPin);     // ackPin is optional, and may be omitted if SM programming is not needed 
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  Serial.println();
  // For testing, the following variables can be changed
  // Different Master Stations use different numbering approaches for accessory decoders
  accCmd.myMaster = Accessory::Lenz;
  // The decoder can listen to one, or a range of addresses.
  accCmd.myDecAddrFirst = 24;    // Note: my decoder address = output (switch) address / 4
  accCmd.myDecAddrLast  = 24;    // Decoder 24 is switch 97..100
  //
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
This is the main class to receive and analyse DCC messages. It has three methods: begin(), end() and input(). 

#### void begin(uint8_t dccPin, uint8_t ackPin=255) ####
Starts the timer T2 and initialises the DCC Interrupt Service Routine (ISR). 
- dccPin is the interrupt pin for the DCC signal. Basically any available AVR interrupt pin may be selected.  
- ackPin is an optional parameter to specify the pin for the DCC Service Mode Acknowledgement signal (of 6ms). If this parameter is omitted, no DCC acknowledegements will be generated. 

The interrupt routine that reads the value of that pin must be fast. The traditional Arduino `digitalRead()` function is relatively slow, certainly when compared to direct port reading, which can be done (for example) as follows: `DccBitVal = !(PIND & (1<<PD3);`
However, direct port reading has as disadvantage that the port and mask should be hardcoded, and can not be set by the main sketch, thus the user of this library. Therefore we take a slightly slower approach, and use a variable called `portInputRegister`, which points to the right input port, and `bit`, which masks the selected input port. To use `portInputRegister` and `bit`, we basically split the Arduino `digitalRead()` function into: 1) an initialisation part, which maps dccPin to `portInputRegister` and `bit`. This part may be slow. 2) The actual reading from the port, which is fast.

#### void end(void) ####
Stops the timer and DCC ISR. This is needed before a decoder can be soft-reset. 

#### bool input(void) ####
Analyze the DCC message received. Returns true, if a new message is received.
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
Create a 6ms DCC ACK signal, which is needed for Service Mode programming


## The ACCESSORY Class ##
The NMRA Standard 9.2.1 and RCN-213 define two types of accessory commands:
1. Basic accessory command, used for switches and similar
2. Extended accessory command, used for signals, turntables and other, more complex devices

#### command (basic..extended) ####
The `command` attribute indicates whether a basic or an extended accessory command was received.
All command stations can generate basic accessory commands, but only a few support extended accesory commands. The LENZ LZV101 (with Xpressnet V3.6) for example can't, but the OpenDCC command station can generate extended accesory commands.

Commands for accessory decoders contain fields that can be interpreted in different ways, depending on the specific decoder that is being implemented.
 
1. Decoder based addressing (basic):
The most common type of accessory decoder is the switch decoder, which connects upto 4 switches. Such decoders understand basic accesory commands, and their fields can be interpreted as follows:
  #### decoderAddress (0..511) ####
    The address of the decoder (board). The NMRA standard defines 9-bits for decoder addresses, although some command stations (like the LENZ LZV101 with XpressNet V3.6) support only 8 bits (0..255).
  #### turnout (1..4) ####
  The `turnout` attribute tells which of the four switches is being targetted.
 
2. Output based addressing (basic and extended)
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

An Accesory Decoder may listen to multiple decoder addresses, for example if it supports more than four switches or skips uneven addresses. After startup, the Accessory object should therefore be initialised with the range of accessory addresses it will listen too. For that purpose set `myDecAddressFirst` and `myDecAddressLast`.

