#💻 M5StickC Plus2 Magic 8-Ball: Optimized & Power-Managed

Project Overview 🪄

This project turns the M5StickC Plus2 into a classic Magic 8-Ball
Uses the IMU to simulate the feeling of shaking a Magic 8-ball. Upon shaking, the scrteen will display a random answer.
Icludes essential battery monitoring and quick access to time and brightness controls.



✨ Key Features and Power Optimizations

- Quick Brightness Control - Button B (Short Press) on the main screen - Cycles brightness from $\mathbf{100\%}$ down to $\mathbf{1\%}$ (Night Mode) for huge power savings

- Low-Flicker Clock/Battery - Automatic, Conditional Redraws - Prevents rapid screen updates and flicker on stable numbers, saving processing power.

- Extreme Power Monitoring - Automatic - Battery percentage only updates when a $\mathbf{5\%}$ change is detected, drastically reducing redraw frequency.

- Time Setup - Button A (Long Press ≥ 2s) - Dedicated, intuitive setup for adjusting the internal RTC clock.

- Deep Sleep Mode - Automatic (60-second inactivity) - Maximizes battery life when idle.


🕹️ Controls Guide

Button A
 - Long Press (2s) - Enters Time Adjustment Mode and in the time set mode
 - on the Time adjustment mode, tap to adjust the digits

Button B
- on the main screen, use it to cycle the brightness in 25% increments
- on the Time adjustment screen, tap to cycle Hour $\rightarrow$ Minute $\rightarrow$ Save $\rightarrow$ Exit

Shake the device to display a random Magic 8-ball answer~
