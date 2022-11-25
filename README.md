# Project AlexMod
AlexMod : Custom firmware for AlexMos SimpleBGC Expansion board  
Using SPWM for Open loop BLDC motor position controlling.

How it works ?
=

The onboard microcontroller ATtiny261A generates 3 Phase (3 channels PWM for U V and W phase) 32KHz PWM from Timer 1. This result in resultant vector A.K.A position of the rotor of the BLDC motor. By changing the Sine angle (Step through Sine LUT). We can precisely control the angle of the rotor. Of course, the precision is depends on PWM resolution (In this case is 8 bit instead of 10 bit) and Sine LUT resolution (Which I went with 256 Sine steps).

The code I wrote use I2C to communicate between the Master device (Arduino Uno or anything) and Slave device (ATtiny261A) with 7 bit address of **0x30**. Master send 3 bytes command. First byte is the direction and the rest are step count.

```
[    First Byte      ]   [     Second byte    ]   [     Thrid byte     ]
7  6  5  4  3  2  1  0   7  6  5  4  3  2  1  0   7  6  5  4  3  2  1  0
FW BW X  X  X  X  X  X      Step count from 0 - 65535 (LSB Byte first)
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
Step count from 0 to 65535. **Send LSB byte first then send the MSB Byte**. Sending 0 step result in no motor movement.

Arduino Setting up. *IMPORTANT*
=

1. Using ATTinyCore. Select board as **"ATtiny261/461/861(a)"**
2. Select Clock source to **"8MHz internal"**
3. Select Timer 1 to **"64MHz"**
4. Disable millis()/micro() to save flash and reserve Timer 0 for future use.
5. Don't forget to **Burn bootloader** to update FUSE settings.

TODO
=

- Controllable speed.
- Save motor constant into EEPROM for degree control mode.
- came up with fomular to calculate the motor constant.