#include <msp430.h>
#include "util.h"
#include "pins.h"
#include "i2c_master.h"

#include "error_codes.h"

#include "ram_isrs.h"

#include "tsl_asm.h"

#include "define_lcd_pinout.h"
#include "define_lcd_to_msp430_connections.h"
#include "define_msp430_lcd_device.h"
#include "define_lcd_font.h"

#include "lcd_display.h"
#include "lcd_display_exp.h"

#define RV_3032_I2C_ADDR (0b01010001)           // Datasheet 6.6

// RV3032 Registers

#define RV3032_HUNDS_REG 0x00    // Hundredths of seconds
#define RV3032_SECS_REG  0x01
#define RV3032_MINS_REG  0x02
#define RV3032_HOURS_REG 0x03
#define RV3032_DAYS_REG  0x05
#define RV3032_MONS_REG  0x06
#define RV3032_YEARS_REG 0x07

// Used to time how long ISRs take with an oscilloscope

/*
// DEBUG VERSIONS
#define DEBUG_PULSE_ON()     SBI( DEBUGA_POUT , DEBUGA_B )
#define DEBUG_PULSE_OFF()    CBI( DEBUGA_POUT , DEBUGA_B )
*/

// PRODUCTION VERSIONS
#define DEBUG_PULSE_ON()     {}
#define DEBUG_PULSE_OFF()    {}


// If this is defined, the LCD will use an optional TPS7A voltage regulator attached to the bias pin.
// If it is not defined, the we will configure the LCD to use the internal voltage reference.
// Using the TPS7A reduces LCD total power significantly (about 40%), at the cost of an extra part.
// Note that if a TPS7A is connected to pin 33 and the internal reference is enabled, it will waste lots of power even if the TPS7A is not enabled.
#define USE_TPS7A_LCD_BIAS


// Put the LCD into blinking mode

void lcd_blinking_mode() {
    LCDBLKCTL = LCDBLKPRE__64 | LCDBLKMOD_2;       // Clock prescaler for blink rate, "10b = Blinking of all segments"
}


// Turn off power to RV3032 (also takes care of making the IO pin not float and disabling the inetrrupt)
void depower_rv3032() {

    CBI( RV3032_CLKOUT_PIE , RV3032_CLKOUT_B );     // Disable interrupt so we do not get a spurious one doing this stuff.

    // Make Vcc pin to RV3032 ground.
    CBI( RV3032_VCC_POUT , RV3032_VCC_B);

    // Now the CLKOUT pin is floating, so we will pull it
    SBI( RV3032_CLKOUT_PREN , RV3032_CLKOUT_B );    // Enable pull resistor (does not matter which way, just keep pin from floating to save power)

}

// Goes into LPM3.5 to save power since we can never wake from here.
// In LPMx.0 draws 1.38uA with the "First STart" message.
// In LPMx.5 draws 1.13uA with the "First STart" message.

// Clearing GIE does not prevent wakeup from LPMx.5 so all interrupts must have been individually disabled.
#pragma FUNC_NEVER_RETURNS
void sleepforeverandever(){

    // These lines enable the LPMx.5 modes
    PMMCTL0_H = PMMPW_H;                    // Open PMM Registers for write
    PMMCTL0_L |= PMMREGOFF_L;               // and set PMMREGOFF

    // Make sure no pending interrupts here or else they might wake us!
    __bis_SR_register(LPM3_bits);     // // Enter LPM4 and sleep forever and ever and ever (no interrupts enabled so we will never get woken up)

}

// Assumes all interrupts have been individually disabled.
#pragma FUNC_NEVER_RETURNS
void blinkforeverandever(){
    lcd_blinking_mode();
    sleepforeverandever();
}

// Assumes all interrupts have been individually disabled.
#pragma FUNC_NEVER_RETURNS
void error_mode( byte code ) {
    lcd_show_errorcode(code);
    blinkforeverandever();
}


// Does not enable interrupts on any pins

