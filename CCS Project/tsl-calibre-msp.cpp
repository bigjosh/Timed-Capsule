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
    lcd_blinking_mode_all();
    sleepforeverandever();
}

// Assumes all interrupts have been individually disabled.
#pragma FUNC_NEVER_RETURNS
void error_mode( byte code ) {
    lcd_show_errorcode(code);
    blinkforeverandever();
}

void sleep_with_interrupts() {
    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger
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
    // Note that they are also pulled down by 100K ohm resistors so they do not float before we get here during reset
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

    CBI( SWITCH_TRIGGER_POUT , SWITCH_TRIGGER_B );      // low
    SBI( SWITCH_TRIGGER_PDIR , SWITCH_TRIGGER_B );      // drive
    SBI( SWITCH_TRIGGER_PREN , SWITCH_TRIGGER_B );      // Set pull mode to UP when POUT is high and PDIR is low (no effect when pin is in OUTPUT mode)
    SBI( SWITCH_TRIGGER_PIES , SWITCH_TRIGGER_B );      // Interrupt on high-to-low transition (pin pulled up, switch connects to ground)

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

/*

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

*/

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

// open the pair of solenoids connected to a single lock slide.
// g can be 0,1, or 2.

// TODO: Potentially reduce the max current load by only giving one solenoid full power just long enough for it to pull, then PWM it while pulling the other one.

void toggle_lock_group( unsigned g ) {

    struct solenoid_pair_t { unsigned a; unsigned b;};

    constexpr solenoid_pair_t solenoid_pairs[] = { {2,3} , {4,5} , {6,1} };

    solenoid_pair_t solenoid_pair = solenoid_pairs[g];

    unsigned solenoid_a =  solenoid_pair.a;
    unsigned solenoid_b =  solenoid_pair.b;

    solenoidOn(solenoid_a);
    solenoidOn(solenoid_b);

    __delay_cycles( 50000 );

    solenoidOff(solenoid_a);
    solenoidOff(solenoid_b);


}

// Unlock the lid by pulling each if the 3 solenoid pairs in sequence.
// We have to pull in pairs because both solenoid pins have to be pulled for the slide to be released.
// We rotate with pair we start with on each call. This is in case one of the pairs needs the slightly higher current the batteries can give after they have rested for a minute.

static unsigned next_starting_pair = 0;

void unlock() {

    // Open each pair of solenoids in sequence, hopefully pulling them just long enough for the lock slide to retract.

    unsigned pull_pair = next_starting_pair;

    do {

        toggle_lock_group(pull_pair);
        __delay_cycles( 100000 );           // Delay to slightly let the batteries recover

        pull_pair++;
        pull_pair %=3;

    } while ( pull_pair != next_starting_pair);

    next_starting_pair++;
    next_starting_pair%=3;
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
volatile unsigned int countdown_d,countdown_h,countdown_m,countdown_s;

// These are for safekeeping in case we reset.
// Note that these are redundant, but it is faster to keep them in this separate format

countdown_time_t countdown_time;


// Start calling the clkout vector above on each tick form the RTC, starting at next tick
// note that you might want to call rc3032_zero() first to ensure a full second elapses before first tick.

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

void stop_countdown_mode() {
    disable_rv3032_clkout_interrupt();
}


void start_setting_mode();          // Forward reference, defined below with the setting mode stuffs


enum class countdown_display_page_t {
    HHMMSS,
    DAYS,
    BLANK
};

volatile countdown_display_page_t countdown_display_page;

// Timer interrupt
// Should fire once per second

#pragma vector=RV3032_CLKOUT_VECTOR
__interrupt void clkout_isr(void) {

    // CLear the pending interrupt.
    // Do this first since there are a couple ways we can exit this ISR
    RV3032_CLKOUT_PIV;          // Implemented as "MOV.W   &Port_1_2_P2IV,R15"

    /*
    if (countdown_display_page==countdown_display_page_t::BLANK) {
        // We have to to the cls here at the top becuase it takes time a
        lcd_cls_LCDMEM();               // Clear the LCDMEM which is currently showing HHMMSS. This will happen aysnchonously, but that is ok becuase it does not matter since a transision to blank will always look OK.
    }
    */

    if (countdown_s==0) {
        if (countdown_m==0) {
            if (countdown_h==0) {
                if (countdown_d==0) {

                    /// Time to unlock!!!!

                    // We are done with this mode
                    stop_countdown_mode();

                    // Show user we are opening
                    lcd_on();                       // We might have been showing a blank page?
                    lcd_show_LCDMEM_bank();
                    lcd_show_open_message();

                    // First disable the clock interrupt since will do not want it anymore

                    disable_rv3032_clkout_interrupt();

                    unlock();

                    // Now go back to setting mode so user can start a new countdown!
                    start_setting_mode();

                    return;

                }
                countdown_h=24;
                countdown_d--;

                lcd_show_days_lcdbmem(countdown_d);

            }
            countdown_m=60;
            countdown_h--;

            //*hours_lcdmemw = hours_lcd_words[countdown_h];

        }
        countdown_s=60;           // Yea I know this looks wrong, but we decremented at the top already.
        countdown_m--;

        //*mins_lcdmemw = mins_lcd_words[countdown_m];

        // TODO: Update the recovery data here.

    }

    countdown_s--;
    //*secs_lcdmemw = secs_lcd_words[countdown_s];

    /*
        // Wow, this compiler is not good. Below we can remove a whole instruction with 3 cycles that is completely unnecessary.

        asm("        MOV.B     &s+0,r15           ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        RLAM.W    #1,r15                ; [] |../tsl-calibre-msp.cpp:1390| ");
        asm("        MOV.W     secs_lcd_words+0(r15),(LCDM0W_L+16) ; [] |../tsl-calibre-msp.cpp:1390|");
    */


    // Now update the display page

    // Order of pages is days->HHMMSS->blank
    // This kind looks right because you read it "5 days... 4 hours and 22 minutes and 16 second... blank"

    // HHMMSS is first in the if chain so we can handle the special case when days == 0
    // When days is zero, we continuously show the HHMMSS page for increased excitement.
    if (countdown_display_page==countdown_display_page_t::HHMMSS || countdown_d==0 ) {
        // The HHMMSS pattern is displayed from the primary LCD buffer
        // This page will either already have an old HHMMSS or it will be blank.
        *hours_lcdmemw = hours_lcd_words[countdown_h];
        *mins_lcdmemw = mins_lcd_words[countdown_m];
        *secs_lcdmemw = secs_lcd_words[countdown_s];
        lcd_show_LCDMEM_bank();         // Show the newly painted HHMMSS

        countdown_display_page = countdown_display_page_t::BLANK;

    } else if (countdown_display_page==countdown_display_page_t::DAYS) {

        lcd_show_LCDBMEM_bank();            // The day page is already painted on the LCDBMEM bank so we only have to show it.

        countdown_display_page=countdown_display_page_t::HHMMSS;


    } else {  // if countdown_display_page==countdown_display_page_t::BLANK

        // TODO: POWER OPTIMIZE THIS!!!


        // TODO: Figure out why this does not work. Somehow blocks the HHMMSS page that comes after the days page. :/
        // lcd_cls_LCDMEM_nowait();               // Clear the LCDMEM which is currently showing HHMMSS. This will happen aysnchonously, but that is ok becuase it does not matter since a transision to blank will always look OK.


        lcd_show_f(LCDMEM, 0, glyph_SPACE  );
        lcd_show_f(LCDMEM, 1, glyph_SPACE  );
        lcd_show_f(LCDMEM, 2, glyph_SPACE  );
        lcd_show_f(LCDMEM, 3, glyph_SPACE  );
        lcd_show_f(LCDMEM, 4, glyph_SPACE  );
        lcd_show_f(LCDMEM, 5, glyph_SPACE  );
        lcd_show_f(LCDMEM, 6, glyph_SPACE  );


        lcd_show_LCDMEM_bank();         // Show the newly painted HHMMSS

        countdown_display_page=countdown_display_page_t::DAYS;

    }

}

enum class setting_units_t {
    YEARS,
    DAYS,
    HOURS,
    SECS,
};


// These have to be volatile because they are updated inside the ISR

volatile setting_units_t setting_unit;

volatile unsigned setting_digits[DIGITPLACE_COUNT-1];        // Current values ( minus one because the units use up LCD place 0).

volatile unsigned setting_cursor_pos;        // Which digit position is the cursor currently on? 0=rightmost place.

// Show the current setting values on the LCD
void update_setting_display() {


    // Note that this does do leading zeros, which I think we want?

    // Show digits, starting at left going to right
    for( unsigned pos = 1; pos < DIGITPLACE_COUNT ; pos++ ) {

        unsigned v = setting_digits[pos-1]; // -1 becuase LCD place 0 is used for units

        lcd_show_digit_f( LCDMEM , pos, v );

        // If the cursor is on this place...
        if (pos==setting_cursor_pos) {
            // ... make the segments blink
            lcd_show_digit_f( LCDBMEM , pos, v);
        } else {
            // Otherwise clear all segments so no blink
            lcd_show_f( LCDBMEM , pos , glyph_SPACE);
        }

    }

    // Now show the units

    glyph_segment_t units_glyph;

    switch ( setting_unit ) {

        case setting_units_t::SECS:
            units_glyph = glyph_c;
            break;

        case setting_units_t::HOURS:
            units_glyph = glyph_h;
            break;

        case setting_units_t::DAYS:
            units_glyph = glyph_d;
            break;

        case setting_units_t::YEARS:
            units_glyph = glyph_y;
            break;


    }

    lcd_show_f( LCDMEM , 0 , units_glyph);

    // Is cursor currently on the units pos?
    if (setting_cursor_pos==0) {
        lcd_show_f( LCDBMEM , 0 , units_glyph);
    } else {
        lcd_show_f( LCDBMEM , 0 , glyph_SPACE);
    }

}

// When the switch bit here is 1, then button is up and we will interrupt high-to-low,
// and we will register a press.


// Cases:
// Switch is armed and goes down and is still down after debounce - interrupt to up
// Switch is armed and goes down and is not still down afer debounce - do nothing, we will get another press when it goes down again.

// Switch is not armed and goes down - ignore
//

unsigned switch_armed_flags;

/*

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

*/


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

// Forward references.
void stop_setting_mode();
void start_countdown_mode( unsigned days, unsigned hours, unsigned mins, unsigned secs);


// Handle interrupt for any switch (buttons and locking trigger)

#pragma vector=SWITCH_CHANGE_VECTOR
__interrupt void button_isr(void) {

    // Capture which flags are set (remember that this one register has the bit for all switches as confirmed above)
    // We capture a snapshot here so we do not miss any updates, but also so we can clear any changes that happen
    // from switch bounce below.

    static_assert(  ( &SWITCH_CHANGE_PIFG == &SWITCH_MOVE_PIFG ) && ( &SWITCH_CHANGE_PIFG == &SWITCH_TRIGGER_PIFG ) , "This code assumes that The two buttons and the trigger switch are all connected to the same ISR." );

    unsigned capture_interrupt_flags = SWITCH_CHANGE_PIFG;

    // If this pin interrupted, and it is armed, then register a press

    if ( TBI( capture_interrupt_flags , SWITCH_CHANGE_B ) && TBI( switch_armed_flags , SWITCH_CHANGE_B )  ) {

        if ( setting_cursor_pos == 0 ) {
            // change units

            if (setting_unit==setting_units_t::YEARS) {
                setting_unit = setting_units_t::SECS;
            } else if (setting_unit==setting_units_t::SECS) {           // TODO: Remove seconds for production version
                setting_unit = setting_units_t::HOURS;
            } else if (setting_unit==setting_units_t::HOURS) {
                setting_unit = setting_units_t::DAYS;
            } else { // if (setting_unit==setting_units_t::DAYS) {

                setting_unit = setting_units_t::YEARS;
                // Switch to years units is special since we have to make sure they never can
                // set more than 100 years.

                setting_digits[4]=0;
                setting_digits[3]=0;

                if (setting_digits[1] != 0 || setting_digits[0] !=0 ) {
                    setting_digits[2]=0;
                }

                if (setting_digits[2]>1) {
                    setting_digits[2]=1;
                }

                // We also constrain the cursor from even going to the unsettable digits
                if (setting_cursor_pos>3) {
                    setting_cursor_pos=3;
                }
            }

        } else {

            // Changing a number digit, not the units place
            // Remember we use (pos-1) because pos 0 on the LCD is the units, so digits start at pos 1
            if (setting_digits[setting_cursor_pos-1]==9) {
                setting_digits[setting_cursor_pos-1] = 0;
            } else {
                setting_digits[setting_cursor_pos-1]++;
            }

            if (setting_unit==setting_units_t::YEARS) {

                // We also constrain choices while in years units to limit from going over 100

                if (setting_cursor_pos==3) {

                    // They changed the hundreds digit of the year

                    if (setting_digits[2] == 1) {

                        // They updated it from 0 to 1, so tens and ones have to be 0 to keep less than 100.
                        setting_digits[1]=0;
                        setting_digits[0]=0;
                    }


                    if (setting_digits[2] > 1) {
                        // They updated hundreds digit from 1 to 2, so we have to bring it back down to 0
                        setting_digits[2] = 0;
                    }

                } else if (setting_digits[1] != 0 || setting_digits[0] !=0 ) {

                    // The years tens or ones digits are non-zero, so hundreds much be zero to keep total less than 100
                    setting_digits[2]=0;

                }

            }
        }

    }

    if ( TBI( capture_interrupt_flags , SWITCH_MOVE_B ) && TBI( switch_armed_flags , SWITCH_MOVE_B )  ) {

        if ( setting_cursor_pos == 0 ) {

            // If in year mode then do not give access to the leftmost 2 digits (max 100 years)

            if ( setting_unit == setting_units_t::YEARS) {
                setting_cursor_pos = 3;
            } else {
                setting_cursor_pos = 5;
            }


        } else {
            setting_cursor_pos--;
        }

    }

    update_setting_display();

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


    if (TBI(capture_interrupt_flags, SWITCH_TRIGGER_B) ) {

        // Trigger generated a interrupt

        // As a safety against glitches, we check to make sure it is still actuated now even after the above 50ms delay

        if (!TBI(SWITCH_TRIGGER_PIN, SWITCH_TRIGGER_B) ) {

            // Trigger is currently still activated, so locking ring is rotated to lock and load position!

            stop_setting_mode();

            // combine the setting digits into a numeric value

            unsigned v=0;

            unsigned i= DIGITPLACE_COUNT-1;         // -1 because one of the digit places is used for the units indicator in setting mode, the rest are digits.
            while (i>0) {                           // SLightly complicated so we can work from highest digit to lowest. C++ should have a reverse foreach syntax.
                i--;
                v*=10;
                v+=setting_digits[i];
            }

            // Compute how long the countdown is based on the value and units the user gave us.

            unsigned d,h,m,s;

            unsigned long secs;     // Luckily unsigned long can hold up to 136 years, so we can work with this size as our universal base time unit.

            switch (setting_unit) {

            case setting_units_t::SECS:
                secs = v;
                break;

            case setting_units_t::HOURS:
                secs = v * 60UL * 60UL;                 // 60 seconds per min * 60 mins per hour
                break;

            case setting_units_t::DAYS:
                secs = v * 60UL * 60UL * 24UL;         // .. * 24 hours in a day
                break;

            case setting_units_t::YEARS:
                secs = v * 31556926UL; // https://frinklang.org/fsp/frink.fsp?fromVal=1+solaryear&toVal=seconds#calc

                // "The mean tropical year is approximately 365 days, 5 hours, 48 minutes, 45 seconds."
                // https://en.wikipedia.org/wiki/Tropical_year#:~:text=the%20mean%20tropical%20year%20is%20approximately%20365%20days%2C%205%20hours%2C%2048%20minutes%2C%2045%20seconds.
                // Note that we are depending on the UI code to prevent years from ever being >100 or else our seconds variable here could overflow.

            }


            // Normalize the total seconds to days, hours, mins, and secs
            // This ensures that, say, 120 seconds shows up correctly as 2 min.

            d= (secs / (60UL * 60UL * 24UL));
            secs = secs - ( d * 60UL * 60UL *24UL );

            h= (secs / (60UL * 60UL) );
            secs = secs - ( h * 60UL * 60UL );

            m= secs / (60UL);
            secs = secs - ( m* 60UL);

            s=secs;

            // Start the countdown

            start_countdown_mode(d, h, m, s);


        }
        // CLear pending interrupt
        CBI( SWITCH_TRIGGER_PIFG , SWITCH_TRIGGER_B );  // Implemented as "AND.B   #0x007f,&Port_A_PAIFG"

    }

}


void enable_button_interrupts() {

    // We do not need to wait for the resistors to take effect since we initially will only interrupt
    // on high to low transitions and the pulls will cause a low to high.

    // Interrupt on the next high-to-low transitions
    SBI( SWITCH_CHANGE_PIES , SWITCH_CHANGE_B );
    SBI( SWITCH_MOVE_PIES , SWITCH_MOVE_B );
    SBI( SWITCH_TRIGGER_PIES , SWITCH_TRIGGER_B);

    // Clear any pending interrupts
    CBI( SWITCH_CHANGE_PIFG , SWITCH_CHANGE_B    );
    CBI( SWITCH_MOVE_PIFG , SWITCH_MOVE_B    );
    CBI( SWITCH_TRIGGER_PIFG , SWITCH_TRIGGER_B );

    // Enable pin change interrupt on the pins
    SBI( SWITCH_CHANGE_PIE , SWITCH_CHANGE_B );
    SBI( SWITCH_MOVE_PIE , SWITCH_MOVE_B );
    SBI( SWITCH_TRIGGER_PIE , SWITCH_TRIGGER_B );

}

void disable_button_interrupts() {
    // Disable pin change interrupt on the pins
    // Do this first so we dont get any spurious ints when we drive them low.

    CBI( SWITCH_CHANGE_PIE , SWITCH_CHANGE_B );
    CBI( SWITCH_MOVE_PIE , SWITCH_MOVE_B );
    CBI( SWITCH_TRIGGER_PIE , SWITCH_TRIGGER_B);
}

void disable_buttons() {

    // Below is probably overly cautious, we could leave the button pull-ups on and that would be fine.
    // But JUST IN CASE in 75 years a button fails short, this makes sure that short does not use any
    // power.

    // Disable pull-ups
    // Note it is import to do this before setting PDIR or else if the button happens to be
    // pressed then it could short a hi drive to ground

    CBI( SWITCH_CHANGE_POUT , SWITCH_CHANGE_B );
    CBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );
    CBI( SWITCH_TRIGGER_POUT , SWITCH_TRIGGER_B );

    // Drive pins low to avoid floating.

    SBI( SWITCH_MOVE_PDIR , SWITCH_MOVE_B );      // drive
    SBI( SWITCH_CHANGE_PDIR , SWITCH_CHANGE_B );      // drive
    SBI( SWITCH_TRIGGER_PDIR, SWITCH_TRIGGER_B );      // drive

}

void enable_buttons() {

    // Set pins to input mode

    CBI( SWITCH_MOVE_PDIR    , SWITCH_MOVE_B );      // drive
    CBI( SWITCH_CHANGE_PDIR  , SWITCH_CHANGE_B );      // drive
    CBI( SWITCH_TRIGGER_PDIR , SWITCH_TRIGGER_B );      // drive


    // Enable pull-ups
    // Note it is import to do this after setting PDIR or else if the button happens to be
    // pressed then it could short a hi drive to ground

    SBI( SWITCH_CHANGE_POUT , SWITCH_CHANGE_B );
    SBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );
    SBI( SWITCH_TRIGGER_POUT , SWITCH_TRIGGER_B );

}


