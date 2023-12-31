#include <msp430.h>
#include "util.h"
#include "pins.h"
#include "i2c_master.h"

#include "error_codes.h"

#include "ram_isrs.h"

#include "tsl_asm.h"


#include "acid_fram_record.hpp"

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

    // Make Vcc pin to RV3032 ground. NOte that the RTC will likely keep running for a while off the backup
    // capacitor, but this should put it into backup mode where CLKOUT is disabled.
    CBI( RV3032_VCC_POUT , RV3032_VCC_B);

    // Now the CLKOUT pin is floating, so we will pull it
    SBI( RV3032_CLKOUT_PREN , RV3032_CLKOUT_B );    // Enable pull resistor (does not matter which way, just keep pin from floating to save power)

    CBI( RV3032_CLKOUT_PIE , RV3032_CLKOUT_B );     // Disable interrupt.
    CBI( RV3032_CLKOUT_PIFG , RV3032_CLKOUT_B );    // Clear any pending interrupt.

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

    // TODO: use the full word-wide registers
    // Initialize all GPIO pins for low power OUTPUT + LOW
    P1DIR = 0xFF;P2DIR = 0xFF;P3DIR = 0xFF;P4DIR = 0xFF;
    P5DIR = 0xFF;P6DIR = 0xFF;P7DIR = 0xFF;P8DIR = 0xFF;
    // Configure all GPIO to Output Low
    P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
    P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

    // --- Solenoid MOSFETs off

    // Set Solenoid transistor pins to output (will be driven low by default, so off)
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
    SBI( DEBUGB_PREN , DEBUGB_B );          // Currently checked at power up, if low then we go into testonly mode.

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
    SBI( SWITCH_MOVE_PREN , SWITCH_MOVE_B );      // Set pull mode to UP (no effect when pin is in OUTPUT mode)
    SBI( SWITCH_MOVE_PIES , SWITCH_MOVE_B );      // Interrupt on high-to-low transition (pin pulled up, button connects to ground)

    CBI( SWITCH_CHANGE_POUT , SWITCH_CHANGE_B );      // low
    SBI( SWITCH_CHANGE_PDIR , SWITCH_CHANGE_B );      // drive
    SBI( SWITCH_CHANGE_PREN , SWITCH_CHANGE_B );      // Set pull mode to UP (no effect when pin is in OUTPUT mode)
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
        } else if (lpin <32 ) {         // Note that we ignore 0 here because we use 0 as a placeholder. This is ugly.
            LCDPCTL1 |= 1 << (lpin-16);
        } else if (lpin <48 ) {         // Note that we ignore 0 here because we use 0 as a placeholder. This is ugly.
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


    LCDBLKCTL =
            LCDBLKPRE__8 |            // Blinking frequency prescaller. This controls how fast the blink is.
            LCDBLKMOD_1                 // Individual control of blinking segments from LCDBM
    ;


/*

    LCDBLKCTL =
            LCDBLKPRE__4 |            // Blinking frequency prescaller. This controls how fast the blink is.
            LCDBLKMOD_2               // Blink all segments
    ;

*/

    LCDCTL0 |= LCDON;                                           // Turn on LCD

}

// The of time registers we read from the RTC in a single block using one i2c transaction.

struct __attribute__((__packed__)) rv3032_time_block_t {
    byte sec_bcd;
    byte min_bcd;
    byte hour_bcd;
    byte weekday_bcd;
    byte date_bcd;
    byte month_bcd;
    byte year_bcd;
};

static_assert( sizeof( rv3032_time_block_t ) == 7 , "The size of this structure must match the size of the block actually read from the RTC");


// Read the time regs in one transaction

void readRV3032time( rv3032_time_block_t *b ) {

    i2c_init();

    i2c_read( RV_3032_I2C_ADDR , RV3032_SECS_REG  , b , sizeof( rv3032_time_block_t ) );

    i2c_shutdown();

}


// Write the time regs in one transacton. Clears the hundreths to zero.

void writeRV3032time( const rv3032_time_block_t *b ) {

    i2c_init();

    i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , b , sizeof( rv3032_time_block_t ) );

    i2c_shutdown();

}


