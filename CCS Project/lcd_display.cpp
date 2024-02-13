/*
 * lcd_display.cpp
 *
 *  Created on: Jan 20, 2023
 *      Author: josh
 */


// Needed for LCDMEM addresses





// Get all the definitions we need to use the LCD


#include <msp430.h>

#include "util.h"


#include "define_lcd_pinout.h"
#include "define_lcd_to_msp430_connections.h"
#include "define_msp430_lcd_device.h"
#include "define_lcd_font.h"


#include "lcd_display.h"
#include "lcd_display_exp.h"



void lcd_segment_set( char * lcdmem_base , lcd_segment_location_t seg  ) {

    byte lpin = lcdpin_to_lpin[ seg.lcd_pin ];

    lcdmem_base[ LCDMEM_OFFSET_FOR_LPIN( lpin ) ] |= lcd_shifted_com_bits( lpin , seg.lcd_com );

}

void lcd_segment_set_to_lcdmem( lcd_segment_location_t seg  ) {

    lcd_segment_set( LCDMEM , seg );


}

void lcd_segment_clear( char * lcdmem_base , lcd_segment_location_t seg  ) {

    byte lpin = lcdpin_to_lpin[ seg.lcd_pin ];

    lcdmem_base[ LCDMEM_OFFSET_FOR_LPIN( lpin ) ] &= ~lcd_shifted_com_bits( lpin , seg.lcd_com );

}

void lcd_segment_clear_to_lcdmem( lcd_segment_location_t seg  ) {

    lcd_segment_clear( LCDMEM, seg );

}


// General function to write a glyph onto memory. That could be main LCDMEM or "blinking" LCDBM
// which can also be used for double buffering.
// No constraints on how pins are connected, but inefficient.
// Remember that means that different segments in the same digit could be at different LCDMEM locations!


void lcd_write_glyph_to_lcdmem( char *lcdmem_base , byte digitplace, glyph_segment_t glyph ) {

    lcd_digit_segments_t digit_segments = lcd_digit_segments[ digitplace ];

    if ( glyph & SEG_A_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_A);
    } else {
        lcd_segment_clear(lcdmem_base , digit_segments.SEG_A);
    }

    if ( glyph & SEG_B_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_B);
    } else {
        lcd_segment_clear(lcdmem_base , digit_segments.SEG_B);
    }

    if ( glyph & SEG_C_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_C);
    } else {
        lcd_segment_clear(lcdmem_base , digit_segments.SEG_C);
    }

    if ( glyph & SEG_D_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_D);
    } else {
        lcd_segment_clear(lcdmem_base , digit_segments.SEG_D);
    }

    if ( glyph & SEG_E_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_E);
    } else {
        lcd_segment_clear(lcdmem_base , digit_segments.SEG_E);
    }

    if ( glyph & SEG_F_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_F);
    } else {
        lcd_segment_clear(lcdmem_base , digit_segments.SEG_F);
    }

    if ( glyph & SEG_G_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_G);
    } else {
        lcd_segment_clear(lcdmem_base , digit_segments.SEG_G);
    }

}


// General function to write a glyph onto the LCD. No constraints on how pins are connected, but inefficient.
// Remember that means that different segments in the same digit could be at different LCDMEM locations!

void lcd_write_glyph_to_lcdmem( byte digitplace, glyph_segment_t glyph ) {

    lcd_write_glyph_to_lcdmem( LCDMEM , digitplace, glyph);

}


// Write a glyph to the blinking LCD buffer

void lcd_write_glyph_to_lcdbm( byte digitplace, glyph_segment_t glyph ) {

    lcd_write_glyph_to_lcdmem( LCDBMEM , digitplace, glyph);

}

void lcd_write_blank( char *lcdmem_base , byte digitplace ) {

    lcd_digit_segments_t digit_segments = lcd_digit_segments[ digitplace ];

    lcd_segment_clear( lcdmem_base , digit_segments.SEG_A);
    lcd_segment_clear( lcdmem_base , digit_segments.SEG_B);
    lcd_segment_clear( lcdmem_base , digit_segments.SEG_C);
    lcd_segment_clear( lcdmem_base , digit_segments.SEG_D);
    lcd_segment_clear( lcdmem_base , digit_segments.SEG_E);
    lcd_segment_clear( lcdmem_base , digit_segments.SEG_F);
    lcd_segment_clear( lcdmem_base , digit_segments.SEG_G);

}