inline void initGPIO() {

    // Default pin init. We will also (possabibly duplicatively) init individual pins
    // TODO: use the full word-wide registers
    // Initialize all GPIO pins for low power OUTPUT + LOW
    P1DIR = 0xFF;P2DIR = 0xFF;P3DIR = 0xFF;P4DIR = 0xFF;
    P5DIR = 0xFF;P6DIR = 0xFF;P7DIR = 0xFF;P8DIR = 0xFF;
    // Configure all GPIO to Output Low
    P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
    P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

    // --- Solenoid MOSFETs off

    // Set Solenoid transistor pins to output off
    // Note that they are also pulled down by 1K ohm resistors so they do not float before we get here during reset
    CBI( S1_POUT , S1_B );
    CBI( S2_POUT , S2_B );
    CBI( S3_POUT , S3_B );
    CBI( S4_POUT , S4_B );
    CBI( S5_POUT , S5_B );
    CBI( S6_POUT , S6_B );

    SBI( S1_PDIR , S1_B );
    SBI( S2_PDIR , S2_B );
    SBI( S3_PDIR , S3_B );
    SBI( S4_PDIR , S4_B );
    SBI( S5_PDIR , S5_B );
    SBI( S6_PDIR , S6_B );


    // --- Optional TPS7A regulator external bias voltage source

    // TODO: Test power savings from regulator to see if it is worth the extra part.

    // Make the pins output. They will stay low if the TPS7A is not configured.
    // There is other code in initLCD that will turn on the internal bias generator if USE_TPS7A_LCD_BIAS is not defined.

    // Set all the pins to the TPS7A to out
    // This will ground them if the TPS7A is not enabled so they do not float.
    SBI( TSP_IN_PDIR , TSP_IN_B );          // Power in pin
    SBI( TSP_ENABLE_PDIR , TSP_ENABLE_B );  // Enable pin

    #ifdef USE_TPS7A_LCD_BIAS

        SBI( TSP_IN_POUT , TSP_IN_B );          // Power up TPS7A
        SBI( TSP_ENABLE_POUT , TSP_ENABLE_B );  // Enable TSP

    #endif

    // --- Debug pins

    // Debug pins
    SBI( DEBUGA_PDIR , DEBUGA_B );          // DEBUGA=Output. Currently used to time how long the ISR takes

    CBI( DEBUGB_PDIR , DEBUGB_B );          // DEBUGB=Input
    SBI( DEBUGB_POUT , DEBUGB_B );          // Pull up
    SBI( DEBUGB_PREN , DEBUGB_B );

    // --- RV3032

    // ~INT in from RV3032 is an open collector output.
    // We don't use this for now, so set to drive low to avoid any power leakage.

    SBI( RV3032_INT_PDIR , RV3032_INT_B ); // OUTPUT, default low

    // Note that we can leave the RV3032 EVI pin as is - tied LOW on the PCB so it does not float (we do not use it)
    // It is hard tied low so that it does not float during battery changes when the MCU will not be running

    // Power to RV3032

    CBI( RV3032_GND_POUT , RV3032_GND_B);
    SBI( RV3032_GND_PDIR , RV3032_GND_B);

    SBI( RV3032_VCC_PDIR , RV3032_VCC_B);
    SBI( RV3032_VCC_POUT , RV3032_VCC_B);

    // Now we need to setup interrupt on RTC CLKOUT pin to wake us on each rising edge (1Hz)

    CBI( RV3032_CLKOUT_PDIR , RV3032_CLKOUT_B );    // Set input
    CBI( RV3032_CLKOUT_PIES , RV3032_CLKOUT_B );    // Interrupt on rising edge (Driven by RV3032 CLKOUT pin which we program to run at 1Hz).

    /* RV3230 App Guide Section 4.21.1:
     *  the 1 Hz clock can be enabled
     *  on CLKOUT pin. The positive edge corresponds to the 1 Hz tick for the clock counter increment (except for
     *  the possible positive edge when the STOP bit is cleared).
     */

    // Note that we do not enable the CLKOUT interrupt until we enter RTL or TSL mode

    // --- Trigger

    // By default we set the trigger to drive low so it will not use power regardless if the pin is in or out.
    // When the trigger is out, the pin is shorted to ground.
    // We will switch it to pull-up later if we need to (because we have not launched yet).

    CBI( TRIGGER_POUT , TRIGGER_B );      // low
    SBI( TRIGGER_PDIR , TRIGGER_B );      // drive
    SBI( SWITCH_CHANGE_PIES , TRIGGER_B );      // Interrupt on high-to-low transition (pin pulled up, switch connects to ground)

    // --- Buttons

    // By default we set the switches to drive low so it will not use power in case the button is stuck or shorted
    // We will switch it to pull-up later if we need to (because we have not launched yet).

    CBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );      // low
    SBI( SWITCH_MOVE_PDIR , SWITCH_MOVE_B );      // drive
    SBI( SWITCH_MOVE_PREN , SWITCH_MOVE_B );      // Set pull mode to UP when POUT is high and PDIR is low (no effect when pin is in OUTPUT mode)
    SBI( SWITCH_MOVE_PIES , SWITCH_MOVE_B );      // Interrupt on high-to-low transition (pin pulled up, button connects to ground)

    CBI( SWITCH_CHANGE_POUT , SWITCH_CHANGE_B );      // low
    SBI( SWITCH_CHANGE_PDIR , SWITCH_CHANGE_B );      // drive
    SBI( SWITCH_CHANGE_PREN , SWITCH_CHANGE_B );      // Set pull mode to UP when POUT is high and PDIR is low (no effect when pin is in OUTPUT mode)
    SBI( SWITCH_CHANGE_PIES , SWITCH_CHANGE_B );      // Interrupt on high-to-low transition (pin pulled up, button connects to ground)

    // Note that we do not enable the trigger pin interrupt here. It will get enabled when we
    // switch to ready-to-lanch mode when the trigger is inserted at the factory. The interrupt will then get
    // disabled again and the pull up will get disabled after the trigger is pulled since once that happens we
    // dont care about the trigger state anymore and we don't want to waste power with either the pull-up getting shorted to ground
    // if they leave the trigger out, or from unnecessary ISR calls if they put the trigger in and pull it out again (or if it breaks and bounces).

    // Disable IO on the LCD power pins
    SYSCFG2 |= LCDPCTL;

    // Disable the GPIO power-on default high-impedance mode
    // to activate the above configured port settings
    PM5CTL0 &= ~LOCKLPM5;

}

