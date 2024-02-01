# Timed Capsule

PCB and firmware for the CW&T Timed Capsule project.

![image](https://github.com/bigjosh/Timed-Capsule/assets/5520281/91afad04-3e84-4fa1-a295-33e6752bc5ca)


The Timed Capsule is a commitment device. You put something in the capsule and set the timer for between 1 hour and 100 years and you will not be able to (nondestructively) get the thing for that long. 

![image](https://github.com/bigjosh/Timed-Capsule/assets/5520281/0ab3330d-875e-4f35-816d-c331ee688e84)


## Design goals:

* Run for decades on 2xAA batteries.
* Stay accurate to to within ±2ppm over the lifetime.
* Have enough power at the end of 50 years in storage to still be able to decisively open the locking mechanisim. 
* Replaceable batteries

## Critical parts:

* MSP430FR4133 processor for LCD driving and supervision
* RV3032-C7 RTC for precision timekeeping
* 7-digit, dynamic LCD glass 
* 4x Energizer Ultimate Lithium AA batteries
* Solenoids which pull decicively at 6V
* Optional TPS7A0230 3V regulator for generating LCD bias voltage

## Method of Operation

### Set timer

Use the `MOVE` button to change cursor posision on the LCD, use the `CHANGE` button to change the current position. 

The rightmost position is the units and can be...

| Value | Units | 
| - | - | 
| `H` | Hours |
| `d` | Days |
| `y` | Years | 

Note that a [year is 365.2422 days](https://pumas.nasa.gov/sites/default/files/examples/04_21_97_1.pdf). 

### Lock 

1. Turn the locking handle on the top clockwise 1/4 turn until it stops.
2. Release the handle and let it return to the resting position.

### Wait

Durring the countdown, the LCD  will rotate though...

1. Days remaining as 'xxxxxd`.
2. Hours, minutes, and seconds until the current day is done as `HHMMSS`.

... stopping on each screen for 1 second. 

If there is less than 1 day remaing until unlock, then the `HHMMMSS` display will show continuously. 

### Unlock

At the appointed hour, the locking mechanisim will automatically retract with a satisfying thud. You should then be able to remove the cap.

Note that the mechanisim will not be able to release unless the handle is in the resting position (turned all the way counter clockwise). If you accedentially turn the handle clockwise when the mechanisim is unlocking, then it will stay locked.
Since the timer automatically resets to 1 hour after each unlock attempt, you can turn the handle counter clockwise and then clockwise again to restart the countdown and - as long as you let the handle return to the resting position- it will unlock then.

## Interesting twists

### Solenoids

It is hard to find common solenoids that definitively actuate at voltages/currents available with our AA batteries, so we picked the solenoids carefully and use an extra pair AA batteries soley for a voltage boost at the moment we pull them. You might suggest using either a boost inductor-based boost circuit or a capacitor-based charge pump to avoid ths, but it would require some very big parts to store enough energy to be useful here (we can not use electrolitic caps since they will likely fail over the decades). It is hard to compete with the density of chemical energy! You might also suggest using a motor and gears to do the actuation (this is how most battery powered door locks do it), but it is hard to imagine gears that stay reliable after sitting still for 50 years. 

### Oscilator

We are using the MSP430's Very Low Power Oscilator (VLO) to drive the LCD since it is actually lower power than the 32Khz XTAL. It is also slower, so more power savings. The internal VLO also empreically uses less total power than using the RV3032 clkout to drive the MSP430's LCD clock, which is supprising. 

### MSP430 LPMx.5 modes

We do NOT use the MSP430's "LPMx.5" extra low power modes since they end up using more power than the "LPM4" mode that we are using. This is becuase it takes 250us to wake from the "x.5" modes and durring this time, the MCU pulls about 200uA. Since we wake every second, this is just not worth it. If we only woke every, say, 15 seconds then we could likely save ~0.3uA by using the "x.5" modes.

### LCD software optimications

To make LCD updates as power efficient as possible, we precomute the LCDMEM values for every second and minute update and store them in tables. Because we were careful to put all the segments making up both seconds digits into a single word of memory (minutes and hours also), we can do a full update with a single 16 bit write. We further optimize by keeping the pointer to the next table lookup in a register and using the MSP430's post-decrement addressing mode to also increment the pointer for free (zero cycles). This lets us execute a full update on non-rollover seconds in only 4 instructions (not counting ISR overhead). This code is here...
[CCS%20Project/tsl_asm.asm#L91](CCS%20Project/tsl_asm.asm#L91)

We also further reduce power using the MSP430's LCD buffer hardware. We keep the "days" display in one LCD buffer and the "HHMMSS" in the the other buffer. This way we only need to update one bit to switch between the two and therefore only end up needing to write to the "days" page once per day to update the day count. 

Finally, we use the MSP430's blinking segment buffer to create the cursor in setting mode. Since this is all done in hardware, the MCU is always sleeping in setting mode
except immedeately after a button is pushed or the trigger is activated.

### Power supply glitch filter

The 100 ohm resistor between the battery and the Vcc pin of the MCU is needed to create a low pass filter. This slows down the voltage changes when the solenoids turn on and off (these solenoids pull more than 1 amp). The MCU can brown out if it sees a change on the Vcc pin faster than 1V/us. The maximum current the MCU can pull is only about 2mA so we only lose 0.2 volts of power supply voltage worst case, and this chip can run down to 1.8V so this does not cost us any run time.

### Keeping backup countdown in FRAM

This chip has FRAM memory which is consistant across power cycles and resets. We keep the current countdown time left (with 1 minute resoltion) in FRAM so in case the chip ever resets, then we will pick up where we left off. There is some complexity in making sure that updates to this FRAM counter are atomic so it does not get corrupted if we happen to fail in the middle of an update. 

### LCD hardware optimizations

We need regulated 3.0V DC for the LCD bias voltage, but we have 3V-3.6V coming from the batteries. 

The MSP430 has a built in adjustable voltage regulator. Is this more efficient than the very-efficient TPS7A30 dedicate regulators?

Let's test! Here we display the specified pattern and then go into LPM4 sleep and then measure current using a Joulescope. 

Vcc=3.55V<br>(2xAA fresh)
| Pattern | TPS7A30 |  Internal 2.96V  |  Internal 3.08V |
| - | -: | -: |   -: | 
| blank    | 0.85uA | 1.10uA | 1.12uA |
| "111111" | 1.01uA | 1.65uA |  |
| "555555" | 1.31uA | 2.36uA |  |
| "888888" | 1.19uA | 2.05uA | 2.12uA |

Ok, since we will spend most of our lifetime around 3.55V, it is clear that the TPS7A is worth it!

Interesting that the 1uF capacitor on the basis pin can maintain the display for several seconds after disconnected from the regulator. 


Let's just make sure there are no surpises when we least want them - when the battery is reaching the end of its life...

Vcc=2.6V<br>(2xAA after many decades)
| Pattern | TPS7A30 |  Internal 2.96V  |
| - | -: | -: |
| "555555" | 1.03uA |  | 
| "888888" | 1.00uA | 2.02uA | 

Good enough. Note that since the TPS7A is only a regulator and not a carge pump that once the Vcc drops below 3V that the LCD will start to 
get dimmer (actually you will just need to look at it from a lower angle to see the same contract level). I see this as a feature for this
application since once we get below 3V I want to user to get some feedback that the batteries are getting low, but for other applications or
if you are using alkaline batteries then you might want to switch to the internal charge pump below 3V. 

And just for fun, let's see how much power we could save if we did not need a regulated voltage soruce at all...

Vcc=3.0V
| Pattern | Direct Vcc |
| - | -: |
| "555555" | 1.25uA | 

... so, wow, the TPS7A is really efficient for generating the LCD bias voltage in this application.

Since we are now going to include the TPS7A in the design, could we get even more power savings by having it also regulate the Vcc for the MSP430?

Vcc=3.55V
| Test | Direct Vcc | TSP7A2030 |
| - | -: |
| Static "123456" | 1.14uA | 1.14uA |
| 1Hz digit updates | 1.09uA | 1.06uA |

So ever so slight saving. Probably not worth the loss in flexibility since ifonly use the TSP7A20 for the LCD bias voltage and we can not get it, we can 
just omit it from the board and and instead enable the internal regulator. 

## Build notes


### Current Usage

| Mode | Vcc=3.55V<br>(2xAA fresh) | Vcc=2.6V<br>(2xAA after many decades) |  Vcc=7.0V<br>(4xAA fresh) | Vcc=5.2V (4xAA after many decades) |
| - | -: | -: | -: | -: |
| Setting mode | 1.3uA | 1.2uA |  |  |
| Countdown mode | 1.1uA | 1.0uA |  |  |
| Unlock |  |  | 1.2A | 1A |

3.55V is approximately the voltage of a pair of fresh Energizer Ultra batteries.
2.6V is approximately the voltage when the screen starts to become hard to read. 

Note that voltage drop over time is not expected to be linear with Energizer Ultra cells. These batteries are predicted to spend most of their lives towards the higher end of the voltage range and only start dropping when they get near to their end of life.  

### Measurement conditions

Production board from initial batch. Bare PCB on my desk (not in tube).  
68F
80%RH

(Higher humidity and temperature tends to increase current consumption. Need some different weather conditions to quantify this!) 
