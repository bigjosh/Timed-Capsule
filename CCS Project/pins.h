/*
 * pins.h
 *
 * Defines all of the pins we are using
 *
 *  Created on: Oct 15, 2022
 *      Author: josh
 */

#ifndef PINS_H_
#define PINS_H_

// Connected to the RV3203 INT pin which is open collector on RV3032 side

#define RV3032_INT_PREN   P1REN
#define RV3032_INT_PDIR   P1DIR
#define RV3032_INT_POUT   P1OUT
#define RV3032_INT_PIN    P1IN
#define RV3032_INT_PIE    P1IE          // Interrupt enable
#define RV3032_INT_PIV    P1IV          // Interrupt vector (read this to get which pin caused interrupt, reading clears highest pending)
#define RV3032_INT_PIFG   P1IFG         // Interrupt flag (bit 1 for each pin that interrupted)
#define RV3032_INT_PIES   P1IES         // Interrupt Edge Select (0=low-to-high 1=high-to-low)

#define RV3032_INT_B (4)       // Bit


// Connected to the RV3203 CLKOUT pin

#define RV3032_CLKOUT_PREN   P1REN
#define RV3032_CLKOUT_PDIR   P1DIR
#define RV3032_CLKOUT_POUT   P1OUT
#define RV3032_CLKOUT_PIN    P1IN
#define RV3032_CLKOUT_PIE    P1IE          // Interrupt enable
#define RV3032_CLKOUT_PIV    P1IV          // Interrupt vector (read this to get which pin caused interrupt, reading clears highest pending)
#define RV3032_CLKOUT_PIFG   P1IFG         // Interrupt flag (bit 1 for each pin that interrupted)
#define RV3032_CLKOUT_PIES   P1IES         // Interrupt Edge Select (0=low-to-high 1=high-to-low)
#define RV3032_CLKOUT_VECTOR PORT1_VECTOR  // ISR vector

#define RV3032_CLKOUT_VECTOR_RAM ram_vector_PORT1  // The RAM vector called when this pin ticks

#define RV3032_CLKOUT_B (1)       // Bit

// I2C data connection to RV3032 on pin P1.5 which is pin number 23 on MSP430

#define I2C_DTA_PREN P1REN
#define I2C_DTA_PDIR P1DIR
#define I2C_DTA_POUT P1OUT
#define I2C_DTA_PIN  P1IN
#define I2C_DTA_B (5)       // Bit

// I2C clock connection to RV3032 on pin P1.0 which is pin number 28 on MSP430

#define I2C_CLK_PREN P1REN
#define I2C_CLK_PDIR P1DIR
#define I2C_CLK_POUT P1OUT
#define I2C_CLK_PIN  P1IN

#define I2C_CLK_B (0)


// RV3032 Event input pin is hardwired to ground to avoid having it float during battery changes (uses lots of power)


// Power the RV3032 on pin P1.2 which is pin number 26 on MSP430

#define RV3032_VCC_PREN P1REN
#define RV3032_VCC_PDIR P1DIR
#define RV3032_VCC_POUT P1OUT
#define RV3032_VCC_PIN  P1IN

#define RV3032_VCC_B (2)

// Ground the RV3032 on pin P1.3 which is pin number 23 on MSP430
// Having the RV30332 ground come from the MCU should give extra isolation since it runs though the MSP decoupling cap
// and this also makes sure their grounds are close together.

#define RV3032_GND_PREN P1REN
#define RV3032_GND_PDIR P1DIR
#define RV3032_GND_POUT P1OUT
#define RV3032_GND_PIN  P1IN

#define RV3032_GND_B (3)



// Trigger switch pin P2.0 which is pin number 42 on MSP430. This pin uses ISR vector.

#define TRIGGER_PREN   P2REN
#define TRIGGER_PDIR   P2DIR
#define TRIGGER_POUT   P2OUT
#define TRIGGER_PIN    P2IN
#define TRIGGER_PIE    P2IE           // Interrupt enable
#define TRIGGER_PIV    P2IV           // Interrupt vector (read this to get which pin caused interrupt, reading clears highest pending)
#define TRIGGER_PIFG   P2IFG          // Interrupt flag (bit 1 for each pin that interrupted)
#define TRIGGER_PIES   P2IES          // Interrupt Edge Select (0=low-to-high 1=high-to-low)
#define TRIGGER_VECTOR PORT2_VECTOR   // ISR vector