void initLCD() {

    // Configure LCD pins
    SYSCFG2 |= LCDPCTL;                                 // LCD R13/R23/R33/LCDCAP0/LCDCAP1 pins enabled

    LCDBLKCTL = 0x00;           // "Settings for LCDMXx and LCDBLKPREx should only be changed while LCDBLKMODx = 00."
                                // And note that this register is NOT cleared on reset, so we clear it here.

    // Enable LCD pins as defined in the LCD connections header
    // Iterate though each of the define LPINs

    for( byte lpin  : lcdpin_to_lpin ) {

        if (lpin <16 ) {
            if (lpin>0) {                       // Note that we ignore 0 here because we use 0 as a placeholder. This is ugly.
                LCDPCTL0 |= 1 << (lpin-0);
            }
        } else if (lpin <32 ) {
            LCDPCTL1 |= 1 << (lpin-16);
        } else if (lpin <48 ) {
            LCDPCTL2 |= 1 << (lpin-32);
        }

    }

    // LCDCTL0 = LCDSSEL_0 | LCDDIV_7;                     // flcd ref freq is xtclk

    // TODO: Try different clocks and dividers

    // Divide by 2 (so CLK will be 10KHz/2= 5KHz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON)
    // Note this has a bit of a flicker
    //LCDCTL0 = LCDDIV_2 | LCDSSEL__VLOCLK | LCD4MUX | LCDSON | LCDON  ;

    // TODO: Try different clocks and dividers
    // Divide by 1 (so CLK will be 10Khz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON)
    //LCDCTL0 = LCDDIV_1 | LCDSSEL__VLOCLK | LCD4MUX | LCDSON | LCDON  ;

    // Divide by 1 (so CLK will be 10Khz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON), Low power waveform
    //LCDCTL0 = LCDDIV_1 | LCDSSEL__VLOCLK | LCD4MUX | LCDSON | LCDON | LCDLP ;

    // LCD using VLO clock, divide by 4 (on 10KHz from VLO) , 4-mux (LCD4MUX also includes LCDSON), low power waveform. No flicker. Squiggle=1.45uA. Count=2.00uA. I guess not worth the flicker for 0.2uA?
    LCDCTL0 =  LCDSSEL__VLOCLK | LCDDIV__4 | LCD4MUX | LCDLP ;

    // LCD using VLO clock, divide by 5 (on 10KHz from VLO) , 4-mux (LCD4MUX also includes LCDSON), low power waveform. Visible flicker at large view angles. Squiggle=1.35uA. Count=1.83uA
    //LCDCTL0 =  LCDSSEL__VLOCLK | LCDDIV__5 | LCD4MUX | LCDLP ;

    // LCD using VLO clock, divide by 6 (on 10KHz from VLO) , 4-mux (LCD4MUX also includes LCDSON), low power waveform. VISIBLE FLICKER at 3.5V
    //LCDCTL0 =  LCDSSEL__VLOCLK | LCDDIV__6 | LCD4MUX | LCDLP ;


/*
    // Divide by 32 (so CLK will be 32768/32 = ~1KHz), Very Low Osc, Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON)
    LCDCTL0 = LCDDIV_7 | LCDSSEL__XTCLK | LCD4MUX | LCDSON | LCDON  ;
*/


    // LCD Operation - Mode 3, internal 3.08v, charge pump 256Hz, ~5uA from 3.5V Vcc
    //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);



    // LCD Operation - internal V1 regulator=3.32v , charge pump 256Hz
    // LCDVCTL = LCDCPEN | LCDREFEN | VLCD_12 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


    // LCD Operation - Pin R33 is connected to external Vcc, charge pump 256Hz, 1.7uA
    //LCDVCTL = LCDCPEN | LCDSELVDD | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);


    // LCD Operation - Pin R33 is connected to internal Vcc, no charge pump
    //LCDVCTL = LCDSELVDD;


    // LCD Operation - Pin R33 is connected to external V1, charge pump 256Hz, 1.7uA
    //LCDVCTL = LCDCPEN | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);



    // LCD Operation - Mode 3, internal 3.02v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.2uA from 3.5V Vcc
    //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_7 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


    // LCD Operation - Mode 3, internal 2.78v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.2uA from 3.5V Vcc
    //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_3 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


    // LCD Operation - Mode 3, internal 2.78v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.0uA from 3.5V Vcc
    //LCDVCTL = LCDCPEN | LCDREFEN | VLCD_3 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


    // LCD Operation - All 3 LCD voltages external. When generating all 3 with regulators, we get 2.48uA @ Vcc=3.5V so not worth it.
    //LCDVCTL = 0;


    // LCD Operation - All 3 LCD voltages external. When generating 1 regulators + 3 1M Ohm resistors, we get 2.9uA @ Vcc=3.5V so not worth it.
    //LCDVCTL = 0;


    // LCD Operation - Charge pump enable, Vlcd=Vcc , charge pump FREQ=/256Hz (lowest)  2.5uA - Good for testing without a regulator
    //LCDVCTL = LCDCPEN |  LCDSELVDD | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);



    // LCD Operation - Charge pump enable, Vlcd=external from R33 pin , charge pump FREQ=/64Hz . 2.1uA/180uA  @ Vcc=3.5V . Vlcd=2.8V  from TPS7A0228 no blinking.
    //LCDVCTL = LCDCPEN |   (LCDCPFSEL0 | LCDCPFSEL1 );



    #ifdef USE_TPS7A_LCD_BIAS

        /* WINNER for controlled Vlcd - Uses external TSP7A regulator for Vlcd on R33 */
        // LCD Operation - Charge pump enable, Vlcd=external from R33 pin , charge pump FREQ=/256Hz (lowest). 2.1uA/180uA  @ Vcc=3.5V . Vlcd from TPS7A0228 no blinking.
        LCDVCTL = LCDCPEN |   (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);

    #else

        // LCD Operation - Mode 3, internal 2.96v, charge pump 256Hz, voltage reference only on 1/256th of the time. ~4.2uA from 3.5V Vcc
        LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3) | LCDREFMODE;


    #endif


    LCDMEMCTL |= LCDCLRM;                                      // Clear LCD memory
    while ( LCDMEMCTL & LCDCLRM );                             // Wait for clear to complete.

    LCDMEMCTL |= LCDCLRBM;                                     // Clear LCD blink memory
    while ( LCDMEMCTL & LCDCLRBM );                            // Wait for clear to complete.

    // Configure COMs and SEGs


    // TODO: This should be parameterized.
    LCDCSSEL0 = LCDCSS8 | LCDCSS9 | LCDCSS10 | LCDCSS11 ;     // L8-L11 are the 4 COM pins
    LCDCSSEL1 = 0x0000;
    LCDCSSEL2 = 0x0000;

    // L08 = MSP_COM0 = LCD_COM1
    // L09 = MSP_COM1 = LCD_COM2
    // L10 = MSP_COM2 = LCD_COM3
    // L11 = MSP_COM3 = LCD_COM4

    // Once we have selected the COM lines above, we have to connect them in the LCD memory. See Figure 17-2 in MSP430FR4x family guide.
    // Each nibble in the LCDMx regs holds 4 bits connecting the L pin to one of the 4 COM lines (two L pins per reg)
    // Note if you change these then you also have to adjust lcd_show_squigle_animation()

    LCDM4 =  0b00100001;  // L09=MSP_COM1  L08=MSP_COM0
    LCDM5 =  0b10000100;  // L10=MSP_COM3  L11=MSP_COM2

    // Enable per-segment blinking. If the bit is set in LCDMBEM then the segment will blink.
    // The blink speed is pretty fast, about 10Hz. We use this to indicate which digit is selected
    // in SETTING mode.
    // "Settings for LCDMXx and LCDBLKPREx should only be changed while LCDBLKMODx = 00.""

    LCDBLKCTL =
            LCDBLKPRE__8 |            // Blinking frequency prescaller. This controls how fast the blink is.
            LCDBLKMOD_1               // Individual control of blinking segments from LCDBM
    ;


    LCDCTL0 |= LCDON;                                           // Turn on LCD

}


// Low voltage flag indicates that the RTC has been re-powered and potentially lost its data.

// Initialize RV3032 for the first time
// Clears the low voltage flag
// sets clkout to 1Hz
// Disables backup function
// Does not enable any interrupts

void rv3032_init() {

    // Give the RV3230 a chance to wake up before we start pounding it.
    // POR refresh time(1) At power up ~66ms
    // Also there is Tdeb which is the time it takes to recover from a backup switch over back to Vcc. It is unclear if this is 1ms or 1000ms so lets be safe.
    __delay_cycles(1100000);    // 1 sec +/-10% (we are running at 1Mhz)

    // Initialize our i2c pins as pull-up
    i2c_init();


    // Set all the registers we care about that can get reset by either power-on-reset or recover from backup
    //uint8_t clkout2_reg = 0b00000000;        // CLKOUT XTAL low freq mode, freq=32768Hz
    //uint8_t clkout2_reg = 0b00100000;        // CLKOUT XTAL low freq mode, freq=1024Hz
    uint8_t clkout2_reg = 0b01100000;        // CLKOUT XTAL low freq mode, freq=1Hz

    i2c_write( RV_3032_I2C_ADDR , 0xc3 , &clkout2_reg , 1 );

    // First control reg. Note that turning off backup switch-over seems to save ~0.1uA
    //uint8_t pmu_reg = 0b01010000;          // CLKOUT off, Direct backup switching mode, no charge pump, 0.6K OHM trickle resistor, trickle charge Vbackup to Vdd. Only predicted to use 50nA more than disabled.
    //uint8_t pmu_reg = 0b01100001;         // CLKOUT off, Level backup switching mode (2v) , no charge pump, 1K OHM trickle resistor, trickle charge Vbackup to Vdd. Predicted to use ~200nA more than disabled because of voltage monitor.
    //uint8_t pmu_reg = 0b01000000;         // CLKOUT off, Other disabled backup switching mode, no charge pump, trickle resistor off, trickle charge Vbackup to Vdd
    //uint8_t pmu_reg = 0b00011101;          // CLKOUT ON, Direct backup switching mode, no charge pump, 12K OHM trickle resistor, trickle charge Vbackup to Vdd.
    //uint8_t pmu_reg = 0b01000000;         // CLKOUT off, backup switchover disabled, no charge pump, 1K OHM trickle resistor, trickle charge off.
    uint8_t pmu_reg = 0b00000000;         // CLKOUT on, backup switchover disabled, no charge pump, 1K OHM trickle resistor, trickle charge off.


    // TODO: which is lower power, INT or CLKOUT?

    i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );

    uint8_t control1_reg = 0b00000100;      // TE=0 so no periodic timer interrupt, EERD=1 to disable automatic EEPROM refresh (why would you want that?).
    i2c_write( RV_3032_I2C_ADDR , 0x10 , &control1_reg , 1 );

    i2c_shutdown();

}

