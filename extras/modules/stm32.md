# STM32 Driver (`STM32.h`)

## Purpose
The *STM32* driver implements **high-performance DCC reception** for STM32 microcontrollers, using **hardware timers in capture/compare mode** for accurate half-bit timing.  
It is designed for modern STM32 MCUs (F0/F1/F3/F4/F7/L4 series, etc.) running the Arduino STM32 core (or compatible HAL).  

This driver aims to **minimize CPU load**, achieve precise timing for **RCN-210 compliance**, and provide a solid foundation for future **RailCom** implementation.

---

## Operating Principle
- A **single hardware timer channel (CCR)** captures **both edges** of the DCC input.
- The DCC input pin is connected to the timer's input capture channel. On every rising or falling edge:
  1. The timer stores the **current count** in the capture register.
  2. The ISR reads the difference (`delta`) between the current and previous capture values.
  3. The `delta` is classified as **1-bit** or **0-bit** based on pre-defined thresholds.
  4. Half-bits are handled natively, allowing **accurate detection of both first and second half of each DCC bit**.
- The ISR then forwards the bit to `sup_isr_assemble_packet.h` to construct full DCC packets.

---

## Pin Selection
For maximum performance and reliability, the STM32 driver uses a **fixed input pin connected to a specific timer capture channel**. This decision was made not only to minimize ISR latency but primarily because mapping arbitrary GPIO pins to timers and channels in a way that works across all STM32 variants is extremely complex. STM32 MCUs vary widely: different families and even different packages can have different timer-to-pin mappings, alternate function assignments, and channel availability. Attempting to create a universal mapping system that works reliably on every STM32 model is impractical and would likely result in intermittent failures on some devices.

By choosing fixed pins for the DCC input, the driver guarantees **predictable, sub-microsecond timing**, and ensures that the timer input capture works correctly on all supported STM32 boards. Users who wish to use a different pin can do so by modifying **three `#define` statements** in the driver’s configuration. However, care must be taken to consult the **STM32 datasheet and reference manual** for the specific MCU in use, to ensure the chosen pin supports the desired timer input capture channel. This approach balances performance, reliability, and configurability, while keeping the code simple and portable across multiple STM32 families.

---

## Performance
- Measured on an STM32F411 at 100 MHz:  
  - **Time from DCC edge to ISR entry:** ~1.2 µs  
  - **Average ISR execution (excluding entry/exit):** ~0.8 µs  
  - **Total ISR duration:** ~3 µs  
- Using **direct timer capture** ensures minimal jitter and predictable timing, critical for RailCom and RCN-210 compliance.  
- **Delta calculation** is performed using direct register access (`DCC_TIMER->CCRn`), which is the fastest possible method on STM32.

---

## Data Flow
When a DCC edge occurs, the timer captures the current count and immediately triggers the ISR. The ISR calculates the time difference from the previous capture to determine the length of the half-bit. Based on this interval, the half-bit is classified as either a `1` or a `0`. The bit is then forwarded to the packet assembler, which stores the half-bit in a temporary buffer. Once a full DCC packet is completed, it is copied into `dccMessage.data` and the `dccMessage.isReady` flag is set, signaling that a complete message is available for the main application.

---


## Hardware Requirements
The STM32 driver requires an **STM32 MCU** with at least one **general-purpose timer** capable of input capture on a GPIO pin. By default, the driver is configured to use **PB6** as the DCC input pin, connected to **TIM4, Channel 1**. This default ensures compatibility with most STM32 Nucleo and Blue Pill boards while providing excellent timing performance.

Users who need to use a different pin or timer channel can do so by modifying the corresponding **`#define` statements** in the driver configuration. When changing the pin or timer, it is important to carefully consult the **specific STM32 datasheet and reference manual** to ensure that the selected pin supports input capture on the desired timer channel. Incorrect mapping may result in the timer failing to capture edges, leading to unreliable DCC decoding.

No additional peripherals are required; the driver operates purely via **timer input capture interrupts** and a single output pin for optional Service Mode acknowledgment.

---

## Advantages over Generic Driver
- **Extremely low latency:** 1.2 µs from edge to ISR, versus ~4 µs typical with `micros()`-based Generic driver.  
- **Half-bit capture:** accurate measurement of both halves of the DCC bit.  
- **Minimal CPU load:** ISR is only ~3 µs per edge, leaving the CPU free for other tasks.  
- **Predictable timing:** hardware timer ensures reliable RCN-210 and future RailCom support.

---

## Comparison with DxCore / MegaCoreX
| Feature                   | STM32 Driver             | DxCore / MegaCoreX       | Generic Driver         |
|----------------------------|------------------------|-------------------------|----------------------|
| Half-bit capture           | ✅ Yes                 | ✅ Yes                  | ❌ No                 |
| ISR latency (typical)      | 1.2 µs entry           | ~3–4 µs                 | ~15–20 µs             |
| Total ISR duration         | ~3 µs                  | ~3–4 µs                 | 10–20 µs              |
| CPU load                   | Minimal                | Minimal                 | High                  |
| Hardware dependency        | Timer capture channel  | Event System / TCB      | None                  |
| RailCom ready              | Planned                | Planned                 | ❌ Not supported      |
| Pin flexibility            | Fixed CCR pin (configurable in board code) | Fixed event pin | Any GPIO (attachInterrupt) |

---

## Limitations
- Requires a **timer with input capture** — not all STM32 MCUs have sufficient channels.  
- DCC input pin is **fixed to a timer channel** for performance; remapping requires user configuration.  
- RailCom is **not yet implemented**, but hardware timing supports future addition.  

---

## Takeaways
- STM32 driver provides **the fastest, most precise DCC reception** for Arduino-based STM32 projects.  
- ISR latency and execution are fully deterministic, making it **ideal for RailCom-capable decoders**.  
- While the Generic driver is portable and DxCore offers excellent timing on AVR, STM32 combines **speed, accuracy, and MCU performance headroom** in one solution.