void lcd_write_blank_to_lcdmem( byte digitplace ) {

    lcd_write_blank(LCDMEM, digitplace);

}


void lcd_write_blank_to_lcdbm( byte digitplace ) {

    lcd_write_blank(LCDBMEM, digitplace);

}


// Do a hardware clear of the LCD memory
// Note this does block until the hardware indicates that the clear is complete.
// TODO: Check how long this takes.

void lcd_cls_LCDMEM() {
    LCDMEMCTL |= LCDCLRM;                                      // Clear LCD memory
    while ( LCDMEMCTL & LCDCLRM );                             // Wait for clear to complete.
}

void lcd_cls_LCDMEM_nowait() {
    LCDMEMCTL |= LCDCLRM;                                      // Clear LCD memory
}


// Thanks chatGPT for this function! :)
template<typename T>
constexpr bool areAllEqual(const T& first) {
    return true;
}

template<typename T, typename... Args>
constexpr bool areAllEqual(const T& first, const T& second, const Args&... args) {
    return (first == second) && areAllEqual(second, args...);
}


// returns true if all of the segments in the digit place are all contained in a single writeable word in LCDMEM
// This depends on the PCB layout , and we were careful to make sure this will be true since it lets us update these
// two digits on the display with a single instruction.


constexpr bool test_all_digit_segments_contained_in_one_word( const lcd_digit_segments_t segments ) {

    return areAllEqual(

            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_A.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_B.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_C.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_D.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_E.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_F.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_G.lcd_pin ]  )

    );

}


// returns true if all of the segments in the two digits places are all contained in a single writeable word in LCDMEM
// This depends on the PCB layout , and we were careful to make sure this will be true since it lets us update these
// two digits on the display with a single instruction.


constexpr bool test_all_digit_segments_contained_in_one_word( const lcd_digit_segments_t digitplace_1_segments  , const lcd_digit_segments_t digitplace_2_segments) {

    return areAllEqual(

            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_1_segments.SEG_A.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_1_segments.SEG_B.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_1_segments.SEG_C.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_1_segments.SEG_D.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_1_segments.SEG_E.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_1_segments.SEG_F.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_1_segments.SEG_G.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_2_segments.SEG_A.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_2_segments.SEG_B.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_2_segments.SEG_C.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_2_segments.SEG_D.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_2_segments.SEG_E.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_2_segments.SEG_F.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ digitplace_2_segments.SEG_G.lcd_pin ]  )

    );

}

// Returns a word that you would OR into the appropriate LCDMEMW word to turn on the specified segment

constexpr unsigned int word_bits_for_segment( lcd_segment_location_t seg_location ) {

    byte lpin = lcdpin_to_lpin[ seg_location.lcd_pin ];

    // Note there was a bug on the line below that took a whole day to find. The type was `byte` and instead of the compiler saying "may exceed type size",
    // instead it threw "value must be constant" far, far away and only when the values passed had the lowest bit set. Arg.

    unsigned int com_bits = lcd_shifted_com_bits( lpin , seg_location.lcd_com );  // This is the value we would put into an LCDMEM byte

    if ( LCDMEM_OFFSET_FOR_LPIN(seg_location.lcd_pin) & 1 ) {           // Is this lpin in the top byte of the LCDMEM word?

        return com_bits << 8;

    } else {

        return com_bits;
    }

}

// THis returns a word that you would OR into the appropriate LCDMEM word to turn on the segments in the specified digitplace to show the specified glyph

constexpr unsigned int glyph_bits( const lcd_digit_segments_t digitplace_segs , const glyph_segment_t glyph ) {

    word bits = 0;          // Start with all off

    if ( glyph & SEG_A_BIT ) {
        bits |=   word_bits_for_segment( digitplace_segs.SEG_A);
    }

    if ( glyph & SEG_B_BIT ) {
        bits |=   word_bits_for_segment( digitplace_segs.SEG_B);
    }

    if ( glyph & SEG_C_BIT ) {
        bits |=   word_bits_for_segment( digitplace_segs.SEG_C);
    }

    if ( glyph & SEG_D_BIT ) {
        bits |=   word_bits_for_segment( digitplace_segs.SEG_D);
    }

    if ( glyph & SEG_E_BIT ) {
        bits |=   word_bits_for_segment( digitplace_segs.SEG_E);
    }

    if ( glyph & SEG_F_BIT ) {
        bits |=   word_bits_for_segment( digitplace_segs.SEG_F);
    }

    if ( glyph & SEG_G_BIT ) {
        bits |=   word_bits_for_segment( digitplace_segs.SEG_G);
    }

    return bits;

}


