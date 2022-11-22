# Project AlexMod
AlexMod : Custom firmware for AlexMos SimpleBGC Expansion board  
Using SPWM for Open loop BLDC motor position controlling.

How it works ?
=

The onboard microcontroller ATtiny261A generates 3 Phase (3 channels PWM for U V and W phase) 32KHz PWM from Timer 1. This result in resultant vector A.K.A position of the rotor of the BLDC motor. By changing the Sine angle (Step through Sine LUT). We can precisely control the angle of the rotor. Of course, the precision is depends on PWM resolution (In this case is 8 bit instead of 10 bit) and Sine LUT resolution (Which I went with 256 Sine steps).

This technique is called "**Field Oriented Control**" or **FOC** for short. But the actual FOC requires Close loop feedback for Load compensation. 

The code I wrote use I2C to communicate between the Master device (Arduino Uno) and Slave device (ATtiny261A) with 7 bit address of **0x30**. Master send 2 bytes command. First byte is the direction and the second byte is the step count.

```
[    First Byte      ]   [     Second byte    ]
7  6  5  4  3  2  1  0   7  6  5  4  3  2  1  0
FW BW X  X  X  X  X  X   Step count from 0 - 255
```
FW and BW bit
-
| FW bit | BW bit | Direction                    |
|--------|--------|------------------------------|
|    0   |    0   | Motor will not spin          |
|    1   |    0   | Motor spin clockwise         |
|    0   |    1   | Motor spin counter clockwise |
|    1   |    1   | Motor will not spin          |

Step count
-
Step count from 0 to 255 will added by 1 automatically on the ATtiny261A. Meaing that setting Step count to 0, the motor will move 1 step. Setting Step count to 255, the Motor will move 256 steps.