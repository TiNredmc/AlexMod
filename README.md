# Project AlexMod
AlexMod : Custom firmware for AlexMos SimpleBGC Expansion board  
Using SPWM for Open loop BLDC motor position controlling.

How it works ?
=

The onboard microcontroller ATtiny261A generates 3 Phase (3 channels PWM for U V and W phase) 32KHz PWM from Timer 1. This result in resultant vector A.K.A position of the rotor of the BLDC motor. By changing the Sine angle (Step through Sine LUT). We can precisely control the angle of the rotor. Of course, the precision is depends on PWM resolution (In this case is 8 bit instead of 10 bit) and Sine LUT resolution (Which I went with 256 Sine steps).

The code I wrote use I2C to communicate between the Master device (Arduino Uno or anything) and Slave device (ATtiny261A) with 7 bit address of **0x30**. Master send 5 bytes command. First byte is the direction and the rest are step count and Sine frequency.

```
[    First Byte      ]   [     Second byte    ]   [     Thrid byte     ]   [	 Fourth byte    ]   [	  Fifth byte     ]
7  6  5  4  3  2  1  0   7  6  5  4  3  2  1  0   7  6  5  4  3  2  1  0   7  6  5  4  3  2  1  0   7  6  5  4  3  2  1  0
Position/Speed control      Step count from 0 - 65535 (LSB Byte first)               Sine frequency 31 - 31250 Hz
```
Position/Speed control 
-
This command byte select the Position or Speed control with direction.

| Command | Mode             | Direction                     |
|---------|------------------|-------------------------------|
|   0x80  | Position control | Motor spins clockwise         |
|   0x40  | Position control | Motor spins counter clockwise |
|   0x20  |   Speed control  | Motor spins clockwise         |
|   0x10  |   Speed control  | Motor spins counter clockwise |

Step count
-
Step count from 0 to 65535. **Send LSB byte first then send the MSB Byte**. 

Sending 0 step result in no motor movement.

Sine frequency 
-
Sine frequency from 4Hz to 2500Hz. **Send LSB byte first then send the MSB Byte**.  

In order to achieve Sine frequency upto 2500Hz. The resolution of Sine Lookup table is downscaled from 256 Sine steps to 8 Sine steps. The LUT step-through frequency is 8 times the Sine frequency.

Note that rotation frequency is lower by the factor of number of magnet pair.  
Example : Motor with 14 magnets has 7 magnet pairs. With Sine frequency of 700Hz. The mechanical rotation frequency is 700Hz/7 = 100Hz or 100Hz * 60 = 6000 RPM.

Arduino Setting up. *IMPORTANT*
=

1. Using ATTinyCore. Select board as **"ATtiny261/461/861(a)"**
2. Select Clock source to **"8MHz internal"**
3. Select Timer 1 to **"64MHz"**
4. Disable millis()/micro() to save flash and reserve Timer 0 for future use.
5. Don't forget to **Burn bootloader** to update FUSE settings.

TODO
=

- Use Trapeziodal commutation when comes to High RPM in speed control mode