// Here is generic code for initializing a constexpr array in C++ 2014
// Based on...
// https://stackoverflow.com/a/34465458/3152071
// ...but had to be modified due to this horrible problem in <MSP430.h>...
// https://stackoverflow.com/questions/69149459/what-causes-the-error-18-expected-a-on-a-msp430
// Use like...
// constexpr auto myArrayStruct = ConstexprArray<square,10>();
// constexpr auto myArray = myArrayStruct.array;


template< unsigned int (*generator_function)(unsigned int) ,  int size>
struct ConstexprArray {
    constexpr ConstexprArray() : array() {
        for (auto i = 0; i != size; ++i)
            array[i] = generator_function(i);
    }
    unsigned int array[size];
};


// This function will generate the words that will go into the compile time cache arrays

template < int tens_digitplace ,  int ones_digitplace , int radix >
constexpr unsigned int generate_lcd_cache_word( unsigned int number ) {

    constexpr lcd_digit_segments_t ones_digitplace_segments = lcd_digit_segments[ones_digitplace];
    constexpr lcd_digit_segments_t tens_digitplace_segments = lcd_digit_segments[tens_digitplace];

    // Cross check to make sure the current LCD layout and connections are compatible with this optimization
    static_assert( test_all_digit_segments_contained_in_one_word(  tens_digitplace_segments,  ones_digitplace_segments )   , "All of the segments in the seconds ones and tens digits must be in the same LCDMEM word for this optimization to work" );

    unsigned int tens_digit = number / radix;
    unsigned int ones_digit = number - ( tens_digit * radix );

    // Start with all of bits for the segments for the tens digit turned on
    unsigned int tens_digitplace_bits = glyph_bits( tens_digitplace_segments , digit_glyphs[ tens_digit ] );

    unsigned int ones_digitplace_bits = glyph_bits( ones_digitplace_segments , digit_glyphs[ ones_digit ] );

    // Combines all the segments that need to be lit to show this two digit number
    return  tens_digitplace_bits | ones_digitplace_bits;

}


// these arrays hold the pre-computed words that we will write to word in LCD memory that
// contain the pairs of seconds and mins digits on the LCD. We keep these in RAM intentionally for power and latency savings.
// use fill_lcd_words() to fill these arrays.
// Assumes the numbers are base 10.

constexpr unsigned int DEC = 10;
constexpr unsigned int HEX = 16;

constexpr unsigned int size = 100;


// Define and compile-time fill the LCD cache arrays. NOte that these will fail at compile time if the pins layout is not compatible with this optimization.
constexpr auto secs_lcd_word_cache_struct  = ConstexprArray< generate_lcd_cache_word< SECS_TENS_DIGITPLACE  , SECS_ONES_DIGITPLACE  , DEC  >, size >();
constexpr auto mins_lcd_word_cache_struct  = ConstexprArray< generate_lcd_cache_word< MINS_TENS_DIGITPLACE  , MINS_ONES_DIGITPLACE  , DEC  >, size >();
constexpr auto hours_lcd_word_cache_struct = ConstexprArray< generate_lcd_cache_word< HOURS_TENS_DIGITPLACE , HOURS_ONES_DIGITPLACE , DEC  >, size >();


// Make the addresses of the cache arrays available to ASM IST
#pragma RETAIN
constexpr const unsigned int * const secs_lcd_words =secs_lcd_word_cache_struct.array;
#pragma RETAIN
constexpr const unsigned int * const mins_lcd_words =mins_lcd_word_cache_struct.array;
#pragma RETAIN
constexpr const unsigned int * const hours_lcd_words =hours_lcd_word_cache_struct.array;