#define TRIGGER_VECTOR_RAM ram_vector_PORT2  // The RAM vector called when this pin ticks

#define TRIGGER_B (0)



// Debug out A on pin P1.7 which is pin number 21 on MSP430

#define DEBUGA_PREN P1REN
#define DEBUGA_PDIR P1DIR
#define DEBUGA_POUT P1OUT
#define DEBUGA_PIN  P1IN

#define DEBUGA_B (7)


// Debug out B on pin P8.3 which is pin number 19 on MSP430

#define DEBUGB_PREN P1REN
#define DEBUGB_PDIR P1DIR
#define DEBUGB_POUT P1OUT
#define DEBUGB_PIN  P1IN

#define DEBUGB_B (6)


// SOLENOIDS

#define S1_PREN P7REN
#define S1_PDIR P7DIR
#define S1_POUT P7OUT
#define S1_PIN  P7IN
#define S1_B (0)       // Bit

#define S2_PREN P7REN
#define S2_PDIR P7DIR
#define S2_POUT P7OUT
#define S2_PIN  P7IN
#define S2_B (1)       // Bit

#define S3_PREN P7REN
#define S3_PDIR P7DIR
#define S3_POUT P7OUT
#define S3_PIN  P7IN
#define S3_B (2)       // Bit

#define S4_PREN P7REN
#define S4_PDIR P7DIR
#define S4_POUT P7OUT
#define S4_PIN  P7IN
#define S4_B (3)       // Bit

#define S5_PREN P7REN
#define S5_PDIR P7DIR
#define S5_POUT P7OUT
#define S5_PIN  P7IN
#define S5_B (4)       // Bit

#define S6_PREN P7REN
#define S6_PDIR P7DIR
#define S6_POUT P7OUT
#define S6_PIN  P7IN
#define S6_B (5)       // Bit


// SWITCHES - Note these are on an interrupt enabled port

#define SWITCH_MOVE_PREN   P1REN
#define SWITCH_MOVE_PDIR   P1DIR
#define SWITCH_MOVE_POUT   P1OUT
#define SWITCH_MOVE_PIN    P1IN
#define SWITCH_MOVE_PIE    P1IE           // Interrupt enable
#define SWITCH_MOVE_PIV    P1IV           // Interrupt vector (read this to get which pin caused interrupt, reading clears highest pending)
#define SWITCH_MOVE_PIFG   P1IFG          // Interrupt flag (bit 1 for each pin that interrupted)
#define SWITCH_MOVE_PIES   P1IES          // Interrupt Edge Select (0=low-to-high 1=high-to-low)
#define SWITCH_MOVE_VECTOR PORT1_VECTOR   // ISR vector
#define SWITCH_MOVE_VECTOR_RAM ram_vector_PORT1  // The RAM vector called when this pin ticks
#define SWITCH_MOVE_B (7)


#define SWITCH_CHANGE_PREN   P1REN
#define SWITCH_CHANGE_PDIR   P1DIR
#define SWITCH_CHANGE_POUT   P1OUT
#define SWITCH_CHANGE_PIN    P1IN
#define SWITCH_CHANGE_PIE    P1IE           // Interrupt enable
#define SWITCH_CHANGE_PIV    P1IV           // Interrupt vector (read this to get which pin caused interrupt, reading clears highest pending)
#define SWITCH_CHANGE_PIFG   P1IFG          // Interrupt flag (bit 1 for each pin that interrupted)
#define SWITCH_CHANGE_PIES   P1IES          // Interrupt Edge Select (0=low-to-high 1=high-to-low)
#define SWITCH_CHANGE_VECTOR PORT1_VECTOR   // ISR vector
#define SWITCH_CHANGE_VECTOR_RAM ram_vector_PORT1  // The RAM vector called when this pin ticks
#define SWITCH_CHANGE_B (6)


// TSP voltage regulator

#define TSP_ENABLE_PREN P7REN
#define TSP_ENABLE_PDIR P7DIR
#define TSP_ENABLE_POUT P7OUT
#define TSP_ENABLE_PIN  P7IN
#define TSP_ENABLE_B (0)       // Bit

#define TSP_IN_PREN P4REN
#define TSP_IN_PDIR P4DIR
#define TSP_IN_POUT P4OUT
#define TSP_IN_PIN  P4IN
#define TSP_IN_B (2)       // Bit




#endif /* PINS_H_ */