// Clear the low voltage flags. These flags remember if the chip has seen a voltage low enough to make it loose time.

void rv3032_clear_LV_flags() {

    // Initialize our i2c pins as pull-up
    i2c_init();

    uint8_t status_reg=0x00;        // Set all flags to 0. Clears out the low voltage flag so if it is later set then we know that the chip lost power.
    i2c_write( RV_3032_I2C_ADDR , 0x0d , &status_reg , 1 );

    i2c_shutdown();

}


// Turn off the RTC to save some power. Only use this if you will NEVER need the RTC again (like if you are going into error mode).

void rv3032_shutdown() {

    i2c_init();

    uint8_t pmu_reg = 0b01000000;         // CLKOUT off, backup switchover disabled, no charge pump
    i2c_write( RV_3032_I2C_ADDR , 0xc0 , &pmu_reg , 1 );

    i2c_shutdown();

}


// This will reset the prescaller in the RTC so that the next tick will come 1000ms from now.

void rv3032_zero() {
    // Initialize our i2c pins as pull-up
    i2c_init();

    unsigned char zero =0;

    // Then zero out the RTC to start counting over again, starting now. Note that writing any value to the seconds register resets the sub-second counters to the beginning of the second.
    // "Writing to the Seconds register creates an immediate positive edge on the LOW signal on CLKOUT pin."
    i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &zero , sizeof( zero ) );

    i2c_shutdown();
}

// Shortcuts for setting the RAM vectors. Note we need the (void *) casts because the compiler won't let us make the vectors into `near __interrupt (* volatile vector)()` like it should.

#define SET_CLKOUT_VECTOR(x) do {RV3032_CLKOUT_VECTOR_RAM = (void *) x;} while (0)
#define SET_TRIGGER_VECTOR(x) do {TRIGGER_VECTOR_RAM = (void *) x;} while (0)

// Terminate after one day
bool testing_only_mode = false;

// Called when trigger pin changes high to low, indicating the trigger has been pulled and we should start ticking.
// Note that this interrupt is only enabled when we enter ready-to-launch mode, and then it is disabled and also the pin in driven low
// when we then switch to time-since-lanuch mode, so this ISR can only get called in ready-to-lanuch mode.

__interrupt void trigger_isr(void) {

    DEBUG_PULSE_ON();

    // First we delay for about 1000 cycles / 1.1Mhz  = ~1ms
    // This will filter glitches since the pin will not still be low when we sample it after this delay. We want to be really sure!
    __delay_cycles( 1000 );

    // Then clear any interrupt flag that got us here.
    // Timing here is critical, we must clear the flag *before* we capture the pin state or we might miss a change
    CBI( TRIGGER_PIFG , TRIGGER_B );      // Clear any pending trigger pin interrupt flag

    // Grab the current trigger pin level
    unsigned triggerpin_level = TBI( TRIGGER_PIN , TRIGGER_B );

    // Now we can check to see if the trigger pin was still low
    // If it is not still low then we will return and this ISR will get called again if it goes low again or
    // if it went low between when we cleared the flag and when we sampled it (VERY small race window of only 1/8th of a microsecond, but hey we need to be perfect, it could be a once in a lifetime moment and we dont want to miss it!)

    if ( !triggerpin_level ) {      // trigger still pulled?

        // WE HAVE LIFTOFF!

        // Disable the trigger pin and drive low to save power and prevent any more interrupts from it
        // (if it stayed pulled up then it would draw power though the pull-up resistor whenever the pin was out)

        CBI( TRIGGER_POUT , TRIGGER_B );      // low
        SBI( TRIGGER_PDIR , TRIGGER_B );      // drive

        CBI( TRIGGER_PIFG , TRIGGER_B );      // Clear any pending interrupt (it could have gone high again since we sampled it). We already triggered so we don't care anymore.

        // Show all 0's on the LCD to quickly let the user know that we saw the pull
        lcd_show_zeros();

        // Do the actual launch, which will...
        // 1. Read the current RTC time and save it to FRAM
        // 2. Set the launchflag in FRAM so we will forevermore know that we did launch already.
        // 3. Reset the current RTC time to midnight 1/1/00.


        // Note that the time variables will already be initialized to zero from power up

        // We will get the next tick with a rising edge on CLKOUT in 1000ms. Make sure we return from this ISR before then.

        // Clear any pending RTC interrupt so we will not tick until the next pulse comes from RTC in 1000ms
        // There could be a race where an RTC interrupt comes in just after the trigger was pulled, in which case we would
        // have a pending interrupt that would get serviced immediately after we return from here, which would look ugly.
        // We could also see an interrupt if the CLKOUT signal was low when trigger pulled (50% likelihood) since it will go high on reset.

        CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear any pending interrupt from CLKOUT

        // Start ticking from... now!
        // (We rely on the secs, mins, hours, and days_digits[] all having been init'ed to zeros.)


    }

    DEBUG_PULSE_OFF();

}

/*

 1024hz RTL pattern readings with Energytrace over 5 mins
 FRPWR is cleared on each ISR call with the following line at the very top...
      BIC.W     #FRPWR,(GCCTL0) ;           // Disable FRAM. Writing to the register enables or disables the FRAM power supply.  0b = FRAM power supply disabled.
 It would be nice if we could permanently disable the FRAM, but does not seem possible on this chip because it is automatically enabled when exiting LPM.

 RAMFUNC  FRPWR  CLKOUT   I
 =======  =====  =====    =======
   Y        0     1kHz     0.354mA
   Y        0     1kHz     0.351mA
   Y        1     1kHz     0.385mA
   Y        1     1kHz     0.371mA
   Y        1     1kHz     0.372mA
   Y        1     1Hz      0.0021mA
   Y        1     1Hz      0.0021mA
   Y        0     1Hz      0.0021mA
   N        1     1Hz      0.0027mA
   N        1     1Hz      0.0027mA

*/



unsigned int step;          // LOAD_TRIGGER      - Used to keep the position of the dash moving across the display (0=leftmost position, only one dash showing)
                            // READY_TO_LAUNCH   - Which step of the squigle animation