void start_setting_mode() {

    // We use the blinking segments mode to make the cursor visible. This is very nice
    // because it is done in the LCD hardware so uses no extra power. Setting this mode
    // also updates the blink speed to be fast which looks good.

    lcd_blinking_mode_segments();
    // We use the main memory bank for the segments we want to show, and the B bank to set which should blink.
    lcd_show_LCDMEM_bank();


    // Start setting mode with 1 hour on the clock, cursor over the ones digit.
    setting_unit = setting_units_t::SECS;
    setting_digits[0]=5;
    setting_digits[1]=0;
    setting_digits[2]=0;
    setting_digits[3]=0;
    setting_digits[4]=0;

    setting_cursor_pos = 1;

    // Show it on the display
    update_setting_display();

    // Arm the buttons so we register the next press
    SBI( switch_armed_flags , SWITCH_CHANGE_B);
    SBI( switch_armed_flags , SWITCH_MOVE_B);

    enable_buttons();
    enable_button_interrupts();
}

void stop_setting_mode() {
    disable_button_interrupts();
    disable_buttons();
}

void tsl_next_day() {
    // PLACEHOLDER
}


// Start counting down!
// When called, it inits the LCD to the starting time and gets everything ready so that subsequent clkout
// Interrupt will update the count. When the count reaches zero, it will open the solenoids and then
// call end_countdown_mode() and then start_setting_mode().

