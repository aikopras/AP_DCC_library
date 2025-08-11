# Mega Driver (`Mega.h`)

## Purpose
The *Mega* driver implements DCC signal reception optimized for traditional ATmega microcontrollers
(e.g., ATmega16, ATmega328, ATmega2560, and similar), using **hardware timers** for accurate mid-bit sampling.
This driver is designed for boards using the standard Arduino core or MightyCore, and relies on peripherals
available on classic AVR devices.

## Operating Principle
- Uses **two interrupts**:
  1. **External interrupt (INTx)** on the rising edge of the DCC signal to start a timer.
  2. **Timer2 overflow interrupt** to sample the DCC signal **77 µs** after the edge.
- The sampling delay (77 µs) is chosen to occur at roughly 3/4 of a nominal "1" bit duration,
  allowing reliable detection of whether the bit is `1` or `0`.
- Sampling is done via direct port access for speed, avoiding the overhead of `digitalRead()`.
- The ISR then assembles bits into bytes and full DCC packets.

### Timing Diagram

```
                     |<-----116us----->|

     DCC 1: _________XXXXXXXXX_________XXXXXXXXX_________
                     ^-INT1
                     |----77us--->|
                                  ^Timer2 ISR: reads zero

     DCC 0: _________XXXXXXXXXXXXXXXXXX__________________
                     ^-INT0
                     |----------->|
                                  ^Timer2 ISR: reads one


```

## Data Flow
1. **INTx ISR** starts Timer2 with a preset count so it overflows after ~77 µs.
2. **Timer2 ISR** samples the DCC line:
   - Low at sample time → `1` bit.
   - High at sample time → `0` bit.
3. Bits are stored in `dccrec.tempMessage` until a full packet is received.
4. Completed packet is copied to `dccMessage.data` and `dccMessage.isReady` is set.

## Hardware Requirements
- **One external interrupt pin** (INT0–INT5 depending on board).
- **Timer2** (8-bit) available for mid-bit sampling.
- Optional: ADC for **voltage detection** (e.g., occupancy detection decoders).

## Implementation Details
- **Direct Port Access**: The driver maps the selected pin to its port and bitmask,
  allowing single-instruction reads inside the ISR.
- **GPIOR Registers**: Used for `dccrecState` and `tempByte` on supported AVRs for extra speed.
- **ADC Triggering**: If `VOLTAGE_DETECTION` is defined, the Timer2 ISR can start an ADC conversion
  when a `0` bit is detected and the ADC is ready.

## Limitations
- Requires **Timer2**; not compatible if Timer2 is already in use.
- Only works on ATmega devices with INTx pins and Timer2.
- **No half-bit detection** — relies on sampling at a fixed delay after edges.
- Timing accuracy depends on crystal frequency and prescaler choice.

## Use Cases
- Recommended for ATmega-based decoders needing **precise DCC timing**.
- Works well in occupancy detectors where ADC sampling is needed.