// Spread the 6 day counter digits out to minimize superfluous digit updates and avoid mod and div operations.
// These are set either when trigger is pulled or on power up after a battery change.

unsigned days_digits[6];

__interrupt void post_centiday_isr(void) {

    lcd_show_digit_f( 0 , 1 );
    lcd_show_digit_f( 1 , 0 );
    lcd_show_digit_f( 2 , 0 );
    lcd_show_digit_f( 3 , 0 );
    lcd_show_digit_f( 4 , 0 );
    lcd_show_digit_f( 5 , 0 );


    lcd_show_digit_f(  6 , days_digits[0] );
    lcd_show_digit_f(  7 , days_digits[1] );
    lcd_show_digit_f(  8 , days_digits[2] );
    lcd_show_digit_f(  9 , days_digits[3] );
    lcd_show_digit_f( 10 , days_digits[4] );
    lcd_show_digit_f( 11 , days_digits[5] );

    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );      // Clear the pending RV3032 INT interrupt flag that got us into this ISR.

}


// Called by the ASM TSL_MODE_ISR when it rolls over from 23:59:59 to 00:00:00
// Note that since this is a `void` C++ function, the linker name gets mangled to `tsl_next_dayv`
#pragma RETAIN
void tsl_next_day() {

    // First take care of our century book keeping...

    // If we get here, then the 100 day counter has clicked so we need to do special update
    // and increment remaining digits for next tick.

    days_digits[1]=0x00;

    lcd_show_centesimus_dies_message();

#warning
    //SET_CLKOUT_VECTOR( post_centiday_isr );

    if (days_digits[2] < 9) {
        days_digits[2]++;
        return;
    }
    days_digits[2]=0x00;

    if (days_digits[3] < 9) {
        days_digits[3]++;
        return;
    }
    days_digits[3]=0x00;

    if (days_digits[4] < 9) {
        days_digits[4]++;
        return;
    }
    days_digits[4]=0x00;

    if (days_digits[5] < 9) {
        days_digits[5]++;
        return;
    }

    // If we get here then we just passed 1 million days, so go into long now mode.

    lcd_show_long_now();
    blinkforeverandever();

}




void solenoidOn( unsigned s ) {
    switch (s) {
    case 1:     SBI( S1_POUT , S1_B ); break;
    case 2:     SBI( S2_POUT , S2_B ); break;
    case 3:     SBI( S3_POUT , S3_B ); break;
    case 4:     SBI( S4_POUT , S4_B ); break;
    case 5:     SBI( S5_POUT , S5_B ); break;
    case 6:     SBI( S6_POUT , S6_B ); break;
    }

}

void solenoidOff( unsigned s ) {
    switch (s) {
    case 1:     CBI( S1_POUT , S1_B ); break;
    case 2:     CBI( S2_POUT , S2_B ); break;
    case 3:     CBI( S3_POUT , S3_B ); break;
    case 4:     CBI( S4_POUT , S4_B ); break;
    case 5:     CBI( S5_POUT , S5_B ); break;
    case 6:     CBI( S6_POUT , S6_B ); break;
    }

}


void fire_solenoid( unsigned s) {


    // First completely pull 1
    solenoidOn(s);
    __delay_cycles( 50000 );
    solenoidOff(s);

}


// Note that solenoid numbers match the PCB markings and range 1-6,
// where 1 is at 1 oclock and the go counter clockwise from there

void fire_solenoids() {


    // First completely pull 1
    fire_solenoid(1);
    fire_solenoid(2);
    fire_solenoid(1);
    fire_solenoid(2);

    // Next lets try to dither in 2
/*
    for( unsigned i = 0; i < 50 ; i ++ ) {

        solenoidOn(1);
        __delay_cycles( 100 );
        solenoidOff(1);

        solenoidOn(2);
        __delay_cycles( 900 );
        solenoidOff(2);


    }
*/
}

void open_lock() {

    // For now just open 1 and 2 for testing
    // TODO: Activate all solenoids in sequence

    solenoidOn(1);
    solenoidOn(2);

    __delay_cycles( 50000 );

    solenoidOff(1);
    solenoidOff(2);

}

// These two countdown vars keep track of how long until we unlock. We do not initialize them since they will get set when
// we transition from setting mode to locked mode. They are in infoA FRAM so they will persist though resets and power cycles.
// These are updated every minute while we are counting down in locked mode so in case we reset or lose power,
// then we will come back up and restart where we left off. They are kept in FRAM which is persistent across power cycles,
// and writing to these to update them only takes a single instruction.

// It would be nice to keep these in a single unsigned long, but since the MSP430 only guarantees that writes to a single
// unsigned int in FRAM are atomic, we store the two words separately so we can do very efficient writes to the low word
// and only do ACID transaction writes when we need to update the high word (which is only once every 2^16 minutes ~= every 45 days)

// We set backup_countdown_time_active_flag when we start an update and clear it when the update it complete.
// When the backup_countdown_time_active_flag==true then use the backup_countdown_time, otherwise use countdown_time.


struct countdown_time_t {
    unsigned int countdown_mins_h;
    unsigned int countdown_mins_l;
};


// Collect up everything we want to have be persistent here to keep it organized.
// We depend on these being initialized to 0 at the factory.
struct persistant_data_t {

    countdown_time_t countdown_time;
    countdown_time_t backup_countdown_time;
    unsigned backup_countdown_time_active_flag;

};

// Tell compiler/linker to put this in "info memory" at 0x1800
// This area of memory never gets overwritten, not by power cycle and not by downloading a new binary image into program FRAM.
// We have to mark `volatile` so that the compiler will really go to the FRAM every time rather than optimizing out some accesses.
// We depend on the programming process to clear this to zero so we can tell if we are starting from factory or restarting after reset or battery change.
volatile persistant_data_t __attribute__(( __section__(".infoA") )) persistent_data;

void unlock_persistant_data() {
    SYSCFG0 = PFWP;                     // Write protect only program FRAM. Interestingly it appears that the password is not needed here?
}

void lock_persistant_data() {
    SYSCFG0 = PFWP | DFWP;              // Write protect both program and data FRAM.
}


// Here are our actual working variables that stay in RAM
// These all get initialized at the moment the device is locked, or
// if we boot up and detect that we were already running.

// These are displayed
unsigned int d,h,m,s;

uint8_t  rtc_secs=0;
uint8_t  rtc_mins=0;
uint8_t  rtc_hours=0;
uint16_t rtc_days=0;       // only needs to be able to hold up to 1 century of days since RTC rolls over after 100 years.


// These are for safekeeping in case we reset.
// Note that these are redundant, but it is faster to keep them in this separate format

countdown_time_t countdown_time;


// Start calling the clkout vector above on each tick form the RTC

