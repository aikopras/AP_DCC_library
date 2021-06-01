//******************************************************************************************************
//
//                       Testing the CV-POM part of the AP_DCC_library
//
// purpose:   This sketch shows what information is received from CV Access Commands
//            Results are displayed on the serial monitor
// author:    Aiko Pras
// version:   2021-06-01 V1.0 ap initial version
//
// usage:     This sketch should declare the following objects:
//            - extern Dcc           dcc;     // The main DCC object
//            - extern Loco          locoCmd;  // To set the Loco address (for PoM)
//            - extern CvAccess      cvCmd;    // To retrieve the data from pom and sm commands
//
//            Setup() should call dcc.begin(dccPin, ackPin).
//            - dccPin is the interrupt pin for the DCC signal,  
//            - ackPin is an optional parameter to specify the pin for the DCC Acknowledgement signal 
//            The main loop() should call dcc.input() as often as possible. If there is input,
//            dcc.cmdType tells what kind of command was received.
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
const uint8_t ackPin = 8;        // Digital Pin 8 on the ATMega2560 (is PH5)

extern Dcc dcc;                  // This object is instantiated in DCC_Library.cpp
extern Loco locoCmd;             // To retrieve the data from loco commands  (7 & 14 bit)
extern CvAccess cvCmd;           // To retrieve the data from pom and sm commands

// For test purposes, define an array with configuration variables
// Note that array numbering starts from 0, and CV numbering from 1
uint8_t myCvs[] = {1,2,3,4,5,6,7,8,9};

// Define constants to differentiate between SM and PoM
const uint8_t SM = 1;
const uint8_t PoM = 2;


void setup() {
  dcc.begin(dccPin, ackPin);
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  Serial.println("");
  // For testing, the following variables can be changed
  locoCmd.myLocoAddressFirst = 5000;
  locoCmd.myLocoAddressLast = 5000;
  delay(1000);
  Serial.println("Test DCC lib - Configuration Variable Access Commands");
}

  
void cv_operation(const uint8_t op_mode) {
  // SM:  op_mode = 1
  // PoM: op_mode = 2
  uint8_t index = cvCmd.number - 1; // CVs start with 1, the array index with 0
  Serial.print("Received CV Number: ");
  Serial.println(cvCmd.number);
  Serial.print("Received CV Value: ");
  Serial.println(cvCmd.value);
  // Ensure we stay within the CV array bounds
  if (index < sizeof(myCvs)) {
    Serial.print("Old CV value = "); 
    Serial.println(myCvs[index]);
    switch(cvCmd.operation) {
      
      case CvAccess::verifyByte :
        if (myCvs[index] == cvCmd.value) {
          Serial.println("Verify Byte Command - Bytes are equal");
          // In SM we send back a DCC-ACK signal, in PoM mode a railcom reply (not implemented) 
          if (op_mode == SM)  {dcc.sendAck();}
        }
        else Serial.println("Verify Byte Command - Bytes are unequal");
      break;
        
      case CvAccess::writeByte :
        Serial.println("Write Byte Command");
        myCvs[index] = cvCmd.value;
        if (op_mode == SM) dcc.sendAck();
      break;
        
      case CvAccess::bitManipulation :
        Serial.print("Bit Manupulation");
        Serial.print(", Bitposition = ");  Serial.print(cvCmd.bitposition);
        Serial.print(", Bitvalue = ");     Serial.println(cvCmd.bitvalue);
        if (cvCmd.writecmd) {
          Serial.print("Write Command");
          myCvs[index] = cvCmd.writeBit(myCvs[index]);
          Serial.print(". New CV value = "); Serial.println(myCvs[index]);
          if (op_mode == SM) dcc.sendAck();
        } 
        else { // verify bit
          Serial.print("Verify Command");
          if (cvCmd.verifyBit(myCvs[index])) {
            Serial.print(". Bits are equal");
            if (op_mode == SM) dcc.sendAck();
          }
          else Serial.print(". Bits are unequal");
        }
        Serial.println();
      break;
    }
  }
  Serial.println();
}



void loop() {
  if (dcc.input()) {

    switch (dcc.cmdType) {

      case Dcc::ResetCmd :
        Serial.println("Reset Command");
      break;

      case Dcc::MyPomCmd :
        Serial.println("Programming on Main command:");
        cv_operation(PoM);
      break;

      case Dcc::SmCmd :
       Serial.println("Service mode command:");
       cv_operation(SM);
      break;

    }
  } 
}
