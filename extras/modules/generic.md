# Generic Driver (`generic.h`)

## Purpose
The *Generic* driver implements DCC signal reception using only standard Arduino functions.  
It contains **no hardware-specific code**, making it portable across a wide range of microcontrollers supported by the Arduino IDE.

## Operating Principle
- Uses a **single GPIO interrupt** triggered on *either* a rising **or** falling edge of the DCC signal.
- On each edge, the current time is read using `micros()` (typical resolution: 4 µs).
- The time difference from the previous edge determines the DCC bit value:
  - **1-bit:** interval between ~104–128 µs (±margin)  
  - **0-bit:** interval between ~180–232 µs (±margin)
- Margins are applied to account for interrupt latency and timing inaccuracy.
- The polarity of the DCC signal is irrelevant. The driver starts on one edge polarity; if an unexpected interval is detected (typically caused by reversed DCC polarity), it automatically switches to the opposite polarity to resynchronize.

## Data Flow
1. The ISR stores received bits in a temporary buffer (`dccrec.tempMessage`).
2. When a full DCC packet is assembled, it is copied into `dccMessage.data`.
3. The `dccMessage.isReady` flag is set to indicate that a complete message is available.

## Hardware Requirements
- Any GPIO pin capable of generating a hardware interrupt.
- No dedicated hardware peripherals required (e.g., no timers, no RMT, no DMA).
- Optional: one output pin for Service Mode acknowledgements.

## Limitations
- **No zero-bit stretching support:** Only nominal 0-bit lengths are recognized.
- **No half-bit capture:** Processes only full bits, ignoring mid-bit transitions.
- **Timing resolution:** Dependent on `micros()` precision (4 µs typical) and ISR latency; may be insufficient for RCN210 compliance on slower MCUs.
- **Jitter sensitivity:** On busy MCUs or systems with variable interrupt latency, timing margins may need to be tuned.
- **RailCom:** Jitter may prevent reliable RailCom detection or usage.

## Use Cases
- Good for portability and simple integration, without RailCom.
- Suitable when maximum cross-platform compatibility is more important than strict NMRA/RCN timing compliance.
- Recommended for MCUs without advanced hardware capture peripherals.