// Make the addresses where you should write the cached values to in order to display the specified secs on the LCD
// Note that we can take any pin (we picked SEG_A) since the above compile time checks ensure that any of the segments would produce the same address
#pragma RETAIN
unsigned int *secs_lcdmemw =&( LCDMEMW[  ( LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[  lcd_digit_segments[ SECS_ONES_DIGITPLACE ].SEG_A.lcd_pin ] ) ) ] );
#pragma RETAIN
unsigned int *mins_lcdmemw =&( LCDMEMW[  ( LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[  lcd_digit_segments[ MINS_ONES_DIGITPLACE ].SEG_A.lcd_pin ] ) ) ] );
#pragma RETAIN
unsigned int *hours_lcdmemw =&( LCDMEMW[  ( LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[  lcd_digit_segments[ HOURS_ONES_DIGITPLACE ].SEG_A.lcd_pin ] ) ) ] );



// Define a full LCD frame so we can put it into LCD memory in one shot.
// Note that a frame only includes words of LCD memory that have actual pins used. This is hard coded here and in the
// RTL_MODE_ISR. There are a total of only 8 words spread across 2 extents. This is driven by the PCB layout.
// It is faster to copy a sequence of words than try to only set the nibbles that have changed.


// Show the digit x at position p
// where p=0 is the rightmost digit

template <uint8_t pos, uint8_t x>                    // Use template to force everything to compile down to an individualized, optimized function for each pos+x combination
inline void lcd_show() {

    uint8_t * const lcd_mem_reg = LCDMEM;

    /*
      I know that line above looks dumb, but if you just access LCDMEM directly the compiler does the wrong thing and loads the offset
      into a register rather than the base and this creates many wasted loads (LCDMEM=0x0620)...

            00c4e2:   403F 0010           MOV.W   #0x0010,R15
            00c4e6:   40FF 0060 0620      MOV.B   #0x0060,0x0620(R15)
            00c4ec:   403F 000E           MOV.W   #0x000e,R15
            00c4f0:   40FF 00F2 0620      MOV.B   #0x00f2,0x0620(R15)

      If we load it into a variable first, the compiler then gets wise and uses that as the base..

            00c4e2:   403F 0620           MOV.W   #0x0620,R15
            00c4e6:   40FF 0060 0010      MOV.B   #0x0060,0x0010(R15)
            00c4ec:   40FF 00F2 000E      MOV.B   #0x00f2,0x000e(R15)

     */

    constexpr byte digitplace = digit_positions_rj( pos );

    lcd_write_glyph_to_lcdmem( digitplace , digit_glyphs[x] );

}


// Show the glyph x at position p
// where p=0 is the rightmost digit


void lcd_show_f( char *lcdmem_base ,  const uint8_t pos, const glyph_segment_t segs ) {

    byte digitplace = digit_positions_rj( pos );

    lcd_write_glyph_to_lcdmem( lcdmem_base , digitplace , segs );

}


void lcd_show_f( const uint8_t pos, const glyph_segment_t segs ) {


    lcd_show_f( LCDMEM ,  pos , segs );

}


void lcd_show_digit_f( const uint8_t pos, const byte d ) {

    lcd_show_f( pos , digit_glyphs[ d ] );
}


void lcd_show_digit_f( char *lcd_base , const uint8_t pos, const byte d ) {

    lcd_show_f( lcd_base , pos , digit_glyphs[ d ] );
}



// Print the current days value into the LCDBMEM buffer
// Currently leading spaces, but could be leading 0s
// TODO: This could be optimized to use the precomputed 2-digit tables, but is it worth it?

void lcd_show_days_lcdbmem( const unsigned days ) {

    unsigned x=days;
    unsigned pow = 10000;
    unsigned place=5;

    // Pad with spaces on left
    while (pow>x) {

        lcd_show_f(LCDBMEM , place, glyph_SPACE);
        place--;
        pow=pow/10;

    }


    while (pow>0) {
        unsigned char y = x / pow;                  // we know the result of this divide must be less than 10
        lcd_show_digit_f( LCDBMEM , place , y);
        place--;

        x = x - (y * pow);
        pow = pow / 10;

    }


}


void lcd_show_day_label_lcdbmem() {
    lcd_show_f( LCDBMEM , 0, glyph_d);
}


