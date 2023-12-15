# Timed Capsule

PCB and firmware for the CW&T Timed Capsule project.

![image](https://github.com/bigjosh/Timed-Capsule/assets/5520281/91afad04-3e84-4fa1-a295-33e6752bc5ca)


The Timed Capsule is a commitment device. You put something in the capsule and set the timer for between 1 hour and 100 years and you will not be able to (nondestructively) get the thing for that long. 

![image](https://github.com/bigjosh/Timed-Capsule/assets/5520281/0ab3330d-875e-4f35-816d-c331ee688e84)


## Design goals:

* Run for decades on 2xAA batteries.
* Stay accurate to to within ±2ppm over the lifetime.
* Have enough power at the end of 50 years in torage to still be able to decisively open the locking mechanisim. 
* Replaceable batteries

## Critical parts:

* MSP430FR4133 processor for LCD driving and supervision
* RV3032-C7 RTC for precision timekeeping
* 7-digit, dynamic LCD glass 
* 4x Energizer Ultimate Lithium AA batteries 
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
3. A blank screen.

... stopping on each screen for 1 second. 

If there is less than 1 day remaing until unlock, then the `HHMMMSS` display will show continuously. 

### Unlock

At the appointed hour, the locking mechanisim will automatically retract with a satisfying thud. You should then be able to remove the cap.

Note that the mechanisim will not be able to release unless the handle is in the resting position (turned all the way counter clockwise). If you accedentially turn the handle clockwise when the mechanisim is unlocking, then it will stay locked.
Since the timer automatically resets to 1 hour after each unlock attempt, you can turn the handle counter clockwise and then clockwise again to restart the countdown and - as long as you let the handle return to the resting position- it will unlock then.

## Interesting twists

It is hard to find common solenoids that definitively actuate at voltages/currents available with our AA batteries, so we picked the solenoids carefully and use an extra pair AA batteries soley for a voltage boost at the moment we pull them. You might suggest using either a boost inductor-based boost circuit or a capacitor-based charge pump to avoid ths, but it would Require very,very big parts to store enough energy to be useful here (we can not use electrolitic caps since they will likely fail over the decades). It is hard to compete with the density of chemical energy!

We are using the MSP430's Very Low Power Oscilator (VLO) to drive the LCD since it is actually lower power than the 32Khz XTAL. It is also slower, so more power savings. 

We do NOT use the MSP430's "LPMx.5" extra low power modes since they end up using more power than the "LPM4" mode that we are using. This is becuase it takes 250us to wake from the "x.5" modes and durring this time, the MCU pulls about 200uA. Since we wake 2 times per second, this is just not worth it. If we only woke every, say, 15 seconds then we could likely save ~0.3uA by using the "x.5" modes. 

We use the RTC's CLKOUT push-pull signal directly into an MSP430 io pin. This would be a problem durring battery changes since the RTC would try to power  
the MSP430 though the protection diodes durring battery changes. We depend on the fact that the RTC will go into backup mode durring battery changes, which will
float the CLKOUT pin. 

Using CLKOUT also means that we get 2 interrupts each second (one on rising, one falling edge). We quickly ignore every other one in the ISR. It would seem more power efficient to use the periodic timer function of the RTC to generate a 0.5Hz output, but enabling the periodic timer uses an additional 0.2uA (undocumented!), so not worth it. 

To make LCD updates as power efficient as possible, we precomute the LCDMEM values for every second and minute update and store them in tables. Because we were careful to put all the segments making up both seconds digits into a single word of memory (minutes also), we can do a full update with a single 16 bit write. We further optimize but keeping the pointer to the next table lookup in a register and using the MSP430's post-decrement addressing mode to also increment the pointer for free (zero cycles). This lets us execute a full update on non-rollover seconds in only 4 instructions (not counting ISR overhead). This code is here...
[CCS%20Project/tsl_asm.asm#L91](CCS%20Project/tsl_asm.asm#L91)

## No power mode

If you somehow manage to interrupt the power durring countdown, the timer will resume counting at whatever time is was at when the power was removed. 


## Build notes

## Current Usage

| Mode | Vcc=3.55V| Vcc=2.6V |
| - | -: | -: | 
| Setting mode | 1.3uA | 1.2uA | 
| Countdown mode | 1.1uA | 1.0uA | 
| Unlock | 1A | 1A |

3.55V is approximately the voltage of a pair of fresh Energizer Ultra batteries.
2.6V is approximately the voltage when the screen starts to become hard to read. 

Note that voltage drop over time is not expected to be linear with Energizer Ultra cells. These batteries are predicted to spend most of their lives towards the higher end of the voltage range and only start dropping when they get near to their end of life.  

There are gains of up to 5uA possible from having fewer LCD segments lit. Not sure how actionable this is. We could, say, save 0.5uA by blinking the Time Since Launch mode screen off every other second. It is likely that Ready To Launch mode's low power relative to Time Since Launch mode is due to the fact that it has only 1 segment lit per digit. 

### Measurement conditions

Production board from initial batch. Bare PCB on my desk (not in tube).  
68F
80%RH

(Higher humidity and temperature tends to increase current consumption. Need some different weather conditions to quantify this!) 