void enable_rv3032_clkout_interrupt() {
    // Now we enable the interrupt on the RTC CLKOUT pin. For now on we must remember to
    // disable it again if we are going to end up in sleepforever mode.

    // Clear any pending interrupts from the RV3032 clkout pin and then enable interrupts for the next falling edge
    // We we should not get a real one for 500ms so we have time to do our stuff

    CBI( RV3032_CLKOUT_PIFG     , RV3032_CLKOUT_B    );
    SBI( RV3032_CLKOUT_PIE      , RV3032_CLKOUT_B    );
}


void disable_rv3032_clkout_interrupt() {

    CBI( RV3032_CLKOUT_PIE      , RV3032_CLKOUT_B    );
    CBI( RV3032_CLKOUT_PIFG     , RV3032_CLKOUT_B    );

}


// Timer interrupt
// Should fire once per second

#pragma vector=RV3032_CLKOUT_VECTOR
__interrupt void clkout_isr(void) {

    // CLear the pending interrupt.
    // Do this first since there are a couple ways we can exit this ISR
    RV3032_CLKOUT_PIV;          // Implemented as "MOV.W   &Port_1_2_P2IV,R15"


    // We order operations here so that we open at the transition from 1 sec -> 0 sec
    s--;

    if (s==0) {
        if (m==0) {
            if (h==0) {
                if (d==0) {

                    /// Time to open!!!!

                    // First disable the clock interrupt since will do not want it anymore

                    disable_rv3032_clkout_interrupt();

                    open_lock();

                    // Now go back to setting mode.

                    // TODO: enter_setting_mode();

                }
                h=24;
                d--;

                lcd_show_days_lcdbmem(d);

            }
            m=60;
            h--;

            *hours_lcdmemw = hours_lcd_words[h];

        }
        s=59;           // Yea I know this looks wrong, but we decremented at the top already.
        m--;

        *mins_lcdmemw = mins_lcd_words[m];

        // TODO: Update the recovery data here.

    }

    *secs_lcdmemw = secs_lcd_words[s];

    /*
        // Wow, this compiler is not good. Below we can remove a whole instruction with 3 cycles that is completely unnecessary.

        asm("        MOV.B     &s+0,r15           ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        RLAM.W    #1,r15                ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        MOV.W     secs_lcd_words+0(r15),(LCDM0W_L+16) ; [] |../tsl-calibre-msp.cpp:1390|");
    */


}



void setting_mode() {

    enum units_t {
        YEARS,
        DAYS,
        HOURS,
        SECONDS
    };


    units_t current_unit = units_t::HOURS;
    unsigned long current_value = 1;        // Current value

    enum cursor_t {
        THOUSANDS,
        HUNDRES,
        TENS,
        ONES,
        UNITS
    };

    byte current_cursor = cursor_t::ONES;




}



// When the switch bit here is 1, then button is up and we will interrupt high-to-low,
// and we will register a press.


// Cases:
// Switch is armed and goes down and is still down after debounce - interrupt to up
// Switch is armed and goes down and is not still down afer debounce - do nothing, we will get another press when it goes down again.

// Switch is not armed and goes down - ignore
//

unsigned switch_armed_flags;

void enable_button_interrupts() {

    // Disable pin drive

    CBI( SWITCH_MOVE_PDIR , SWITCH_MOVE_B );
    CBI( SWITCH_CHANGE_PDIR , SWITCH_CHANGE_B );

    // Pull-up pins
    // Note that we set the pull mode to UP in initGPIO
    // and setting out to 1 when PDIR is clear will enable the pull-up

    SBI( SWITCH_CHANGE_POUT , SWITCH_CHANGE_B );
    SBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );

    // We do not need to wait for the resistors to take effect since we initially will only interrupt
    // on high to low transitions and the pulls will cause a low to high.


    // Initially Interrupt on the next high-to-low transitions
    SBI( SWITCH_CHANGE_PIES , SWITCH_CHANGE_B );
    SBI( SWITCH_MOVE_PIES , SWITCH_MOVE_B );

    // Clear any pending interrupts
    CBI( SWITCH_CHANGE_PIFG , SWITCH_CHANGE_B    );
    CBI( SWITCH_MOVE_PIFG , SWITCH_MOVE_B    );

    // Arm the buttons so we register the next press
    SBI( switch_armed_flags , SWITCH_CHANGE_B);
    SBI( switch_armed_flags , SWITCH_MOVE_B);

    // Enable pin change interrupt on the pins
    SBI( SWITCH_CHANGE_PIE , SWITCH_CHANGE_B );
    SBI( SWITCH_MOVE_PIE , SWITCH_MOVE_B );

}


#if (SWITCH_CHANGE_VECTOR != SWITCH_MOVE_VECTOR)
    #error "This code assumes that both buttons are on the same interrupt vector"
#endif


void testSolenoid(unsigned s) {

    solenoidOn(s);
    __delay_cycles(10000);          // 50ms initial pull



    for( unsigned i=0; i<10000 ; i++ ) {      // half power for 1000 * 1ms = 1000ms

        solenoidOn(s);
        __delay_cycles(20);            // on .5ms
        solenoidOff(s);
        __delay_cycles(80);            // off .5ms

    }



    solenoidOff(s);

    __delay_cycles(1000000);


}


// Handle interrupt for any switch (buttons and locking trigger)

