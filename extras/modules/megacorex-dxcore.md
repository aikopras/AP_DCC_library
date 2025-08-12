# MegaCoreX / DxCore Driver (`MegaCoreX_DxCore.h`)

## Purpose
The *MegaCoreX / DxCore* driver implements **high-performance** DCC reception for modern AVR microcontrollers, such as:

* **ATmega4808 / ATmega4809** (Nano Every, MegaCoreX)
* **AVR Dx-series** (e.g., AVR128DA48 / AVR128DB48, DxCore)
* **tinyAVR 0/1/2-series** (megaTinyCore)

It uses the **Event System** to connect the DCC input directly to a **TCB timer** in *Capture Frequency Measurement Mode*.
This hardware-based approach removes almost all software latency, enabling precise **half-bit timing** in compliance with **RCN-210**.

---

## Operating Principle
- **Event System** routes DCC pin transitions directly to a **TCB timer** — no CPU intervention.
- On each edge of the DCC signal, the TCB:
  1. Captures the time since the previous edge in the `TCBn.CCMP` register.
  2. Triggers the TCB ISR, which classifies the half-bit as `0` or `1` based on the contents of the `TCBn.CCMP` register.
  3. Passes the result to `sup_isr_assemble_packet.h` for packet assembly.
- Both **first** and **second** half-bits are measured, ensuring accurate bit-length detection.
- After a complete packet is assembled:
  - Data is copied to `dccMessage.data`.
  - `dccMessage.isReady` is set.

---

## Data Flow

1. **Event System** detects a DCC edge → triggers TCB capture.
2. **TCB ISR**:
   * Toggles trigger edge polarity.
   * Interprets the measured interval as part of a 0 or 1 bit.
   * Forwards the bit to the packet assembler.
3. On packet completion:
   - Packet is moved to `dccMessage.data`.
   - `dccMessage.isReady` flag is set.

---

## Hardware Requirements
- **MCU**: ATmega with Event System and TCB timers (e.g., MegaCoreX, DxCore and TinyAVR 0/1/2-series).
- **Timer**: One TCB timer (`TCB0` by default; configurable via `DCC_USES_TIMERB1` / `B2` / `B3`).
- **Event Channel**: Automatically assigned to connect the DCC pin to the TCB; depending on the other code pin/event routing restrictions may apply.
- **Pins**: Any GPIO that can act as an event generator.
- Optional: ACK output pin for Service Mode.

---

## Software Requirements (board definitions)
* **ATmega4808 / ATmega4809** → [MegaCoreX](https://github.com/MCUdude/MegaCoreX)
* **AVR Dx-series** → [DxCore](https://github.com/SpenceKonde/DxCore)
* **tinyAVR 0/1/2-series** → [megaTinyCore](https://github.com/SpenceKonde/megaTinyCore)

---

## Advantages over Generic Driver
* **Minimal CPU load** — avoids slow Arduino calls (`attachInterrupt()`, `micros()`).
- **Precise half-bit timing** — compliant with RCN-210.
- **Robust against jitter** — hardware measurement avoids software delays.
- **Solid basis for future RailCom implementation** — though RailCom TX is not yet implemented.

---

## Limitations
- Requires modern ATMega MCUs, with **Event System** and **TCB timers**.
- Implementation complexity is higher than the generic driver.
- RailCom feedback is not yet implemented.
- Zero-bit stretching is not implemented.

---

## Performance Considerations

On classic ATmega MCUs (e.g., ATmega328P, ATmega2560), each interrupt pin has its own vector, so `attachInterrupt()` is relatively fast.
On newer ATmega MCUs, pins share an ISR per port, which forces Arduino’s `attachInterrupt()` to check which pin caused the interrupt — adding overhead.
Also, `micros()` takes longer on these newer devices.

Example measurements with the **NmraDcc** library (similar to the Generic driver):



![Performance difference](../attachInterrupt-NMRADCC.png "Performance difference")

- **UNO**: ~15 µs to capture & process a DCC bit.
- **Nano Every**: ~22 µs for same task.
- `attachInterrupt()` overhead: 4.1 µs (UNO) vs 7.4 µs (Every).
- `micros()`: 3.5 µs (UNO) vs 7.2 µs (Every).

By using the **Event System + TCB Capture Mode** offered by the newer ATMega processors, the DCC bit timing can be measured in hardware, with CPU intervention only on capture. As a result, ISR execution time drops considerably, to **~3–4 µs** on a 24 MHz AVR128DA48:

![Performance new processors](../CPU-load-128DA48-24Mhz-1.png "Performance new processors")

For more detail, see: [Performance on MegaCoreX and DxCore microcontrollers](extras/Performance_MegacoreX.md).

---

## Future Enhancements
RailCom support is **currently in development**.
Planned approach:
- Use the Packet End Bit to detect the RailCom cutout (per RCN-217).
- Start an additional timer that fires at the correct moment.
- Use the Event and CCL peripherals to:
  - Route the RailCom timer output to a dedicated UART for RailCom transmission.
  - Route the UART output to a RailCom TX-pin.

This should allow for reliable RailCom feedback with minimal CPU involvement.