/*
 * You should sleep after calling this with
 *
    // Wait for interrupt to fire at next clkout low-to-high change to drive us into the state machine (in either "pin loading" or "time since launch" mode)
    // Could also enable the trigger pin change ISR if we are in RTL mode.
    // Note if we use LPM3_bits then we burn 18uA versus <2uA if we use LPM4_bits.
    __bis_SR_register(LPM4_bits | GIE );                // Enter LPM4
    __no_operation();                                   // For debugger
 *
 */


void start_countdown_mode( unsigned days, unsigned hours, unsigned mins, unsigned secs) {

    // Restart the RTC starting... now. This means the first interrupt will happen in 1 second - plenty of time for us to do out init work here.
    // This also makes things *feel* right so that the second tick is aligned with whatever user action that got us here.
    rv3032_zero();

    // Switch to LCD mode where we can manually double buffer. We keep the days page on the second LCDBMEM page.
    // The blink none mode prevents the blinking hardware from automatically switching the pages on us,
    // we will do it ourselves.
    lcd_blinking_mode_none();

    // Init the values we use inside the ISR

    countdown_d = days;
    countdown_h = hours;
    countdown_m = mins;
    countdown_s = secs;


    // Get the first HHMMSS up on the LCD for the people to look at
    // We need to do this because the ISR only updates digits that change, so this
    // paints all the digits so something will be there until they next change.

    *secs_lcdmemw    = secs_lcd_words[countdown_s];
    *mins_lcdmemw    = secs_lcd_words[countdown_m];
    *hours_lcdmemw   = secs_lcd_words[countdown_h];

    // Now init that second page with the current day count and the label (which will stay there)
    // Note that the ISR will switch to this page when/if it wants to display it.
    // The ISR will also update this page if the day count changes
    lcd_show_day_label_lcdbmem();
    lcd_show_days_lcdbmem( countdown_d );


    if ( countdown_d >0) {

        // If countdown is more than a day, then show the day count initially
        lcd_show_LCDBMEM_bank();

        if ( countdown_h==0 && countdown_m==0 && countdown_s==0) {
            // Go next to the blank page otherwise it looks weird if 5 days turns directly to 4 days without seeing the HHMMSS yet.
            countdown_display_page = countdown_display_page_t::BLANK;
        } else {
            // If there are also some hours, mins, or seconds then show those. This can happen, say, if the setting was 49 hours (=2 days + 1 hour)
            countdown_display_page = countdown_display_page_t::HHMMSS;
        }

    } else {

        // If less than a day left then show HHMMSS now (and for the rest of the countdown)
        lcd_show_LCDMEM_bank();
        countdown_display_page = countdown_display_page_t::HHMMSS;

    }

    // Now compute the values for the backup counters....
    // TODO: compute backup counters

    // Enable the interrupts so we wake up on each rising edge of the RV3032 clkout.
    // The RV3032 is always set to 1Hz so this will wake us once per second.

    // Now we enable the interrupt on the RTC CLKOUT pin. For now on we must remember to
    // disable it again if we are going to end up in sleepforever mode.

    // Clear any pending interrupts from the RV3032 clkout pin and then enable interrupts for the next falling edge
    // This will start calling the ISR on the next tick.

    enable_rv3032_clkout_interrupt();

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


    //lcd_cls_LCDMEM();

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

    start_setting_mode();
    sleep_with_interrupts();                    // Wait for interrupts to take over.

    // should never never get here.

    start_countdown_mode(2001, 1 , 1, 10);
    sleep_with_interrupts();                    // Wait for interrupts to take over.


    // Simple Solenoid tester

    // Set buttons as inputs
    CBI( SWITCH_CHANGE_PDIR , SWITCH_CHANGE_B);
    CBI( SWITCH_MOVE_PDIR   , SWITCH_MOVE_B);

    // Pull-up pins
    // Note that we set the pull mode to UP in initGPIO
    // and setting out to 1 when PDIR is clear will enable the pull-up

    SBI( SWITCH_CHANGE_POUT , SWITCH_CHANGE_B );
    SBI( SWITCH_MOVE_POUT , SWITCH_MOVE_B );


    // Set the day page up for now.
    lcd_blinking_mode_none();
    lcd_show_day_label_lcdbmem();
    lcd_show_days_lcdbmem(  200 );


    while (1) {

        if (!TBI( SWITCH_CHANGE_PIN , SWITCH_CHANGE_B)) {

            lcd_show_LCDBMEM_bank();

        }

        if (!TBI( SWITCH_MOVE_PIN , SWITCH_MOVE_B)) {

            lcd_show_LCDMEM_bank();

        }
    }


    while (1) {

        if (!TBI( SWITCH_CHANGE_PIN , SWITCH_CHANGE_B)) {

            // Button Down

            countdown_h=0; countdown_m=0; countdown_s=15;

            rv3032_zero();

            lcd_show_LCDBMEM_bank();

            while (1);


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
    //locked_mode();

    // Start normal operation!

    while (1) {

        switch (mode) {

        case mode_t::SETTING:
            //setting_mode();
            break;

        case mode_t::LOCKED:
            //locked_mode();
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


    error_mode( ERROR_MAIN_RETURN );                    // This would be very weird if we ever saw it.

}




