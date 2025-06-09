# Supported boards #

This library is compatible with various processors and boards. For this reason, different "drivers" have been developed.

In version 2 of the library, the drivers were renamed and moved to a separate directory called "variants". In addition, a *generic* driver was developed that has the advantage of running on all possible boards, but the disadvantage of possibly having suboptimal performance. Therefore, to achieve optimal DCC decoding quality and efficiency, specific drivers that exploit peripherals available on modern processors have been developed. Additional drivers for specific processors may be added in the future, to further improve DCC decoding results.

Regardless of the selected driver, the following peripherals are always needed:
- A free-to-choose interrupt pin (dccpin) for the DCC input signal
- A free-to-choose output pin for the DCC-ACK signal. This is only needed if SM programming is required.

### Generic.h ###

This driver should compile and run on all boards that can be installed via the Arduino Board Manager. The driver uses the functions `attachInterrupt()`, `digitalPinToInterrupt()` and `micros()`. Any board that can perform these functions should be able to run this library.
From a technical standpoint, this driver is comparable to the NmraDcc library. A change in the state of a pin triggers an Interrupt Service Routine (ISR), which calls the `micros()` function and performs a comparison with the value stored during the previous interrupt. If the difference falls within certain ranges, it is concluded that the bit should be a 0 or a 1.

However, this approach has several disadvantages. First, if there are interrupts from other peripherals, such as the UART, I2C, or the `millis()` timer, the execution of the ISR may be delayed. Second, `attachInterrupt()` takes nonnegligible time. On traditional ATmega processors, it takes 3 µs before and 2 µs after the real ISR code. On newer 8-bit processors, these times may be considerably larger. Third, the resolution of `micros()` is 4 µs, and executing this call may take multiple µs. Together, these factors may lead to imprecise pulse length measurements and incorrect DCC signal decoding. These factors are particularly notable with processors that run at speeds of 24 MHz or lower.

### Mega.h ###

For traditional ATmega processors, such as those used on Arduino UNO, NANO and MEGA boards, we use an approach in which a change in the DCC input signal triggers an interrupt service routine (ISR). Unlike the generic driver, however, this code does not measure time using the `micros()` function. Instead, it sets a hardware timer (`Timer2`) to 77 µs. Once this timer fires, the value of the DCC input signal is read to determine whether it is a 0 or a 1. This approach is identical to that of the OpenDCC V2 decoders developed by Wolfgang Kufer.

This approach offers more precise and efficient DCC decoding at the expense of an extra hardware timer.

Note that this driver can also be used in conjunction with MegaCore boards, that support processors like the ATMega16.

### MegaCoreX_DxCore ###

For newer ATMegaX processors, such as the ATmega4808 and ATmega4809 (Arduino Nano Every, MegaCoreX), as well as the AVR Dx series (DxCore), we exploit the new Event System Peripheral to offload certain tasks to the hardware. Additionally, we use one of the timers in frequency capture mode. This approach considerably improves the quality and efficiency of the DCC decoding. The quality is improved due to the fact that the interval between changes in the DCC input signal is measured in hardware, and is therefore made very precise and in accordance with the timing requirements as defined by the Rail Community (RCN-210). In fact, DCC decoding quality does not degrade, even when other tasks frequently generate interrupts, such as communication buses for feedback signals. Efficiency improves because more functions are performed in hardware, thereby reducing the CPU load. See [Performance on MegaCoreX and DxCore microcontrollers](Performance_MegacoreX.md) for details.

To exploit these hardware peripherals, the MegaCoreX or DxCore boards should be installed by the Arduino IDE:
- https://github.com/MCUdude/MegaCoreX
- https://github.com/SpenceKonde/DxCore

TCB0 is the default timer. This can be changed by setting  in the [MegaCoreX_DxCore.h](../src/variants/MegaCoreX_DxCore.h) file one of the following `#defines`: `DCC_USES_TIMERB1`, `DCC_USES_TIMERB2` or `DCC_USES_TIMERB3`.

When using an ATMegaX 4809 processor on an Arduino Nano Every with the "standard" Arduino megaAVR board selected in the Arduino IDE instead of the MegaCoreX board, a decoding approach similar to that used with traditional ATmega processors is employed: a change in the DCC input signal triggers an ISR, which sets a timer. Once the timer fires, the value of the DCC input pin is read.
Note that using a "standard" Arduino Mega AVR board results in considerably more overhead than we are accustomed to with traditional ATmega processors. Therefore, switching to a MegaCoreX board is strongly encouraged.

## Tested board and processors ##
The library has been tested on the following boards and processors:
- Arduino UNO / Nano (ATmega 328)
- Arduino Mega  (ATmega 2560)
- Arduino UNO Rev4 (Renesas - RA4M1)
- Arduino Zero (SAMD - ATSAMD21G18A)
- Arduino Nano Every (4808/4809)
- ATMega16A
- DxCore (Many AVR DA processors)
- STM32: Nucleo F411RE, F446RE, F439ZI
- ESP32
- RP2040

### Acknowledgements ###
This library could not have been developed without the inspiring ideas and the [feedback](https://forum.opendcc.de/viewtopic.php?f=17&t=8141) obtained from members of the OpenDCC forum, in particular Frank Schumacher and Wolfgang Kufer.
In addition, I would like to acknowledge the contributions made by Laurens Roekens, especially for the addition of new functions and proposals to support the ESP32 and RailCom.  
