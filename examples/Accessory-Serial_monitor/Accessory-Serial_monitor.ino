//******************************************************************************************************
//
//                       Testing the Accesory Decoder part of the AP_DCC_library
//
// purpose:   This sketch shows what information is received from Accesory Decoder Commands
//            Results are displayed on the serial monitor
// author:    Aiko Pras
// version:   2021-06-01 V1.0 ap initial version
//
// usage:     This sketch should declare the following objects:
//            - extern Dcc           dcc;     // The main DCC object
//            - extern Accessory     accCmd;  // To retrieve the data from accessory commands
//
//            Setup() should call dcc.begin(dccPin). dccPin is the interrupt pin for the DCC signal
//            The main loop() should call dcc.input() as often as possible. If there is input,
//            dcc.cmdType tells what kind of command was received.
//            In this sketch we react on MyAccessoryCmd and AnyAccessoryCmd).
//
//
// hardware:  - Timer 2 is used
//            - a free to chose interrupt pin (dccpin) for the DCC input signal
//            - a free to chose digital output pin for the DCC-ACK signal
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#include <Arduino.h>
#include <AP_DCC_library.h>

//const uint8_t dccPin = 11;       // Pin 11 on the ATMega 16 is INT1
const uint8_t dccPin = 20;       // Pin 20 on the ATMega2560 - Liftdecoder is INT1

extern Dcc dcc;                  // This object is instantiated in DCC_Library.cpp
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