#pragma vector=SWITCH_CHANGE_VECTOR
__interrupt void button_isr(void) {

    // Capture which flags are set (remember that this one register has the bit for all switches as confirmed above)
    // We capture a snapshot here so we do not miss any updates, but also so we can clear any changes that happen
    // from switch bounce below.

    // Show reset count
    //*hours_lcdmemw = secs_lcd_words[reset_counter_value];

    unsigned capture_interrupt_flags = SWITCH_CHANGE_PIFG;

    // If this pin interrupted, and it is armed, then register a press

    if ( TBI( capture_interrupt_flags , SWITCH_CHANGE_B ) && TBI( switch_armed_flags , SWITCH_CHANGE_B )  ) {

        // Button armed and pressed

        s++;
        if (s==100) {
            s=0;
        }

        *secs_lcdmemw = secs_lcd_words[s];

        // Make digitplace 0 not blink
        lcd_write_blank_to_lcdbm( 0 );

        // Make digitplace 5 blink
        lcd_write_glyph_to_lcdbm( 5 , glyph_8 );

   //     fire_solenoid(1);


        testSolenoid(2);

    }


    // Update minutes on every call so we can see if there are spurious ints happening

    m++;
    if (m==100) {
        m=0;
    }

    *mins_lcdmemw = mins_lcd_words[m];


    if ( TBI( capture_interrupt_flags , SWITCH_MOVE_B ) && TBI( switch_armed_flags , SWITCH_MOVE_B )  ) {

        h++;
        if (h==100) {
            h=0;
        }

        *hours_lcdmemw = hours_lcd_words[h];

        // Make digitplace 5 not blink
        lcd_write_blank_to_lcdbm( 5 );

        // Make digitplace 0 blink
        lcd_write_glyph_to_lcdbm( 0 , glyph_8 );

        // testSolenoid(1);



/*
        solenoidOn(1);
        __delay_cycles(1000);
        solenoidOff(1);
*/

        solenoidOn(1);
        __delay_cycles(50000);
        solenoidOff(1);


/*

        for( unsigned i=0; i<100 ; i++ ) {

            solenoidOn(1);
            __delay_cycles(1000);
            solenoidOff(1);

            solenoidOn(2);
            __delay_cycles(9000);
            solenoidOff(2);

        }

        */


/*


        for( unsigned i=0; i<1000 ; i++ ) {

            solenoidOn(1);
            __delay_cycles(100);
            solenoidOff(1);

            solenoidOn(2);
            __delay_cycles(100);
            solenoidOff(2);

            __delay_cycles(800);

        }

*/

//        solenoidOn(1);
//        solenoidOn(2);
        __delay_cycles(50000);
 //       solenoidOff(1);
 //       solenoidOff(2);

/*
        fire_solenoid(2);
        fire_solenoid(1);
        fire_solenoid(2);
        fire_solenoid(1);
*/
        //fire_solenoids();



    }



    // Ok this part is tricky. We need to be able to debounce the switches but we can only get interrupts
    // in one direction at a time. So our strategy is to wait for 50ms after any change to give the bouncing time
    // to settle down, and then we look at the current state of the pins and set the interrupt direction so we
    // will interrupt on the next transition in either direction.

    // Delay 50ms for debouncing (We are running at 1MHz)
    __delay_cycles( 50000 );


    // Cupture current state of the switches
    unsigned current_switch_state = SWITCH_CHANGE_PIN;

    if ( TBI( capture_interrupt_flags , SWITCH_CHANGE_B) ) {

        // Something changed this interrupt

        if (!TBI( current_switch_state , SWITCH_CHANGE_B ) ) {

            // The button is currently down after the debounce period

            CBI( SWITCH_CHANGE_PIES , SWITCH_CHANGE_B );        // Now interrupt on low-to-high so we will wake when button is released.

            CBI( switch_armed_flags , SWITCH_CHANGE_B );        // And disarm.

        } else {

            // If the button is up after the debbounce period, then
            // it shoudl be armed and we want to inetrrupt next time it is pressed

            SBI( SWITCH_CHANGE_PIES , SWITCH_CHANGE_B );        // Now interrupt on low-to-high so we will wake when button is released.

            SBI( switch_armed_flags , SWITCH_CHANGE_B );        // And disarm.
        }

        // Ack the interrupt

        CBI( SWITCH_CHANGE_PIFG , SWITCH_CHANGE_B );

    }

    if ( TBI( capture_interrupt_flags , SWITCH_MOVE_B) ) {

        // Something changed this interrupt

        if (!TBI( current_switch_state , SWITCH_MOVE_B ) ) {

            // The button is currently down after the debounce period

            CBI( SWITCH_CHANGE_PIES , SWITCH_MOVE_B );        // Now interrupt on low-to-high so we will wake when button is released.

            CBI( switch_armed_flags , SWITCH_MOVE_B );        // And disarm.

        } else {

            // If the button is up after the debbounce period, then
            // it shoudl be armed and we want to inetrrupt next time it is pressed

            SBI( SWITCH_CHANGE_PIES , SWITCH_MOVE_B );        // Now interrupt on low-to-high so we will wake when button is released.

            SBI( switch_armed_flags , SWITCH_MOVE_B );        // And disarm.
        }

        // Ack the interrupt

        CBI( SWITCH_CHANGE_PIFG , SWITCH_MOVE_B );  // Implemented as "AND.B   #0x007f,&Port_A_PAIFG"

    }



}



void disable_buttons() {

    // Disable pin change interrupt on the pins
    // Do this first so we dont get any spurious ints when we drive them low.

    CBI( SWITCH_CHANGE_PIE , SWITCH_CHANGE_B );
    CBI( SWITCH_MOVE_PIE , SWITCH_MOVE_B );


    // Disable pull-ups
    // NOte it is import to do this before setting PDIR or else if the button happens to be
    // pressed then it could short a hi drive to ground

    CBI( SWITCH_CHANGE_POUT , SWITCH_CHANGE_B );
    CBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );

    // Drive pins low to avoid floating.

    SBI( SWITCH_MOVE_PDIR , TRIGGER_B );      // drive
    SBI( SWITCH_CHANGE_PDIR , TRIGGER_B );      // drive

}


void locked_mode() {

    // First we need to enable the interrupts so we wake up on each rising edge of the RV3032 clkout.
    // The RV3032 is always set to 1Hz so this will wake us once per second.

    // Now we enable the interrupt on the RTC CLKOUT pin. For now on we must remember to
    // disable it again if we are going to end up in sleepforever mode.

    // Clear any pending interrupts from the RV3032 clkout pin and then enable interrupts for the next falling edge
    // We we should not get a real one for 500ms so we have time to do our stuff

    CBI( RV3032_CLKOUT_PIFG     , RV3032_CLKOUT_B    );
    SBI( RV3032_CLKOUT_PIE      , RV3032_CLKOUT_B    );


    // Wait for interrupt to fire at next clkout low-to-high change to drive us into the state machine (in either "pin loading" or "time since lanuch" mode)
    // Could also enable the trigger pin change ISR if we are in RTL mode.
    // Note if we use LPM3_bits then we burn 18uA versus <2uA if we use LPM4_bits.
    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger


}


enum mode_t {
    SETTING,           // Currently setting the timeout
    LOCKED,            // Locked and counting down until we unlock
};

/*
 * The PERSISTENT pragma may be used only with statically-initialized variables. It prevents such variables
 * from being initialized during a reset. Persistent variables disable startup initialization; they are given an initial
 * value when the code is loaded, but are never again initialized.
 */

//#pragma #pragma PERSISTENT
mode_t mode;


void regulatorTest() {

    // SHOW SOMETHING


    *secs_lcdmemw = secs_lcd_words[56];
    *mins_lcdmemw = mins_lcd_words[34];
    *hours_lcdmemw = hours_lcd_words[12];


    //lcd_cls();

    // SLEEP
    __bis_SR_register(LPM4_bits );                // Enter LPM4, never wake up
    __no_operation();                                   // For debugger


    // We should never get here. Indicate error.
    *secs_lcdmemw = secs_lcd_words[44];
    *mins_lcdmemw = mins_lcd_words[55];
    *hours_lcdmemw = hours_lcd_words[66];

    while (1);


}

// Proof of life test just oscillates the easy-to-reach MOVE switch pin between 0 and Vcc at 0.5Hz
// It is the upper right pin of the button. Don't push the button while running this test or you will
// short out the MCU.

void test_twiddle_move_switch_pin() {
    while (1) {

        __delay_cycles(1000000);

        SBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );

        __delay_cycles(1000000);

        CBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );


    }

}

