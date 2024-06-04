//******************************************************************************************************
//
// file:     sup_isr_assemble_packet.h
// purpose:  Include file for the various device specific sup_isr_XXX.cpp files
//           Implements the code to assemble bits to a complete message
//           This code is independent of a specific type of hardware
//
//******************************************************************************************************
  dccrec.bitCount++;

  switch( dccrecState )
  {
  // According to NMRA standard S9.2, a packet consists of:
  // - Preamble
  // - Packet Start Bit
  // - Address Data Byte:
  // - Data Byte Start Bit + Data Byte [0 or more times]
  // - Packet End Bit


  case WAIT_PREAMBLE:
    // The preamble to a packet consists of a sequence of "1" bits.
    // A digital decoder must not accept as a valid, any preamble
    // that has less then 10 complete one bits
    if( DccBitVal )                                   // a "1" bit is received
    {
      if( dccrec.bitCount > 10 )
        dccrecState = WAIT_START_BIT;
    }
    else
    {
      dccrec.bitCount = 0;                            // not a valid preamble.
    }
    break;

  case WAIT_START_BIT:
    // The packet start bit is the first bit with a value of "0"
    // that follows a valid preamble. The packet start bit terminates the preamble
    // and indicates that the next bits are an address data byte
    if( !DccBitVal )                                  // a "0" bit is received
    {
      dccrecState = WAIT_DATA;
      dccrec.tempMessageSize = 0;
      // Initialise all fields
      uint8_t i;
      for(i = 0; i< MaxDccSize; i++ )
        dccrec.tempMessage[i] = 0;
      dccrec.bitCount = 0;
      tempByte = 0;

    }
    break;

  case WAIT_DATA:
  //==================
    tempByte = (tempByte << 1);                       // move all bits left
    if (DccBitVal) tempByte |= 1;                     // add a "1"
    if( dccrec.bitCount == 8 )                        // byte is complete
    {
      if(dccrec.tempMessageSize == MaxDccSize )       // Packet is too long - abort
      {
        dccrecState = WAIT_PREAMBLE;
        dccrec.bitCount = 0;
      }
      else
      {
        dccrecState = WAIT_END_BIT;                  // Wait for next byte or end of packet
        dccrec.tempMessage[dccrec.tempMessageSize++ ] = tempByte;
      }
    }
    break;

  case WAIT_END_BIT:
    // The next bit is either a Data Byte Start Bit or a Packet End Bit
    // Data Byte Start Bit: precedes a data byte and has the value of "0"
    // Packet End Bit: marks the termination of the packet and has a value of "1"
    if( DccBitVal ) // End of packet?
    {
      // Complete packet received and no errors
      // If somewhere in the future Railcom feedback will be implemented, this could be
      // the place to start a timer that determines the exact moment a UART should start
      // sending the RailCom feedback data.
      uint8_t i;
      uint8_t bytes_received;
      bytes_received = dccrec.tempMessageSize;
      for (i=0; i < bytes_received; i++) {
        dccMessage.data[i] = dccrec.tempMessage[i];
        }
      dccMessage.size = bytes_received;
      dccrecState = WAIT_PREAMBLE;
      // tell the main program we have a new valid packet
      noInterrupts();
      dccMessage.isReady = 1;
      interrupts();
    }
    else  // Get next Byte
    dccrecState = WAIT_DATA;
    dccrec.bitCount = 0;                              // prepare for the next byte
    tempByte = 0;
  }