rv3032_time_block_t rv_3032_time_block_init = {
  0,        // Sec
  0,        // Min
  0,        // Hour
  1,        // Weekday
  1,        // Date
  1,        // Month
  0         // Year
};


// This table of days per month came from the RX8900 datasheet page 9

static uint8_t daysInMonth( uint8_t m , uint8_t y) {

    switch ( m ) {

        case  4:
        case  6:
        case  9:
        case 11:
                    return 30 ;

        case  2:

                    if ( y % 4 == 0 ) {         // "Leap years are correctly handled from 2000 to 2099."
                                                // Empirical testing also shows that 00 is a leap year to the RV3032.
                                                // https://electronics.stackexchange.com/questions/385952/does-the-epson-rx8900-real-time-clock-count-the-year-00-as-a-leap-year

                        return 29 ;             // "February in leap year 01, 02, 03 ... 28, 29, 01

                    } else {

                        return 28;              // February in normal year 01, 02, 03 ... 28, 01, 02
                    }
/*
        case  1:
        case  3:
        case  5:
        case  7:
        case  8:
        case 10:
        case 12:    // Interestingly, we will never hit 12. See why?

*/
        default:
                    return 31 ;

    }

    // unreachable

}

static const unsigned days_per_century = ( 100UL * 365 ) + 25;       // 25 leap years in every century (RV3032 never counts a leap year on century boundaries). Comes out to 18262, which is both even and fits in an unsigned.

// Convert the y/m/d values to a count of the number of days since 00/1/1
// Note this can be unsigned since it at most can be 365 days per year * 100 years = ~36,000 days per century

static unsigned date_to_days( uint8_t y , uint8_t m, uint8_t d ) {

    uint32_t dayCount=0;

    // Count the days in each year so far this century

    for( uint8_t y_scan = 0; y_scan < y ; y_scan++ ) {

        if ( y_scan % 4 == 0 ) {
            // leap year every 4 years on RX8900
            dayCount += 366UL;      // 366 days per year past in leap years

        } else {

            dayCount += 365UL;      // 365 days per year past in normal years
        }

    }


    // Now accumulate the days in months past so far this year

    for( uint8_t m_scan = 1; m_scan < m ; m_scan++ ) {      // Don't count current month

        dayCount += daysInMonth( m_scan , y );       // Year is needed to count leap day in feb in leap years.


    }

    // Now include the passed days so far this month

    dayCount += (uint32_t) d-1;     // 1st day of a month is 1, so if it is the 1st of the month then no days has elapsed this month yet.

    return dayCount;

}

// RV3032 uses BCD numbers :/

uint8_t bcd2c( uint8_t bcd ) {

    uint8_t tens = bcd/16;
    uint8_t ones = bcd - (tens*16);

    return (tens*10) + ones;
}

uint8_t c2bcd( uint8_t c ) {

    uint8_t tens = c/10;
    uint8_t ones = c - (tens*10);

    return (tens*16) + ones;

}

// These two values must be atomically updated so that we can reliably count centuries even in the face
// of a potential battery change that straddles the century roll over.

struct century_counter_t {
    unsigned postmidcentury_flag;       // Set to 1 if we have past the 50 year mark in current century.
    unsigned century_count;             // How many centuries have we been in TSL mode?
};

// Initial starting state for a century counter
century_counter_t century_counter_init = {
    .postmidcentury_flag = 0,
    .century_count = 0,
};


// Here is the persistent data that we store in "information memory" FRAM that survives power cycles
// and reprogramming.

struct __attribute__((__packed__)) persistent_data_t {

    // Archival use only. Never read by the unit.
    rv3032_time_block_t commisioned_time;       // Calendar time when this unit was commissioned. Set during programming, never read.
    rv3032_time_block_t launched_time;          // Time when this unit was launched relative to commissioned. Set when trigger pulled, never read.

