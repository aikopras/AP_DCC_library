//******************************************************************************************************
//
//                       Testing the Accesory Decoder part of the AP_DCC_library
//
// purpose:   This sketch shows what information is received from Accesory Decoder Commands
//            Results are displayed on a LCD display
// author:    Aiko Pras
// version:   2021-06-01 V1.0 ap initial version
//
// usage:     This sketch should declare the following objects:
//            - extern Dcc           dcc;     // The main DCC object
//            - extern Accessory     accCmd;  // To retrieve the data from accessory commands
//            Note the 'extern' keyword, since these objects are instantiated in DCC_Library.cpp
//
//            Setup() should call dcc.attach(dccPin). dccPin is the interrupt pin for the DCC signal
//            The main loop() should call dcc.input() as often as possible. If there is input,
//            dcc.cmdType tells what kind of command was received.
//            In this sketch we react on MyAccessoryCmd and AnyAccessoryCmd).
//
//
// hardware:  - Timer 2 or TCB0 is used
//            - a free to chose interrupt pin (dccpin) for the DCC input signal
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#include <Arduino.h>
#include <AP_DCC_library.h>
#include <LiquidCrystal.h>
// For the ATMEGA16 - Mightycore we need:
// LiquidCrystal lcd(4, 5, 6, 0, 1, 2, 3);
// For the ATMEGA2560 - Lift decoder we need:
LiquidCrystal lcd(53, 51, 50, 12, 11, 9, 10);

//const uint8_t dccPin = 11;       // Pin 11 on the ATMega 16 is INT1
const uint8_t dccPin = 20;       // Pin 20 on the ATMega2560 - Liftdecoder is INT1

extern Dcc dcc;                  // This object is instantiated in DCC_Library.cpp
extern Accessory accCmd;         // To retrieve data from accessory commands

void setup() {
  dcc.attach(dccPin);
  lcd.begin(16,2);
  lcd.print("Test DCC lib");
  // For testing, the following variables can be changed
  accCmd.myMaster = Lenz;
  // Set Accessory address. We may also specify an address range.
  // Note: my decoder address = output (switch) address / 4
  accCmd.setMyAddress(24);    // Decoder 24 is switch 97..100
  delay(1000);
}


void loop() {
  if (dcc.input()) {
    switch (dcc.cmdType) {

      case Dcc::MyAccessoryCmd :
        lcd.clear();
        lcd.print(accCmd.decoderAddress);
        lcd.print(" (my)");
        lcd.setCursor(11, 0);
        lcd.print(accCmd.turnout);
        lcd.setCursor(15, 0);
        if (accCmd.command == Accessory::basic) lcd.print('B');
          else lcd.print('E');
        lcd.setCursor(0, 1);
        lcd.print(accCmd.outputAddress);
        lcd.setCursor(11, 1);
        if (accCmd.position == 1) lcd.print('+');
          else lcd.print('-');
        lcd.setCursor(15, 1);
        if (accCmd.activate) lcd.print('A');
      break;

      case Dcc::AnyAccessoryCmd :
        lcd.clear();
        lcd.print(accCmd.decoderAddress);
        lcd.print(" (any)");
      break;

      default:
      break;
    }
  }
}