inline void lcd_show_fast_secs( uint8_t secs ) {

    *secs_lcdmemw = secs_lcd_words[  secs ];

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

    // Note if you change these, you also need to change the LCDBM assignments in the blinking mode change functons

    LCDM4 =  0b00100001;  // L09=MSP_COM1  L08=MSP_COM0
    LCDM5 =  0b10000100;  // L10=MSP_COM3  L11=MSP_COM2

    // Enable per-segment blinking. If the bit is set in LCDMBEM then the segment will blink.
    // The blink speed is pretty fast, about 10Hz. We use this to indicate which digit is selected
    // in SETTING mode.
    // "Settings for LCDMXx and LCDBLKPREx should only be changed while LCDBLKMODx = 00.""

    LCDBLKCTL =
            LCDBLKPRE__8 |            // Blinking frequency prescaller. This controls how fast the blink is, about 2Hz.
            LCDBLKMOD_0               // Blinking disabled
    ;


    LCDCTL0 |= LCDON;                                           // Turn on LCD

}



/*
 * LCD segments on. This bit supports flashing LCD applications by turning off all
    segment lines, while leaving the LCD timing generator and R33 enabled.
    0b = All LCD segments are off.
    1b = All LCD segments are enabled and on or off according to their
    corresponding memory location.
 */

// Instantly blank LCD segments in hardware (does not alter memory)

void lcd_off() {
    LCDCTL0 &= ~LCDSON;     // 0b = All LCD segments are off.
                            // TODO: This would be faster with a direct immediate write
}

// Unblank all LCD segments in hardware

void lcd_on() {
    LCDCTL0 |=  LCDSON;         // 1b = All LCD segments are enabled and on or off according to their corresponding memory location.
                                // TODO: This would be faster with a direct immediate write
}


/*
 * Select LCD memory registers for display
    When LCDBLKMODx = 00, LCDDISP can be set by software.
    The bit is cleared in LCDBLKMODx = 01 and LCDBLKMODx = 10 or if a mux
    mode >=5 is selected and cannot be changed by software.
    When LCDBLKMODx = 11, this bit reflects the currently displayed memory but
    cannot be changed by software. When returning to LCDBLKMODx = 00 the bit is
    cleared.
    0b = Display content of LCD memory registers LCDMx
    1b = Display content of LCD blinking memory registers LCDBMx
 */


// Show the main mem bank on LCD
void lcd_show_LCDMEM_bank() {
    LCDMEMCTL &= ~LCDDISP;              // TODO: Make faster with direct write
}

// Show the secondary (LCDBMEM) bank on the lcd
void lcd_show_LCDBMEM_bank() {
    LCDMEMCTL |= LCDDISP;               //  TODO: Make faster with direct write
}

/*
 * Changing blinking modes is slightly weird on this chip since different modes have different requirements for the LCDBMEM addresses used by the COM pins
 *
 *
    Blinking Mode LCDBLKMODx Description
    00b          Blinking disabled, the user can select which memory to be displayed by setting LCDDISP bit in LCDMEMCTL register
                 LCDMx: the COM related configuration bits should be set accordingly
                 LCDBMx: the COM related configuration bits should be set according to LCDMx configuration
    01b          Blinking of individual segments as enabled in blinking memory register LCDBMx
                 LCDMx: the COM related memory bits should be set accordingly
                 LCDBMx: the COM related memory bits should be set to 0
    10b          Blinking of all segments
                 LCDMx: the COM related memory bits should be set accordingly
                 LCDBMx: this memory is not used in this blinking mode, no programming of LCDBMx necessary
    11b          Switching between display contents as stored in LCDMx and LCDBMx memory registers
                 LCDMx: the COM related memory bits should be set accordingly
                 LCDBMx: the COM related memory bits should be set according to LCDMx configuration

 *
 *
 */


// Put the LCD into no blinking mode. Note that in this mode you can use the bank select functions to pick which pages is shown.
void lcd_blinking_mode_none() {

    // "Settings for LCDMXx and LCDBLKPREx should only be changed while LCDBLKMODx = 00."
    LCDBLKCTL &= ~LCDBLKMOD_3;       // Clear the LCDBLKMODx bits. This seems superfluous.

    // "LCDBMx: the COM related configuration bits should be set according to LCDMx configuration"
    LCDBM4 =  LCDM4;            // TODO: These could be faster, and do we really need them? Doesn't seem to actually matter.
    LCDBM5 =  LCDM5;

    LCDBLKCTL = LCDBLKPRE__16 | LCDBLKMOD_0;       // Clock prescaler for blink rate, "Blinking of individual segments as enabled in blinking memory register LCDBMx."
}