    // State. We have 3 persistent states, (1) first startup fresh from programming, (2) ready to launch, (3) launched.
    // We make these volatile to ensure that the compiler actually writes changes to memory rather than trying to cache in a register
    volatile unsigned once_flag;                             // Set to 1 first time we boot up after programming
    volatile unsigned launch_flag;                           // Set to 1 when the trigger pin is pulled

    // Used to count centuries since the RTC does not.
    // Note this is complicated because there is no way to atomically write to the FRAM, and in fact any one write
    // could be corrupted if power fails just at the right moment. But since at most one write can fail, we can use
    // a two-step commit with a fall back.
    // We do NOT make this volatile since the member values themselves are volatile.

    acid_FRAM_record_t<century_counter_t> acid_century_counter;

};

// Tell compiler/linker to put this in "info memory" at 0x1800
// This area of memory never gets overwritten, not by power cycle and not by downloading a new binary image into program FRAM.
// We have to mark `volatile` so that the compiler will really go to the FRAM every time rather than optimizing out some accesses.
persistent_data_t __attribute__(( __section__(".infoA") )) persistent_data;

void unlock_persistant_data() {
    SYSCFG0 = PFWP;                     // Write protect only program FRAM. Interestingly it appears that the password is not needed here?
}

void lock_persistant_data() {
    SYSCFG0 = PFWP | DFWP;              // Write protect both program and data FRAM.
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


// Returns  0=above time variables have been set to those held in the RTC
//          1=RTC has lost power so valid time not available.
//          2=Bad data from RTC

uint8_t RV3032_read_state() {

    // Initialize our i2c pins as pull-up
    i2c_init();

    // First check if the RV3032 just powered up, or if it was already running. Start with 0xff so if the i2c read fails then we assume not running.
    uint8_t status_reg=0xff;
    i2c_read( RV_3032_I2C_ADDR , 0x0d , &status_reg , 1 );

    i2c_shutdown();

    if ( (status_reg ) == 0xff ) {               //

            // Signal to caller that we did not get an expected value from RTC.
            return 2;

    }


    if ( (status_reg & 0x03) != 0x00 ) {               //If power on reset or low voltage drop detected then we have not been running continuously

        // Signal to caller that we have had a low power event.
        return 1;

    }

    return 0;

}

// Time from RTC - we read the RTC registers into these (we do decoding from d/m/y to elapsed days)
// During time-since-launch mode, this is how long we have been counting modulo one century.
// Scope-wise these should not be in global variables since we read them from the RTC and then
// use them to start the TSL mode display. But we need a way to get the into the TSL ISR which is in
// assembly and with this compiler the best way to do that pass is though shared global variable addresses.

uint8_t  rtc_secs=0;
uint8_t  rtc_mins=0;
uint8_t  rtc_hours=0;
uint16_t rtc_days=0;       // only needs to be able to hold up to 1 century of days since RTC rolls over after 100 years.

// This counts the centuries, which we have to do separately since the RTC will not do it for us

century_counter_t century_counter;      // Cached copy of the ACID one in `persistantData` so we do not need to read that one out every time while running
unsigned days_into_current_century;     // So we know when to update the post mid century flag and then to tick the century

// Read time from RTC into our global time variables

void RV_3032_read_time() {

    // Initialize our i2c pins as pull-up
    i2c_init();

    // Read time

    rv3032_time_block_t t;
    readRV3032time(&t);

    rtc_secs  = bcd2c(t.sec_bcd);
    rtc_mins  = bcd2c(t.min_bcd);
    rtc_hours = bcd2c(t.hour_bcd);

    uint8_t y;
    uint8_t m;
    uint8_t d;

    // The RTC registers store their values as BCD because an RTC chip back in the 1970's did. :/.
    d = bcd2c(t.date_bcd);
    m = bcd2c(t.month_bcd);
    y = bcd2c(t.year_bcd);

    // Convert dd/mm/yy into the count of days into the current century.
    rtc_days = date_to_days( y, m, d);

    i2c_shutdown();

}

/*

// Test to see if the RV3032 considers (21)00 a leap year
// RESULT: Confirms that year "00" is a leap year with 29 days in February.

void testLeapYear() {

    // Initialize our i2c pins as pull-up
    i2c_init();

    // Feb 28, 2100 (or 2000)
    // Numbers in BCD
    uint8_t b23 = 0x23;
    uint8_t b59 = 0x59;

    uint8_t b02 = 0x02;
    uint8_t b28 = 0x28;
    uint8_t b00 = 0x00;


    i2c_write( RV_3032_I2C_ADDR , RV3032_HUNDS_REG , &b00 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &b00 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_MINS_REG  , &b59 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_HOURS_REG , &b23 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_DAYS_REG  , &b28 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_MONS_REG  , &b02 , 1 );
    i2c_write( RV_3032_I2C_ADDR , RV3032_YEARS_REG , &b00 , 1 );

    i2c_shutdown();


    while (1) {


        __delay_cycles(1000000);

        i2c_init();

        uint8_t t;

        i2c_read( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &t , 1 );
        secs = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_MINS_REG  , &t , 1 );
        mins = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_HOURS_REG , &t , 1 );
        hours = bcd2c(t);

        uint8_t y;
        uint8_t m;
        uint8_t d;

        i2c_read( RV_3032_I2C_ADDR , RV3032_DAYS_REG  , &t , 1 );
        d = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_MONS_REG  , &t , 1 );
        m = bcd2c(t);

        i2c_read( RV_3032_I2C_ADDR , RV3032_YEARS_REG , &t , 1 );
        y = bcd2c(t);

        i2c_shutdown();


    }



}


*/

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

        // Initialize our i2c pins as pull-up
        i2c_init();

        // First get current time and save it to FRAM for archival purposes.
        // Also update the persistent storage to reflect that we launched now.
        unlock_persistant_data();
        i2c_read( RV_3032_I2C_ADDR , RV3032_SECS_REG  , (void *)  &persistent_data.launched_time , sizeof( rv3032_time_block_t ) );
        persistent_data.launch_flag=0x01;
        persistent_data.acid_century_counter.writeData(&century_counter_init);
        lock_persistant_data();

        // Then zero out the RTC to start counting over again, starting now. Note that writing any value to the seconds register resets the sub-second counters to the beginning of the second.
        // "Writing to the Seconds register creates an immediate positive edge on the LOW signal on CLKOUT pin."
        i2c_write( RV_3032_I2C_ADDR , RV3032_SECS_REG  , &rv_3032_time_block_init , sizeof( rv3032_time_block_t ) );

        i2c_shutdown();

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

    days_into_current_century++;

    if (days_into_current_century>=(days_per_century/2)) {

        // We are more than half way into the current century

        century_counter_t cc;
        persistent_data.acid_century_counter.readData(&cc);

        // Has it been 100 years since we last incremented the century counter?

        if (days_into_current_century >= days_per_century ) {

            // We just ticked into a new century! Party like its 2x00!

            // Atomically increment us to the next century
            cc.century_count++;
            cc.postmidcentury_flag=0;
            unlock_persistant_data();
            persistent_data.acid_century_counter.writeData(&cc);
            lock_persistant_data();

            // Get ready for next century click!
            days_into_current_century -= days_per_century;

        } else {

            // It is a new day, and we are past the middle of the current century
            // Check to see if we have already recorded that we are past the middle

            if (cc.postmidcentury_flag != 1) {

                // We have just crossed the middle of the century for the first time since
                // the last century click (which may have been more than 50 years ago if the battery was
                // pulled right at year 50)

                // Record that we have pasted the middle century mark so if we have (1) have a battery
                // pull between now and the end of the century, and (2) the battery is put back in after the
                // century has already clicked, then we will notice it when we repower up and
                // account for the missing century

                cc.postmidcentury_flag = 1;
                unlock_persistant_data();
                persistent_data.acid_century_counter.writeData(&cc);
                lock_persistant_data();

            }

        }
    }

    // Now that century book keeping is done, we can update the actual display to reflect the new day

    if (days_digits[0] < 9) {

        days_digits[0]++;

        lcd_show_digit_f(  6 , days_digits[0] );

        return;

    }

    days_digits[0]=0x00;

    if (days_digits[1] < 9) {

        lcd_show_digit_f( 6 , 0 );

        days_digits[1]++;

        lcd_show_digit_f(  7 , days_digits[1] );

        return;

    }

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


// Reference version of the ready to launch animation. This has been replaced with optimized ASM in tsl_asm.asm
void ready_to_launch_reference() {
    // We depend on the trigger ISR to move us from ready-to-launch to time-since-launch

    // Do the increment and normalize first so we know it will always be in range
    step++;
    step &=0x07;

    lcd_show_squiggle_frame(step );

    static_assert( SQUIGGLE_ANIMATION_FRAME_COUNT == 8 , "SQUIGGLE_ANIMATION_FRAME_COUNT should be exactly 8 so we can use a logical AND to keep it in bounds for efficiency");            // So we can use fast AND 0x07

}

// Note that we do not make this an "interrupt" function even though it is an ISR.
// We can do this because we know that there is nothing being interrupted since we just
// sleep between interrupts, so no need to save any registers. We do need to do our own
// RETI (return from interrupt) at the end since the function itself will only have a
// normal return.

// -- normal naked ISR
// 24us
// 2.04uA with zeros
// 161uA peak
// 29.44us wake time

// -- ramfunc, FRAM controller on
// 21us
// 1.49uA with dashes
// 2.17uA with zeros
// 26.62us wake time

// -- ramfunc, FRAM controller off
// 21us
// 2.04uA with zeros
// peak 119uA
// 26us wake time

// -- normal naked ISR + 500 cycles
// 500us
// 2.15uA with zeros (1.9uA @ 1.8mA range, 2.15uA @ auto)
// 240uA peak (3mA on auto)

// -- ramfunc, FRAM controller off + 500 cycles
// 499us
// 2.15uA with zeros (2.15uA auto)
// 4.9mA peak on auto. Zoomed in shows 245uA durring the 500us



// Reference version of time-since-launch updater
// This has been replaced with vastly more efficient ASM code in tsl_asm.asm
void time_since_launch_reference() {
    // 502us

    // Show current time

    rtc_secs++;

    if (rtc_secs == 60) {

        rtc_secs=0;

        rtc_mins++;

        if (rtc_mins==60) {

            rtc_mins = 0;

            rtc_hours++;

            if (rtc_hours==24) {

                rtc_hours = 0;

                if (rtc_days==999999) {

                    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

                        // The long now

                        lcd_show_long_now();
                        blinkforeverandever();

                    }



                } else {

                    if (testing_only_mode) {
                        lcd_show_testing_only_message();
                        blinkforeverandever();
                    }

                    tsl_next_day();

                }


            }

            lcd_show_digit_f( 4 , rtc_hours % 10  );
            lcd_show_digit_f( 5 , rtc_hours / 10  );

        }

        lcd_show_digit_f( 2 ,  rtc_mins % 10  );
        lcd_show_digit_f( 3 ,  rtc_mins / 10  );

    }

    // Show current seconds on LCD using fast lookup table. This is a 16 bit MCU and we were careful to put the 4 nibbles that control the segments of the two seconds digits all in the same memory word,
    // so we can set all the segments of the seconds digits with a single word write.

    *secs_lcdmemw = secs_lcd_words[rtc_secs];

    /*
        // Wow, this compiler is not good. Below we can remove a whole instruction with 3 cycles that is completely unnecessary.

        asm("        MOV.B     &secs+0,r15           ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        RLAM.W    #1,r15                ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        MOV.W     secs_lcd_words+0(r15),(LCDM0W_L+16) ; [] |../tsl-calibre-msp.cpp:1390|");
    */

}


