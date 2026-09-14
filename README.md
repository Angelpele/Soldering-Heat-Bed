# PCB Heated Soldering Bed

A custom PCB-based heated soldering bed designed for controlled PCB heating and solder reflow.

The system combines a resistive heating element, temperature feedback through an NTC thermistor, relay-based heater control, active cooling, an OLED user interface, push-button controls, and an Arduino Nano as the main controller.

The project supports both a manually configurable heating mode and a predefined reflow profile based on the Kester Sn63Pb37 temperature profile.

---

## Features

- Arduino Nano based control system
- PCB-mounted heating bed
- 12 V heating system
- NTC thermistor temperature measurement
- Relay-controlled heating element
- 12 V cooling fan
- 0.91" 128×32 I2C OLED display
- Four-button user interface
- Safety lever input
- Buzzer for warnings and status notifications
- Manual temperature control
- Automatic fan control
- Kester Sn63Pb37 reflow profile
- Temperature setpoint ramping to reduce thermal overshoot
- EEPROM storage for manual temperature settings
- Non-blocking software architecture for the buzzer and control system

---

## Project Overview

The PCB Heated Bed is intended to provide a controlled and repeatable heating platform for PCB assembly and soldering experiments.

The Arduino Nano continuously measures the temperature using an NTC thermistor and controls the heating element through a relay. The OLED provides information about the current temperature, operating mode and system status.

The system has two main operating modes:

### Manual Heating

The user can select a target temperature between:

- **30 °C minimum**
- **230 °C maximum**

The selected temperature is stored in EEPROM so that it is preserved after restarting the controller.

### Reflow Profile

The firmware includes a predefined three-stage reflow profile:

| Stage | Target Temperature | Duration |
|---|---:|---:|
| Preheat / Soak Start | 150 °C | 60 s |
| Soak End | 180 °C | 60 s |
| Reflow Peak | 210 °C | 30 s |

The temperature setpoint is ramped at approximately **1 °C/s** to limit the heating rate and reduce overshoot.

---

## Hardware

### Main Components

| Component | Reference | Specification / Description |
|---|---|---|
| Microcontroller | U101 | Arduino Nano |
| Heating element | Heater | 12 V heating element |
| Heater switching | R101 / Relay | 5 V relay |
| Temperature sensor | TH101 | NTC thermistor |
| Cooling fan | M101 | 12 V fan |
| Display | OLED_0.91 | 0.91" 128×32 I2C OLED |
| Buzzer | BZ101 | Buzzer |
| Main power supply | Transformer | 220 V AC → 12 V |
| Main switch | SW / Main Switch | System power switch |
| MOSFET | Q101 | N-channel MOSFET |
| Resistor | R101 | 100 kΩ |
| Resistor | R102 | 100 Ω |
| Resistor | R103 | 1 kΩ |
| Resistor | R104 | Value according to final BOM |
| Buttons | BUT1–BUT4 | User interface buttons |
| Safety input | LEVER | Lever / safety switch |

The components and circuit blocks above are based on the project schematic.

---

## PCB

### Finished Soldered Heating Bed

<!-- Replace with your finished PCB/heating-bed photograph -->

![Finished soldering heated bed](images/finished_heated_bed.jpg)

*Finished PCB and soldered heating-bed assembly.*

---

## Schematic

The complete electrical schematic was designed in KiCad.

<!-- Replace with your schematic image -->

![Heating bed schematic](images/heating_bed_schematic.png)

### Wiring / Cable Connections

<!-- Replace with your cable/wiring photograph -->

![Heating bed wiring](images/heating_bed_wiring.jpg)

The wiring connects the control electronics with the heater, thermistor, fan, OLED, buzzer, buttons and power system.

The power section includes a 220 V AC to 12 V transformer, while the control electronics use the regulated low-voltage supply.

---

## System Architecture

The system can be divided into four main sections:

```text
                    ┌──────────────────────┐
                    │      AC POWER        │
                    │     220 V AC         │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │     Transformer      │
                    │     220 V → 12 V     │
                    └──────────┬───────────┘
                               │
                 ┌─────────────┴─────────────┐
                 │                           │
                 ▼                           ▼
        ┌─────────────────┐         ┌─────────────────┐
        │ Heating Element │         │   12 V Fan      │
        │     + Relay     │         │   + MOSFET      │
        └────────┬────────┘         └────────┬────────┘
                 │                           │
                 │                           │
                 └─────────────┬─────────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │     Arduino Nano     │
                    │     Main Controller   │
                    └───────┬──────┬───────┘
                            │      │
              ┌─────────────┘      └──────────────┐
              ▼                                   ▼
      ┌─────────────────┐                 ┌─────────────────┐
      │  NTC Thermistor │                 │  OLED 128×32    │
      │ Temperature     │                 │  I2C Display    │
      │ Feedback        │                 └─────────────────┘
      └─────────────────┘

                            │
                  ┌─────────┴─────────┐
                  ▼                   ▼
           ┌─────────────┐     ┌─────────────┐
           │  Buttons    │     │   Buzzer    │
           └─────────────┘     └─────────────┘
Temperature Control

The temperature is measured using an NTC thermistor connected to an analog input.

The firmware calculates the thermistor resistance from the ADC reading and then converts it to temperature using the Steinhart-Hart/Beta approximation.

The thermistor configuration used by the firmware is:

const float SERIES_RESISTOR      = 100000.0;
const float NOMINAL_RESISTANCE   = 75000.0;
const float NOMINAL_TEMPERATURE  = 20.0;
const float B_COEFFICIENT        = 4092.0;

The temperature is continuously updated and displayed on the OLED.

Heater Control

The heating element is controlled through a relay.

Instead of simply switching the heater permanently ON or OFF, the firmware uses a time-proportioning control strategy.

The heating window is:

const unsigned long HEAT_WINDOW_MS = 4000;

The maximum heater duty cycle is limited to:

const float HEAT_DUTY_MAX = 0.35f;

This limits the amount of time the heater remains active within each control window.

A proportional heating band is also used:

const float HEAT_PROP_BAND = 30.0f;

This allows the heater output to gradually decrease as the measured temperature approaches the target temperature.

Temperature Ramp Control

A major part of the control system is the temperature ramp.

The firmware does not immediately command the heater to reach the final target temperature.

Instead, it gradually increases a dynamic setpoint:

const float RAMP_RATE_C_PER_SEC = 1.0f;

This gives a target ramp of approximately:

1 °C/s

The purpose of this system is to:

Reduce temperature overshoot
Improve thermal stability
Protect the PCB and components
Better approximate a controlled reflow profile
Compensate for the large thermal mass of the heating bed
Reflow Profile

The firmware contains a predefined reflow profile based on a Kester Sn63Pb37 profile.

const ProfileStage KESTER_PROFILE[MAX_STAGES] = {
  { 150, 60 },
  { 180, 60 },
  { 210, 30 }
};

The three stages are:

Temperature
   ^
210|                         ┌───────────
   |                        /
180|             ┌─────────┘
   |            /
150|───────────┘
   |
   +------------------------------------> Time
        60 s       60 s      30 s

The controller automatically advances from one stage to the next once the measured temperature reaches the stage target and the required dwell time has elapsed.

After the final stage, the heater is disabled and the system enters cooling mode.

Reflow Test

The actual heating performance was tested and compared against the theoretical reflow profile.

<!-- Replace with your actual graph -->

Theory vs Real Temperature

The theoretical profile represents the desired temperature progression, while the measured profile shows the real thermal response of the physical heating bed.

Differences between both curves are expected due to:

Thermal inertia of the heating bed
Heater power limitations
Relay switching behavior
Thermistor response time
Heat losses to the environment
Physical PCB and bed thermal mass
Temperature sensor position
Control-system response time

The comparison is useful for determining whether the heating bed follows the desired reflow profile and for further tuning of the temperature-control parameters.

Software

The controller is programmed using the Arduino IDE and runs on an Arduino Nano.

The firmware is structured around a non-blocking state-machine approach rather than using long delays.

This allows the controller to simultaneously:

Read the thermistor
Update the OLED
Read the buttons
Monitor the safety lever
Control the heater
Control the cooling fan
Generate buzzer notifications
Manage the reflow profile
Store configuration values in EEPROM
Software Libraries

The code uses the following Arduino libraries:

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
Libraries
Wire — I2C communication
Adafruit_GFX — graphics functions for the OLED
Adafruit_SSD1306 — SSD1306 OLED driver
EEPROM — persistent storage of user settings
User Interface

The OLED provides several screens.

Home

Displays:

Current operating mode
Current temperature
Reflow state

Example:

REFLOW: RAMPING
      165.4C
Fan Control

The fan has three operating modes:

AUTO
ON
OFF

In automatic mode, the fan is enabled during cooling and when the temperature is sufficiently high after heating.

Manual Temperature

The user can configure the desired manual heating temperature.

The allowed range is:

30 °C → 230 °C

The value is saved to EEPROM when confirmed.

Reflow Profile

The user can select the predefined Kester Sn63Pb37 profile directly from the OLED menu.

Button Controls

The firmware uses four buttons:

Button	Function
BUT1	Previous menu / increase depending on screen
BUT2	Next menu / decrease depending on screen
BUT3	Select / confirm
BUT4	Back / cancel

The buttons use software debouncing with a 30 ms debounce period.

Safety Lever

A lever input is used as a safety/control input for the heating system.

When the lever is not active, the heater is immediately disabled.

When the lever is activated, the system first enters a warning period before heating begins.

The warning period is:

const unsigned long WARNING_TIME = 5000;

Therefore, the system waits approximately 5 seconds before starting the heating process.

The buzzer provides audible feedback during this period.

Cooling

After completing the reflow profile, the system enters a cooling state.

The heater is switched OFF and the fan is enabled.

Cooling continues until the measured temperature reaches approximately:

50 °C

At that point, the system enters the DONE state and produces a longer buzzer notification.

Firmware State Machine

The bed control software uses the following states:

                 ┌──────────────┐
                 │   BED_IDLE   │
                 └──────┬───────┘
                        │
                        ▼
                ┌───────────────┐
                │ BED_WARNING   │
                └───────┬───────┘
                        │
                        ▼
                ┌───────────────┐
                │ BED_RAMPING   │
                └───────┬───────┘
                        │
                        ▼
                ┌───────────────┐
                │ BED_SOAKING   │
                └───────┬───────┘
                        │
                  Next stage?
                    /       \
                  YES       NO
                   │         │
                   ▼         ▼
             BED_RAMPING  BED_COOLING
                              │
                              ▼
                         BED_DONE

This state-machine architecture makes the heating process easier to control and prevents the main program from becoming blocked during heating or cooling.

Arduino Pin Configuration

The current firmware defines the following connections:

Arduino Pin	Function
D5	Relay / heater control
D6	Cooling fan
D7	BUT1
D8	BUT4
D9	Buzzer
D10	BUT3
D11	BUT2
D12	Safety lever
A1	NTC thermistor
A4	OLED SDA
A5	OLED SCL

Note: The firmware pin assignment should be kept synchronized with the final PCB schematic and wiring.

EEPROM Settings

The firmware stores the manually selected target temperature in the Arduino Nano's EEPROM.

A magic byte is used to determine whether valid settings are already stored.

If the EEPROM contains invalid data, the firmware loads a safe default temperature of:

60 °C

The user therefore does not need to configure the manual temperature again after every restart.

Project Structure

A suggested repository structure is:

PCB-Heated-Bed/
│
├── README.md
│
├── firmware/
│   └── PCB_Heated_Bed_v2.ino
│
├── hardware/
│   ├── Heating_Bed.kicad_sch
│   ├── PCB_Heated_Bed.kicad_pcb
│   └── BOM.csv
│
├── images/
│   ├── finished_heated_bed.jpg
│   ├── heating_bed_schematic.png
│   ├── heating_bed_wiring.jpg
│   └── reflow_theory_vs_real.png
│
└── LICENSE
Assembly

The heating bed consists of the custom PCB, heating element, temperature sensor and control electronics.

After soldering and assembling the components, the system can be tested progressively:

Verify the power supply.
Verify the Arduino Nano starts correctly.
Check the OLED.
Check the NTC temperature reading.
Test the fan.
Test the relay without the heating element connected.
Test the safety lever.
Perform a low-temperature heating test.
Perform a complete temperature-profile test.
Compare the measured profile against the theoretical profile.
Testing

Testing should be performed progressively, starting at low temperatures before attempting a complete reflow cycle.

Particular attention should be paid to:

Temperature sensor calibration
Heater response
Relay operation
Thermal overshoot
Maximum bed temperature
Fan cooling performance
Emergency/safety behavior
Temperature uniformity across the heating surface

The reflow graph should be used to tune the heater duty limit, proportional band and temperature ramp if necessary.

Future Improvements

Possible improvements for future versions include:

Closed-loop PID temperature control
Multiple temperature sensors
Better temperature uniformity across the heating surface
Solid-state relay or MOSFET-based heater switching
More configurable reflow profiles
SD card / USB temperature logging
Automatic calibration of the NTC
Graphical temperature display
PC-based monitoring
Improved thermal insulation
Emergency over-temperature shutdown
Automatic profile generation
License

This project is provided for educational and experimental purposes.

If you use or modify this project, please consider crediting the original project and linking back to this repository.

Author

PCB Heated Soldering Bed

Designed and developed as a custom electronics and PCB assembly project.


**Image filenames to use:** `finished_heated_bed.jpg`, `heating_bed_schematic.png`, `heating_bed_wiring.jpg`, and `reflow_theory_vs_real.png`. This keeps the README clean and makes the placeholders very easy to replace.

One important detail: I kept the **firmware pin mapping from your `.ino`**, rather than blindly copying the labels visible in the schematic, because the schematic appears to contain some pin assignments that differ from the current code.  

¿Quieres que te haga una **versión más profesional/engineering-style** o una **más visual tipo GitHub project sho