// Just count up the numbers 0-99 on all 3 pairs of digits

void lcd_test() {


    while (1) {

        for( byte i=0; i<100;i++) {

            *secs_lcdmemw = secs_lcd_words[i];
            *mins_lcdmemw = mins_lcd_words[i];
            *hours_lcdmemw = hours_lcd_words[i];

            __delay_cycles(500000);    // 0.5 sec +/-10% (we are running at 1Mhz)


        }

    }
}

int main( void )
{

    WDTCTL = WDTPW | WDTHOLD | WDTSSEL__VLO;   // Give WD password, Stop watchdog timer, set WD source to VLO
                                               // The thinking is that maybe the watchdog will request the SMCLK even though it is stopped (this is implied by the datasheet flowchart)
                                               // Since we have to have VLO on for LCD anyway, mind as well point the WDT to it.
                                               // TODO: Test to see if it matters, although no reason to change it.

    // Disable the Voltage Supervisor (SVS=OFF) to save power since we don't care if the MSP430 goes low voltage
    // This code from LPM_4_5_2.c from TI resources
    PMMCTL0_H = PMMPW_H;                // Open PMM Registers for write
    PMMCTL0_L &= ~(SVSHE);              // Disable high-side SVS

    // Count the number of boot-ups since programming
    SYSCFG0 &= ~PFWP;                   // Program FRAM write enable
//    reset_counter_value++;
    SYSCFG0 |= PFWP;                    // Program FRAM write protected (not writable)


    // Init GPIO first to be safe.
    initGPIO();
    // Init LCD next so we can talk
    initLCD();

    // Power up display with a nice dash pattern
    lcd_show_dashes();

    //#warning stop here for now
    //while (1);

    // Initialize the RV3032 with proper clkout & backup settings.
    rv3032_init();

    //regulatorTest();


    // Simple Solenoid tester

    // Set buttons as inputs
    CBI( SWITCH_CHANGE_PDIR , SWITCH_CHANGE_B);
    CBI( SWITCH_MOVE_PDIR   , SWITCH_MOVE_B);

    // Pull-up pins
    // Note that we set the pull mode to UP in initGPIO
    // and setting out to 1 when PDIR is clear will enable the pull-up

    SBI( SWITCH_CHANGE_POUT , SWITCH_CHANGE_B );
    SBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );

    while (1) {

        if (!TBI( SWITCH_CHANGE_PIN , SWITCH_CHANGE_B)) {

            // Button Down

            h=1; m=0; s=30;

            rv3032_zero();

            enable_rv3032_clkout_interrupt();

            __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
            __no_operation();                                   // For debugger


        }


    }

    while (1) {

        if (TBI( SWITCH_CHANGE_PIN , SWITCH_CHANGE_B)) {

            // Button Up
            *secs_lcdmemw = secs_lcd_words[0];

        }
        else {
            *secs_lcdmemw = secs_lcd_words[1];
            testSolenoid(1);
        }


        if (TBI( SWITCH_MOVE_PIN , SWITCH_MOVE_B)) {
            // Button Up
            *hours_lcdmemw = hours_lcd_words[0];

        }
        else {
            *hours_lcdmemw = hours_lcd_words[1];
            testSolenoid(2);

        }


    }

    enable_button_interrupts();
    enable_rv3032_clkout_interrupt();

    lcd_write_glyph_to_lcdmem(0, glyph_J);


    *secs_lcdmemw = secs_lcd_words[55];
    *mins_lcdmemw = mins_lcd_words[44];


    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger


    //lcd_test();
    //regulatorTest();


    #warning testing only
    locked_mode();

    // Start normal operation!

    while (1) {

        switch (mode) {

        case mode_t::SETTING:
            setting_mode();
            break;

        case mode_t::LOCKED:
            locked_mode();
            break;

        }

    }
    //LCDMEM[18] = 0xff;      // For proof of life testing. Turn on battery icons.

    byte s=0,m=0,h=0;

    while (1) {

        *secs_lcdmemw = secs_lcd_words[s];
        *mins_lcdmemw = mins_lcd_words[m];
        *hours_lcdmemw = hours_lcd_words[h];

        unsigned start_val = TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B );
        while (TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B )==start_val); // wait for any transition

        lcd_segment_set_to_lcdmem(lcd_segment_dot1);

        while (TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B )!=start_val); // wait to go back

        lcd_segment_clear_to_lcdmem(lcd_segment_dot1);

        s++;
        if (s==100) {
            s=0;
            m++;
            if (m==100) {
                m=0;
                h++;
                if (h==100) {
                    h=0;
                }
            }
        }

    }



    // If we get here then we know the RTC is good and that it has good clock data (ie it has been continuously powered since it was commissioned at the factory)

    // Wait for any clkout transition so we know we have at least 500ms to read out time and activate interrupt before time changes so we dont miss any seconds.
    // This should always take <500ms
    // This also proves the RV3032 is running and we are connected on clkout
    unsigned start_val = TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B );
    while (TBI( RV3032_CLKOUT_PIN, RV3032_CLKOUT_B )==start_val); // wait for any transition

    // Now we enable the interrupt on the RTC CLKOUT pin. For now on we must remember to
    // disable it again if we are going to end up in sleepforever mode.

    // Clear any pending interrupts from the RV3032 clkout pin and then enable interrupts for the next falling edge
    // We we should not get a real one for 500ms so we have time to do our stuff

    CBI( RV3032_CLKOUT_PIFG     , RV3032_CLKOUT_B    );
    SBI( RV3032_CLKOUT_PIE      , RV3032_CLKOUT_B    );

    // Activate the RAM-based ISR vector table (rather than the default FRAM based one).
    // We use the RAM-based one so that we do not have to unlock FRAM every time we want to
    // update an entry. It was also hoped that the RAM-based one would be more power efficient
    // but this does not seem to matter in practice.

    ACTIVATE_RAM_ISRS();

    // Wait for interrupt to fire at next clkout low-to-high change to drive us into the state machine (in either "pin loading" or "time since lanuch" mode)
    // Could also enable the trigger pin change ISR if we are in RTL mode.
    // Note if we use LPM3_bits then we burn 18uA versus <2uA if we use LPM4_bits.
    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger

    // We should never ever get here

    // Disable all interrupts since any interrupt will wake us from LPMx.5 and execute a reset, even if interrupts are disabled.
    // Since we never expect to get here, be safe and disable everything.

    CBI( RV3032_CLKOUT_PIE , RV3032_CLKOUT_B );     // Disable interrupt.
    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );    // Clear any pending interrupt.

    CBI( TRIGGER_PIE , TRIGGER_B );                 // Disable interrupt
    CBI( TRIGGER_PIFG , TRIGGER_B );                // Clear any pending interrupt.

    error_mode( ERROR_MAIN_RETURN );                    // This would be very weird if we ever saw it.

}