// Put the LCD into blinking mode where any segment in LCDBMEM blinks
void lcd_blinking_mode_segments() {

    // "Settings for LCDMXx and LCDBLKPREx should only be changed while LCDBLKMODx = 00."
    LCDBLKCTL &= ~LCDBLKMOD_3;       // Clear the LCDBLKMODx bits

    // "LCDBMx: the COM related memory bits should be set to 0"
    LCDBM4 =  0x00;            // TODO: Do we really need them? Doesn't seem to actually matter.
    LCDBM5 =  0x00;

    LCDBLKCTL = LCDBLKPRE__16 | LCDBLKMOD_1;       // Clock prescaler for blink rate, "Blinking of individual segments as enabled in blinking memory register LCDBMx."
}

// Put the LCD into blinking mode where all segments blink
void lcd_blinking_mode_all() {

    // "Settings for LCDMXx and LCDBLKPREx should only be changed while LCDBLKMODx = 00."
    LCDBLKCTL &= ~LCDBLKMOD_3;       // Clear the LCDBLKMODx bits


    // "LCDBMx: this memory is not used in this blinking mode, no programming of LCDBMx necessary"

    LCDBLKCTL = LCDBLKPRE__64 | LCDBLKMOD_2;       // Clock prescaler for blink rate, "Blinking of all segments as enabled in blinking memory register LCDBMx."
}

// Put the LCD into double page buffer mode where it will automatically switch between LCDMEM and LCDBMEM
// Sets the DIV to the max of 512 so will only blink about once every 5 secs. This is because we will actually do the switching manually in the ISR.
// You can reset the counter so that it never switches automatically by calling  lcd_blinking_mode_doublebuffer_reset_timer()

void lcd_blinking_mode_doublebuffer() {

    // "Settings for LCDMXx and LCDBLKPREx should only be changed while LCDBLKMODx = 00."
    LCDBLKCTL &= ~LCDBLKMOD_3;       // Clear the LCDBLKMODx bits. Also clears the DIV counter.

    // "LCDBMx: the COM related memory bits should be set according to LCDMx configuration"
    LCDBM4 =  LCDM4;            // TODO: These could be faster, and do we really need them? Doesn't seem to actually matter.
    LCDBM5 =  LCDM5;

    LCDBLKCTL = LCDBLKPRE__512 | LCDBLKMOD_3;       // Clock prescaler for blink rate, "11b = Switching between display contents as stored in LCDMx and LCDBMx memory registers."
}

void lcd_blinking_mode_doublebuffer_reset_timer() {

    // "The divider generating the blinking frequency fBLINK is reset when LCDBLKMODx = 00."
    LCDBLKCTL = LCDBLKPRE__512 | LCDBLKMOD_0;      // Clear the DIV counter
    LCDBLKCTL = LCDBLKPRE__512 | LCDBLKMOD_3;       // Clock prescaler for blink rate, "11b = Switching between display contents as stored in LCDMx and LCDBMx memory registers."

}



// For now, show all 9's.
// TODO: Figure out something better here

void lcd_show_long_now() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_digit_f( i , 9 );

    }

}


/*

// Original non-optimized version.

// Show a frame n the ready-to-lanuch squiggle animation
// 0 <= step < SQUIGGLE_ANIMATION_FRAME_COUNT
// 839us, ~1.5uA

void lcd_show_squiggle_frame( byte step ) {

    static_assert( SQUIGGLE_SEGMENTS_SIZE == 8 , "SQUIGGLE_SEGMENTS_SIZE should be exactly 8 so we can use a logical AND to keep it in bounds for efficiency");            // So we can use fast AND 0x07
    static_assert( SQUIGGLE_ANIMATION_FRAME_COUNT == 8 , "SQUIGGLE_ANIMATION_FRAME_COUNT should be exactly 8 so we can use a logical AND to keep it in bounds for efficiency");            // So we can use fast AND 0x07

    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        if ( i & 0x01 ) {
            lcd_show_f( i , squiggle_segments[ ((SQUIGGLE_SEGMENTS_SIZE + 4 )-  step ) & 0x07 ]);
        } else {
            lcd_show_f( i , squiggle_segments[ step ]);
        }

    }

}

*/