// Note that solenoid numbers match the PCB markings and range 1-6,
// where 1 is at 1 oclock and the go counter clockwise from there

void fire_solenoid( unsigned s ) {

    switch (s) {
    case 1:     SBI( S1_POUT , S1_B ); break;
    case 2:     SBI( S2_POUT , S2_B ); break;
    case 3:     SBI( S3_POUT , S3_B ); break;
    case 4:     SBI( S4_POUT , S4_B ); break;
    case 5:     SBI( S5_POUT , S5_B ); break;
    case 6:     SBI( S6_POUT , S6_B ); break;
    }

    __delay_cycles( 50000 );

    switch (s) {
    case 1:     CBI( S1_POUT , S1_B ); break;
    case 2:     CBI( S2_POUT , S2_B ); break;
    case 3:     CBI( S3_POUT , S3_B ); break;
    case 4:     CBI( S4_POUT , S4_B ); break;
    case 5:     CBI( S5_POUT , S5_B ); break;
    case 6:     CBI( S6_POUT , S6_B ); break;
    }


}

// These two countdown vars keep track of how long until we unlock. We do not initialize them since they will get set when
// we transition from setting mode to locked mode. They are NOINIT so they will persist though resets and power cycles.
// These are updated every 3 seconds while we are counting down in locked mode so in case we reset or lose power,
// then we will come back up and restart where we left off. They are kept in FRAM which is persistent across power cycles,
// and writing to these to update them only takes a single instruction.

