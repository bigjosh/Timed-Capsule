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
    }

    if ( glyph & SEG_B_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_B);
    }

    if ( glyph & SEG_C_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_C);
    }

    if ( glyph & SEG_D_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_D);
    }

    if ( glyph & SEG_E_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_E);
    }

    if ( glyph & SEG_F_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_F);
    }

    if ( glyph & SEG_G_BIT ) {
        lcd_segment_set( lcdmem_base , digit_segments.SEG_G);
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

void lcd_cls() {
    LCDMEMCTL |= LCDCLRM;                                      // Clear LCD memory
    while ( LCDMEMCTL & LCDCLRM );                             // Wait for clear to complete.
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

    if (x>10000) {

        unsigned y = x/10000;
        lcd_show_digit_f( LCDBMEM , 5 , x);

        x = x - (y*10000);
    } else {
        lcd_write_glyph_to_lcdmem( LCDBMEM , 5 , glyph_SPACE);
    }


    if (x>1000) {

        unsigned y = x/1000;
        lcd_show_digit_f(LCDBMEM , 4 , x);

        x = x - (y*1000);
    } else {
        lcd_write_glyph_to_lcdmem( LCDBMEM , 4 , glyph_SPACE);
    }


    if (x>100) {

        unsigned y = x/100;
        lcd_show_digit_f(LCDBMEM , 3 , x);

        x = x - (y*100);
    } else {
        lcd_write_glyph_to_lcdmem( LCDBMEM , 3 , glyph_SPACE);
    }


    if (x>10) {

        unsigned y = x/10;
        lcd_show_digit_f(LCDBMEM , 2 , x);

        x = x - (y*10);
    } else {
        lcd_write_glyph_to_lcdmem( LCDBMEM , 2 , glyph_SPACE);
    }


    // Always show rightmost digit even if 0
    lcd_show_digit_f(LCDBMEM , 1 , x);

}


void lcd_show_day_label_lcdbmem() {
    lcd_write_glyph_to_lcdmem( LCDBMEM , 0 , glyph_d );
}


inline void lcd_show_fast_secs( uint8_t secs ) {

    *secs_lcdmemw = secs_lcd_words[  secs ];

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


constexpr glyph_segment_t first_start_message[] = {
                                                   glyph_F,
                                                   glyph_i,
                                                   glyph_r,
                                                   glyph_S,
                                                   glyph_t,
                                                   glyph_SPACE,
                                                   glyph_S,
                                                   glyph_t,
                                                   glyph_A,
                                                   glyph_r,
                                                   glyph_t,
                                                   glyph_SPACE,
};

// Show "First Start"
void lcd_show_first_start_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , first_start_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}

// " -Armning-  "

constexpr glyph_segment_t arming_message[] = {
                                                   glyph_SPACE,
                                                   glyph_SPACE,
                                                   glyph_A,
                                                   glyph_r,
                                                   glyph_m1,
                                                   glyph_m2,
                                                   glyph_i,
                                                   glyph_n,
                                                   glyph_g,
                                                   glyph_SPACE,
                                                   glyph_SPACE,
                                                   glyph_SPACE,
};

// Show "Arming"
void lcd_show_arming_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , arming_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}

// CLOCK GOOd

constexpr glyph_segment_t clock_good_message[] = {
                                                   glyph_SPACE,
                                                   glyph_C,
                                                   glyph_L,
                                                   glyph_O,
                                                   glyph_C,
                                                   glyph_K,
                                                   glyph_SPACE,
                                                   glyph_g,
                                                   glyph_O,
                                                   glyph_O,
                                                   glyph_d,
                                                   glyph_SPACE,
};

// Show "CLOCK GOOd"
void lcd_show_clock_good_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , clock_good_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}



// CLOCK LOSt

constexpr glyph_segment_t clock_lost_message[] = {
                                                   glyph_SPACE,
                                                   glyph_C,
                                                   glyph_L,
                                                   glyph_O,
                                                   glyph_C,
                                                   glyph_K,
                                                   glyph_SPACE,
                                                   glyph_L,
                                                   glyph_O,
                                                   glyph_S,
                                                   glyph_t,
                                                   glyph_SPACE,
};

// Show "CLOCK LOSt"
void lcd_show_clock_lost_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
        lcd_show_f(  i , clock_lost_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}



// Refresh day 100's places digits
void lcd_show_centesimus_dies_message() {

    for( byte i=0; i<DIGITPLACE_COUNT; i++ ) {
//        lcd_show_f(  i , centesimus_dies_message[ DIGITPLACE_COUNT - 1- i] );        // digit place 12 is rightmost, so reverse order for text
    }
}
