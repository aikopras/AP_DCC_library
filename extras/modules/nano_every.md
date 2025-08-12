# Nano Every Driver (`sup_isr_Nano_Every.h`)

## Purpose
The **Nano Every** driver implements DCC reception for the Arduino Nano Every using a combination of a **pin change interrupt** and a **TCB timer** in Periodic Interrupt mode.  
It is functionally similar to the [Mega driver](mega.md) but adapted to the Nano Every's different timer naming and operation.

⚠️ **Note:** This driver is only recommended if you do not want to install the [MegaCoreX](https://github.com/MCUdude/MegaCoreX) core.  
Using MegaCoreX together with the [MegaCoreX/DxCore driver](megacorex-dxcore.md) is strongly preferred due to significantly better performance.

## Operating Principle
- A **pin interrupt** triggers on each rising DCC edge.
- The ISR starts the selected **TCB timer** in Periodic Interrupt mode.
- The timer ISR runs every ~66 µs to read the DCC input level.
- Bit values are assembled into packets using `sup_isr_assemble_packet.h`.

## Data Flow
1. **Pin ISR**:
   - Resets and starts the TCB timer.
2. **Timer ISR**:
   - Reads the DCC input pin directly via port access.
   - Classifies the bit as `0` or `1`.
   - Passes the bit to the packet assembly routine.
3. On packet completion:
   - Packet is moved to `dccMessage.data`.
   - `dccMessage.isReady` flag is set.

## Hardware Requirements
- **MCU**: Arduino Nano Every (ATmega4809 running with the Arduino default core).
- **Timer**: One TCB timer (`TCB0` by default; can be changed via `WE_USE_TIMERB1` / `B2` / `B3`).
- **Pins**: Any GPIO usable as external interrupt source.
- Optional: ACK output pin for Service Mode.

## Software Requirements (board definitions)
- **Arduino Nano Every** with the **Arduino default core** (no MegaCoreX installed).

## Advantages over Generic Driver
- Simpler than the Event System approach.
- Works with the stock Arduino Nano Every core.

## Limitations
- Higher CPU load than the MegaCoreX/DxCore driver.
- Less accurate timing — depends on periodic sampling rather than hardware capture.
- Requires careful tuning of timer period (~66 µs) to avoid missing edges.
- Polarity of the DCC signal **is important**.
- Not recommended if MegaCoreX is an option.

## Performance Considerations
The Arduino default core for ATmega4809 has high overhead for `attachInterrupt()` calls due to its shared-port ISR model.  
This driver uses periodic sampling to avoid that cost, but at the expense of timing precision.  
Measured ISR execution time is typically 3–8 µs, but bit detection margin is narrower than with the Event System.

## Future Enhancements
- No RailCom support planned for this driver — use MegaCoreX with the MegaCoreX/DxCore driver for advanced features.
