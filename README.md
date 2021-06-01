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
The main loop() should call dcc.input() as often as possible. If there is input,  dcc.cmdType tells what kind of command was received (such as MyAccessoryCmd or MyLocoF0F4Cmd).
Note that command stations will periodically retransmit certain commands, to ensure that, even in noisy environments, commands will be received. Such retransmissions will be filtered within this library. The main sketch will not receive retransmissions.

#include "Arduino.h"
#include "AP_DCC_library.h"

const uint8_t dccPin = 20;       // Pin 20 on the ATMega2560 - Liftdecoder is INT1

extern Dcc dcc;                       // This object is instantiated in DCC_Library.cpp
extern Accessory accCmd;

void setup() {
  dcc.begin(dccPin);
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  Serial.println("");
  // For testing, the following variables can be changed
  accCmd.myMaster = Accessory::Lenz;
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

## hardware ##
- Timer 2 is used
- a free to chose interrupt pin (dccpin) for the DCC input signal
- a free to chose digital output pin for the DCC-ACK signal



## The DCC Class class ##
This is the main class to receive and analyse DCC messages. It has three methods: begin(), end() and input(). 

- ### begin(uint8_t dccPin, uint8_t ackPin=255) ###
Starts the timer T2 and initialises the DCC Interrupt Service Routine (ISR). 
- dccPin is the interrupt pin for the DCC signal. Basically any available AVR interrupt pin may be selected.  
- ackPin is an optional parameter to specify the pin for the DCC Service Mode Acknowledgement signal (of 6ms). If this parameter is omitted, no DCC acknowledegements will be generated. 

The interrupt routine that reads the value of that pin must be fast. The traditional Arduino digitalRead() function is relatively slow, certainly when compared to direct port reading, which can be done (for example) as follows: DccBitVal = !(PIND & (1<<PD3);
However, direct port reading has as disadvantage that the port and mask should be hardcoded, and can not be set by the main sketch, thus the user of this library. Therefore we take a slightly slower approach, and use a variable called "portInputRegister", which points to the right input port, and "bit", which masks the selected input port. To use "portInputRegister" and "bit", we basically split the Arduino digitalRead() function into: 1) an initialisation part, which maps dccPin to "portInputRegister" and "bit". This part may be slow. 2) The actual reading from the port, which is fast.

- ### void end(void) ###
Stops the timer and DCC ISR. This is needed before a decoder can be soft-reset. 

- ### bool input(void) ###
Analyze the DCC message received. Returns true, if a new message is received.
The main loop() should call dcc.input() as often as possible. If there is input, dcc.cmdType informs main what kind of command was received. The following command types are possible:
- Unknown - Start value, but should never be returned
I- gnoreCmd - Command should be ignored
- ResetCmd - We have received a reset command
- SomeLocoMovesFlag - Loco speed > 0, but not for this decoder
- MyLocoSpeedCmd - Loco speed and direction command, for this decoder
- MyEmergencyStopCmd - Loco Emergency stop, for this decoder
- MyLocoF0F4Cmd - F0..F4  command, for this decoder address(es)
- MyLocoF5F8Cmd - F5..F8  command, for this decoder address(es)
- MyLocoF9F12Cmd - F9..F12 command, for this decoder address(es)
- MyLocoF13F20Cmd - F13..F20 command, for this decoder address(es)
- MyLocoF21F28Cmd - F21..F28 command, for this decoder address(es)
- MyLocoF29F36Cmd - F29..F36 command, for this decoder address(es)
- MyLocoF37F44Cmd - F37..F44 command, for this decoder address(es)
- MyLocoF45F52Cmd - F45..F52 command, for this decoder address(es)
- MyLocoF53F60Cmd - F53..F60 command, for this decoder address(es)
- MyLocoF61F68Cmd - F61..F68 command, for this decoder address(es)
- AnyAccessoryCmd - Accessory command, but not for this decoder address(es)
- MyAccessoryCmd - Accessory command, for this decoder address(es)
- MyPomCmd - Programming on the Main (PoM)
- SmCmd - Programming in Service Mode (SM = programming track)