// Fill the screen with horizontal dashes

void lcd_show_dashes() {

    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_f( i , glyph_dash );

    }
}


// Fill the screen with 0's

void lcd_show_zeros() {

    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_f( i , glyph_0 );

    }
}

// Fill the screen with X's

void lcd_show_XXX() {

    for( uint8_t i=0 ; i<DIGITPLACE_COUNT; i++ ) {

        lcd_show_f( i , glyph_X );

    }
}



constexpr glyph_segment_t testingonly_message[] = {
                                                   glyph_t,
                                                   glyph_E,
                                                   glyph_S,
                                                   glyph_t,
                                                   glyph_I,
                                                   glyph_n,
                                                   glyph_g,
                                                   glyph_SPACE,
                                                   glyph_O,
                                                   glyph_n,
                                                   glyph_L,
                                                   glyph_y,
};


void lcd_show_testing_only_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , testingonly_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }

}


// "bAtt Error X"

constexpr glyph_segment_t batt_errorcode_message[] = {
     glyph_b, // "b"
     glyph_A, // "A"
     glyph_t,  // t
     glyph_t,  // t
     glyph_SPACE, // " "
     glyph_E, // "E"
     glyph_r, // "r"
     glyph_r, // "r"
     glyph_o, // "o"
     glyph_r, // "r"
     glyph_SPACE, // " "
     glyph_X, // "X"

};

// Show the message "bAtt Error X" on the lcd.

void lcd_show_batt_errorcode( byte code  ) {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , batt_errorcode_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }

    lcd_show_digit_f( 0 , code );

}


// "Error CodE X"

constexpr glyph_segment_t errorcode_message[] = {
    glyph_E, // "E"
    glyph_r, // "r"
    glyph_r, // "r"
    glyph_o, // "o"
    glyph_r, // "r"
    glyph_SPACE, // " "
    glyph_C, // "C"
    glyph_o, // "o"
    glyph_d, // "d"
    glyph_E, // "E"
    glyph_SPACE, // " "
    glyph_X, // "X"

};

// Show the message "Error X" on the lcd.

void lcd_show_errorcode( byte code  ) {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , errorcode_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }

    lcd_show_digit_f( 0 , code );

}

// LoAd PIn --=

constexpr glyph_segment_t load_pin_message[] = {
                                                 glyph_L,
                                                 glyph_O,
                                                 glyph_A,
                                                 glyph_d,
                                                 glyph_SPACE,
                                                 glyph_P,
                                                 glyph_i,
                                                 glyph_n,
                                                 glyph_SPACE,
                                                 glyph_SPACE,
                                                 glyph_SPACE,
                                                 glyph_SPACE,
};


void lcd_show_load_pin_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , load_pin_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 0 is rightmost, so reverse order for text
    }

}

void lcd_show_load_pin_animation(unsigned int step) {

    switch( step ) {

        case 0:
            lcd_show_f(  3 , glyph_dash );
            break;

        case 1:
            lcd_show_f(  3 , glyph_SPACE );
            lcd_show_f(  2 , glyph_dash );
            break;

        case 2:
            lcd_show_f(  2 , glyph_SPACE );
            lcd_show_f(  1 , glyph_dash );
            break;

        case 3:
            lcd_show_f(  1 , glyph_SPACE );
            lcd_show_f(  0 , glyph_dash );
            break;

        case 4:
            lcd_show_f(  0 , glyph_SPACE );
            break;

        default:
            __never_executed();

    }

}


constexpr glyph_segment_t open_message[] = {
                                                   glyph_SPACE,
                                                   glyph_O,
                                                   glyph_P,
                                                   glyph_E,
                                                   glyph_n,
                                                   glyph_SPACE,
};

// Show " OPEn"
void lcd_show_open_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , open_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}




constexpr glyph_segment_t start_message[] = {
                                                   glyph_lbrac,
                                                   glyph_topbot,
                                                   glyph_topbot,
                                                   glyph_topbot,
                                                   glyph_topbot,
                                                   glyph_rbrac
};

// Show "First Start"
void lcd_show_start_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , start_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}



// Refresh day 100's places digits
void lcd_show_centesimus_dies_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
//        lcd_show_f(  i , centesimus_dies_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}
