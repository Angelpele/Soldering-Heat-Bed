# PCB Heated Soldering Bed

Custom PCB based heating bed for PCB assembly and solder reflow.

The system uses an Arduino Nano to control a relay that switches a 220V heating element, monitor temperature with an NTC thermistor, control a cooling fan, and provide a simple OLED interface.

---

## Features

| Feature | Description |
|---|---|
| Controller | Arduino Nano |
| Heater | 220 V heating element |
| Temperature sensor | NTC thermistor |
| Heater control | Relay |
| Cooling | 12 V fan + N-MOSFET |
| Display | 0.91" 128×32 I2C OLED |
| User input | 4 buttons + safety lever |
| Audio | Buzzer |
| Modes | Manual heating / Reflow |
| Reflow profile | Sn63Pb37 |
| Temperature range | 30–230 °C |
| Temperature ramp | 1 °C/s |

---

## Hardware

### Components

| Component | Reference | Specification |
|---|---|---|
| Arduino Nano | U101 | Main controller |
| Heating element | Heater | 12 V |
| Relay | R101 | 5 V |
| NTC thermistor | TH101 | Temperature measurement |
| MOSFET | Q101 | N-channel |
| Fan | M101 | 12 V |
| OLED | OLED_0.91 | 128×32, I2C |
| Buzzer | BZ101 | Audio indication |
| Transformer | T1 | 220 V AC → 12 V |
| Main switch | SW1 | Power switch |
| BUT1–BUT4 | — | User controls |
| LEVER | — | Safety input |
| R101 | — | 100 kΩ |
| R102 | — | 100 Ω |
| R103 | — | 1 kΩ |
| R104 | — | — |

---

## Finished Soldering Bed

<p align="center">
  <img src="Media/Completed_view.jpg" alt="Finished heated soldering bed" width="700">
</p>

---

## Schematic & Wiring

<table>
<tr>
<td width="65%" align="center">

**Schematic**

<img src="Media/Heating_Bed.pdf" alt="Heating bed schematic" width="100%">

</td>
<td width="35%" align="center">

**Wiring**

<img src="Media/Internals.jpg" alt="Heating bed wiring" width="100%">

</td>
</tr>
</table>

---

## Reflow Profile

The firmware implements a three-stage Sn63Pb37 reflow profile.

| Stage | Target | Duration |
|---|---:|---:|
| Stage 1 | 150 °C | 60 s |
| Stage 2 | 180 °C | 60 s |
| Stage 3 | 210 °C | 30 s |

The setpoint is ramped at approximately **1 °C/s**.

---

## Reflow Test

The real temperature profile was measured using the NTC and compared with the theoretical profile.

<p align="center">
  <img src="Media/Reflow_testing.jpg" alt="Theoretical vs real reflow temperature profile" width="800">
</p>

| Profile | Description |
|---|---|
| Theory | Target temperature profile |
| Real | Temperature measured during the test |

The difference between the two profiles is mainly related to the thermal inertia of the heating bed, heater power, heat losses, sensor position and control response.

---

## Temperature Control

The NTC is read through an analog input and converted to temperature using the thermistor parameters below.

| Parameter | Value |
|---|---:|
| Series resistor | 100 kΩ |
| Nominal resistance | 75 kΩ |
| Nominal temperature | 20 °C |
| Beta coefficient | 4092 |

```cpp
const float SERIES_RESISTOR      = 100000.0;
const float NOMINAL_RESISTANCE   = 75000.0;
const float NOMINAL_TEMPERATURE  = 20.0;
const float B_COEFFICIENT        = 4092.0;