- ### void sendAck() ###
Create a 6ms DCC ACK signal (needed for Service Mode)





// retransmit all messages, so the main application has to check and filter for retransmissionsThe RSbusHardware class initialises the USART for sending the RS-bus messages, and the Interrupt Service Routine (ISR) used for receiving the RS-bus pulses send by the master to poll each decoder if it has data to send. Most AVRs have a single USART, but the ATMega 162, 164 and 644 have two, while the 1280 and 2560 have four USARTs. By specifying the tx_pin, the choice which USART will be used is made implicitly.

- ### attach() ###
Should be called at the start of the program to connect the TX pin to the USART and the RX pin to the RS-bus Interrupt Service Routine (ISR).

- ### detach() ###
Should be called in case of a (soft)reset of the decoder, to stop the ISR

- ### checkPolling() ###
Should be called as often as possible from the program's main loop. The RS-bus master sequentially polls each decoder. After all decoders have been polled, a new polling cyclus will start. checkPolling() maintains the polling status.

- ### masterIsSynchronised ###
A flag maintained by checkPolling() and used by objects from the RSbusConnection class to determine if the start of the (first) polling cyclus has been detected and the master is ready to receive feedback data.


## The RSbusConnection class ##
For each address this decoder will use, a dedicated RSbusConnection object should be created by the main program. To connect to the master station, each RSbusConnection object should start with sending all 8 feedback bits to the master. Since RS_bus messages carry only 4 bits of user data (a nibble), the 8 bits will be split in 4 low and 4 high bits and send in two consequetive messages.

- ### address ###
The address used by this RS-bus connection object. Valid values are: 1..128

- ###  needConnect  ###
A flag indicating to the user that a connection should be established to the master station. If this flag is set, the main program should react with calling send8bits(), to tell the master the value of all 8 feedback bits. Note that this flag may be ignored if we always use send8bits() and send all 8 feedback bits, and never use send4bits() for sending only part of our feedback bits.

- ### type ###
A variable that specifies the type of decoder. The default value is Switch, but this may be changed to Feedback. Note that RS-bus messages do contain two bits to specify this decoder type, but there is no evidence that these bits are actually being used by the master. Therefore the "type" can be set via this library for completeness, but an incorrect value will (most likely) not lead to any negative effect.

- ### send4bits() ###
Sends a single 4 bit message (nibble) to the master station. We have to specify whether the high or low order bits are being send. Note that the data will not immediately be send, but first be stored in an internal FIFO buffer until the address that belongs to this object is called by the master.

- ### send8bits() ###
Sends two 4 bit messages (nibbles) to the master station. Note that the data will not immediately be send, but first be stored (as two nibbles) in an internal FIFO buffer until the address that belongs to this object is called by the master.

- ### checkConnection() ###
Should be called as often as possible from the program's main loop. It maintains the connection logic and checks if data is waiting in the FIFO buffer. If data is waiting, it checks if the USART and RS-bus ISR are able to accept that data. The RS-bus ISR waits till its address is being polled by the master, and once it gets polled sends the RS-bus message (carrying 4 bits of feedback data) to the master.

# Details #

    enum Decoder_t { Switch, Feedback };
    enum Nibble_t  { HighBits, LowBits };

    class RSbusHardware {
      public:
        uint8_t masterIsSynchronised;

        RSbusHardware();
        void attach(int tx_pin, int rx_pin);
        void detach(void);
        void checkPolling(void);
    }


    class RSbusConnection {
      public:
        uint8_t address;
        uint8_t needConnect;
        Decoder_t type;

        RSbusConnection();
        void send4bits(Nibble_t nibble, uint8_t value);
        void send8bits(uint8_t value);
        void checkConnection(void);
    }

# Printed Circuit Boards (PCBs) #
The schematics and PCBs are available from my EasyEda homepage [EasyEda homepage](https://easyeda.com/aikopras),
* [RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-tht)
* [SMD version of the RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-smd)
