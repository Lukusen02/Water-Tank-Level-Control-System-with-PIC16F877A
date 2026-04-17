# Water Tank Level Control System with PIC16F877A

## Overview

This project implements an embedded system for monitoring and controlling the water level of a tank using the PIC16F877A microcontroller. The system automatically manages a water pump, controls a stepper motor via an A4988 driver, and provides real-time feedback through a 7-segment display and LED indicators.

The firmware is written in C and follows a modular, non-blocking architecture to ensure stable and continuous operation.

## Features

* Real-time water level detection using digital sensors
* Automatic pump control (ON/OFF based on tank level)
* Stepper motor control with 3 selectable speeds
* Push-button interface with software debouncing
* Visual feedback using:

  * 7-segment display (BCD via 74LS47)
  * LED indicators for speed levels
* Non-blocking system design (state machine + time base)


## Hardware Requirements

* PIC16F877A microcontroller
* A4988 stepper motor driver
* Stepper motor (e.g., NEMA 17)
* 7-segment display (common anode)
* 74LS47 BCD to 7-segment decoder
* Water level sensors (electrodes)
* NPN transistor (for pump control)
* Push button
* LEDs (x3)
* Resistors
* Power supply
* Breadboard or PCB


## Pin Configuration

| Pin          | Function              |
| ------------ | --------------------- |
| RA0–RA3, RA5 | Water level sensors   |
| RB0–RB3      | BCD output to display |
| RB4–RB6      | Speed indicator LEDs  |
| RC0          | STEP (A4988)          |
| RC1          | DIR (A4988)           |
| RC2          | Pump control          |
| RC3          | Push button           |


## Setup

1. Clone this repository:

```bash
git clone https://github.com/your-username/your-repo.git
```

2. Open the project in MPLAB (XC8 compiler).

3. Connect the hardware according to the pin configuration.

4. Compile and upload the code to the PIC16F877A.

## Usage

* Power on the system.
* The display shows the current water level (0–5).
* The pump:

  * Turns ON when the tank is empty
  * Turns OFF when the tank is full
* Press the button to cycle through motor speeds:

  * Speed 1 → Speed 2 → Speed 3 → repeat
* LEDs indicate the current speed level.
* The stepper motor runs continuously according to the selected speed.


## Testing

The system was tested through:

* Simulation in Proteus
* Physical implementation on breadboard

All modules (sensors, motor, display, and pump control) were validated under different operating conditions.

## Schematic

*Add your Proteus schematic image here*

## Code

The full source code is included in this repository.


## References

* PIC16F877A Datasheet (Microchip)
* A4988 Datasheet (Allegro)
* 74LS47 Datasheet (Texas Instruments)


## Authors

* Juan Pablo González
* Samuel David Martínez