// We choose to track triple-seconds rather than seconds because (1) we have only 1/3 as many updates so less power,
// (2) matches to the three-screen display cycle, and (3) means we can keep a full day in a single 16 bit memory location.


#pragma NOINIT
unsigned int countdown_tripple_secs;

#pragma NOINIT
unsigned int countdown_days;

// The MSP430 guarantees that writes to a single FRAM address will be atomic, but once per day we will need to do a non-atomic
// update of both the days and the tripple_secs, so we make a backup of the values, set the backup flag, make our updates, and clear the backup flag.
// This ensures consistency in case we reset or lose power during the update, and we will at most miss 3 seconds.

#pragma NOINIT
unsigned int countdown_days_backup_in_progress_flag;

#pragma NOINIT
unsigned int countdown_tripple_secs_backup_value;

#pragma NOINIT
unsigned int countdown_days_backup_value;


// Incremented each time we boot just to keep track of battery changes.
#pragma NOINIT
unsigned int reset_counter_value=0;


void setting_mode() {

    enum units_t {
        YEARS,
        DAYS,
        HOURS
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

unsigned int h=0,m=0,s=0;


//#pragma vector=RV3032_CLKOUT_VECTOR
__interrupt void clkout_isr(void) {

    s++;

    if (s==60) {
        s=0;

        m++;
        if (m==60) {
            m=0;

            h++;

            if (h==24) {
                h=0;
            }

            *hours_lcdmemw = hours_lcd_words[h];

        }

        *mins_lcdmemw = mins_lcd_words[m];

    }

    *secs_lcdmemw = secs_lcd_words[s];

    RV3032_CLKOUT_PIV;

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



// Handle interrupt for any switch (buttons and locking trigger)

#pragma vector=SWITCH_CHANGE_VECTOR
__interrupt void button_isr(void) {

    // Capture which flags are set (remember that this one register has the bit for all switches as confirmed above)
    // We capture a snapshot here so we do not miss any updates, but also so we can clear any changes that happen
    // from switch bounce below.

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

        fire_solenoid(1);

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

        fire_solenoid(1);
        fire_solenoid(2);



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

        CBI( SWITCH_CHANGE_PIFG , SWITCH_MOVE_B );

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
// It is the upper right pin of the button.

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


    enable_button_interrupts();
    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger


    //lcd_test();
    regulatorTest();



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

    if ( persistent_data.launch_flag != 0x01 ) {

        // We have never launched!

        // First we need to make sure that the trigger pin is inserted because
        // we would not want to just launch because the pin was out when batteries were inserted.
        // This also lets us test both trigger positions during commissioning at the factory.

        CBI( TRIGGER_PDIR , TRIGGER_B );      // Input
        SBI( TRIGGER_PREN , TRIGGER_B );      // Enable pull resistor
        SBI( TRIGGER_POUT , TRIGGER_B );      // Pull up

        step =0;                              // Used to slide a dash indicator pointing to the pin location


        // Set us up to run the loading/ready-to-launch sequence on the next tick
        // Note that the trigger ISR will be activated when we get into ready-to-launch
#warning
        //SET_CLKOUT_VECTOR( &startup_isr);

        // We will go into "load pin" mode when next second ticks. This gives the pull-up a chance to take effect and also avoids any bounce aliasing right at power up.

    } else {

        // Read the time since launch from the RTC into our global time variables. We know the RTC has good time since we checked status above.
        RV_3032_read_time();

        days_into_current_century = rtc_days;   // Capture this, we will continue to use it while running

        // Alas, the RV3032 does not track centuries so we need to do some extra work here to account for them.
        // First read the most recently stored century record

        persistent_data.acid_century_counter.readData( &century_counter );

        // Next compute if we are in the first or second half of the current century (using the date we read from the RTC)
        // Remember that `days` here is the number of days since the current century started.
        unsigned current_post_midcentury_flag = (days_into_current_century >= days_per_century/2);

        // Now check if a century has rolled over while we were powered down
        // That is "it was the 2nd half of the current century the last time a day ticked by, but not it is the first half of the century"
        // (this would happen if a battery change happened right at a century change)

        if ( century_counter.postmidcentury_flag && !current_post_midcentury_flag ) {

            // We were in the 2nd half of the century before, and now we are back at the first half,
            // so the century must have rolled in between

            century_counter.century_count++;
            century_counter.postmidcentury_flag=0;

            // Save this result

            unlock_persistant_data();
            persistent_data.acid_century_counter.writeData(&century_counter);
            lock_persistant_data();

        }

        // Add the accumulated centuries to days into current century to calculate the total rtc_days since we launched.
        // Note we only need this value temporarily to scatter the value into the digits array which is where we update/display

        unsigned long days_since_launch = days_into_current_century + ( century_counter.century_count * days_per_century );

        // OK, now `days_since_launch` is the full number of days that have passed since we launched and days_into_current_century is the number of days into the current century

        // Break out the days into digits for more efficient updating while running.
        // We only do this ONCE per set of batteries so no need for efficiency here.

        days_digits[0]= (days_since_launch / 1      ) % 10 ;
        days_digits[1]= (days_since_launch / 10     ) % 10 ;
        days_digits[2]= (days_since_launch / 100    ) % 10 ;
        days_digits[3]= (days_since_launch / 1000   ) % 10 ;
        days_digits[4]= (days_since_launch / 10000  ) % 10 ;
        days_digits[5]= (days_since_launch / 100000 ) % 10 ;

        // Show the days since launch on the display

        lcd_show_digit_f(  6 , days_digits[0] );
        lcd_show_digit_f(  7 , days_digits[1] );
        lcd_show_digit_f(  8 , days_digits[2] );
        lcd_show_digit_f(  9 , days_digits[3] );
        lcd_show_digit_f( 10 , days_digits[4] );
        lcd_show_digit_f( 11 , days_digits[5] );

        // Note here that we show the time as read from the RTC, but on the next call to the
        // ISR that happens in 1 second, we will use these same values to show the *next* second
        // since the ISR seconds table is offset by 1. We do this for efficiency since the MSP430
        // only has post-increment, so it would take an extra cycle to increment before display in the ISR.

        lcd_show_digit_f( 4 , rtc_hours % 10  );
        lcd_show_digit_f( 5 , rtc_hours / 10  );
        lcd_show_digit_f( 2 , rtc_mins  % 10  );
        lcd_show_digit_f( 3 , rtc_mins  / 10  );
        lcd_show_digit_f( 0 , rtc_secs  % 10  );
        lcd_show_digit_f( 1 , rtc_secs  / 10  );

        // Now start ticking at next second tick interrupt
        // The TSL_MODE_BEGIN ISR will initialize the TSL mode counting registers from
        // the `hours`, `mins`, and `secs` globals. Note thta the ISR does not know about `days`
        // becuase it calls back to the C `tsl_next_day()` routine when days increment.
        // We do not set up the trigger ISR since it can never come. We will tick like this
        // forever (or at least until we loose power).
        //SET_CLKOUT_VECTOR( &TSL_MODE_BEGIN );
    }

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




