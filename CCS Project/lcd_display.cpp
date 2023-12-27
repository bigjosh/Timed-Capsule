/*
 * lcd_display.cpp
 *
 *  Created on: Jan 20, 2023
 *      Author: josh
 */


// Needed for LCDMEM addresses
#include <msp430.h>

#include "util.h"

// Get all the definitions we need to use the LCD
#include "define_lcd_pinout.h"
#include "define_lcd_to_msp430_connections.h"
#include "define_msp430_lcd_device.h"
#include "define_lcd_font.h"

#include "lcd_display.h"
#include "lcd_display_exp.h"


void lcd_segment_set( byte * lcdmem_base , lcd_segment_location_t seg  ) {

    byte lpin = lcdpin_to_lpin[ seg.lcd_pin ];

    lcdmem_base[ LCDMEM_OFFSET_FOR_LPIN( lpin ) ] |= lcd_shifted_com_bits( lpin , seg.lcd_com );

}

void lcd_segment_set_to_lcdmem( lcd_segment_location_t seg  ) {

    lcd_sement_set( LCDMEM , seg );


}

void lcd_segment_clear_to_lcdmem( lcd_segment_location_t seg  ) {

    byte lpin = lcdpin_to_lpin[ seg.lcd_pin ];

    LCDMEM[ LCDMEM_OFFSET_FOR_LPIN( lpin ) ] &= ~lcd_shifted_com_bits( lpin , seg.lcd_com );

}


// General function to write a glyph onto the LCD. No constraints on how pins are connected, but inefficient.
// Remember that means that different segments in the same digit could be at different LCDMEM locations!

void lcd_write_glyph_to_lcdmem( byte digitplace, glyph_segment_t glyph ) {

    lcd_digit_segments_t digit_segments = lcd_digit_segments[ digitplace ];

    if ( glyph & SEG_A_BIT ) {
        lcd_segment_set_to_lcdmem(digit_segments.SEG_A);
    }

    if ( glyph & SEG_B_BIT ) {
        lcd_segment_set_to_lcdmem(digit_segments.SEG_B);
    }

    if ( glyph & SEG_C_BIT ) {
        lcd_segment_set_to_lcdmem(digit_segments.SEG_C);
    }

    if ( glyph & SEG_D_BIT ) {
        lcd_segment_set_to_lcdmem(digit_segments.SEG_D);
    }

    if ( glyph & SEG_E_BIT ) {
        lcd_segment_set_to_lcdmem(digit_segments.SEG_E);
    }

    if ( glyph & SEG_F_BIT ) {
        lcd_segment_set_to_lcdmem(digit_segments.SEG_F);
    }

    if ( glyph & SEG_G_BIT ) {
        lcd_segment_set_to_lcdmem(digit_segments.SEG_G);
    }

}

void lcd_write_blank_to_lcdmem( byte digitplace ) {

    lcd_digit_segments_t digit_segments = lcd_digit_segments[ digitplace ];

    lcd_segment_clear_to_lcdmem(digit_segments.SEG_A);
    lcd_segment_clear_to_lcdmem(digit_segments.SEG_B);
    lcd_segment_clear_to_lcdmem(digit_segments.SEG_C);
    lcd_segment_clear_to_lcdmem(digit_segments.SEG_D);
    lcd_segment_clear_to_lcdmem(digit_segments.SEG_E);
    lcd_segment_clear_to_lcdmem(digit_segments.SEG_F);
    lcd_segment_clear_to_lcdmem(digit_segments.SEG_G);

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


constexpr bool test_all_digit_segments_contained_in_one_word( const lcd_digit_segments_t segements ) {

    return areAllEqual(

            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_A.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_B.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_C.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_D.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_E.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_F.lcd_pin ]  ),
            LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[ segments.SEG_G.lcd_pin ]  ),

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

constexpr word word_bits_for_segment( lcd_segment_location_t seg_location ) {

    byte lpin = lcdpin_to_lpin( seg_location.lcd_pin );

    byte com_bits = lcd_shifted_com_bits( lpin , seg_location.lcd_com );  // THis is the value we would put into an LCDMEM byte

    if ( LCDMEM_OFFSET_FOR_LPIN(seg_location.lcd_pin) & 1 ) {           // Is this lpin in the top byte of the LCDMEM word?

        return com_bits << 8;

    } else {

        return com_bits;
    }

}

// THis returns a word that you would OR into the appropriate LCDMEM word to turn on the segments in the specified digitplace to show the specified glyph

constexpr word glyph_bits( lcd_digit_segments_t digitplace_segs , glyph_segment_t glyph ) {

    static_assert( test_all_digit_segments_contained_in_one_word( segs ) , "All of the segments in this digit must be in the same word in LCDMEM for this optimization to work" );

    word bits = 0;          // Start with all off

    if ( glyph & SEG_A_BIT ) {
        bits |=   word_bits_for_segment( digit_segments.SEG_A);
    }

    if ( glyph & SEG_B_BIT ) {
        bits |=   word_bits_for_segment( digit_segments.SEG_B);
    }

    if ( glyph & SEG_C_BIT ) {
        bits |=   word_bits_for_segment( digit_segments.SEG_C);
    }

    if ( glyph & SEG_D_BIT ) {
        bits |=   word_bits_for_segment( digit_segments.SEG_D);
    }

    if ( glyph & SEG_E_BIT ) {
        bits |=   word_bits_for_segment( digit_segments.SEG_E);
    }

    if ( glyph & SEG_F_BIT ) {
        bits |=   word_bits_for_segment( digit_segments.SEG_F);
    }

    if ( glyph & SEG_G_BIT ) {
        bits |=   word_bits_for_segment( digit_segments.SEG_G);
    }

    return bits;

}

// these arrays hold the pre-computed words that we will write to word in LCD memory that
// contain the pairs of seconds and mins digits on the LCD. We keep these in RAM intentionally for power and latency savings.
// use fill_lcd_words() to fill these arrays.


template <

// Cross check to make sure the current LCD layout and connections are compatible with this optimization
static_assert( test_all_digit_segments_contained_in_one_word(  tens_digitplace_segments,  ones_digitplace_segments )   , "All of the segments in the seconds ones and tens digits must be in the same LCDMEM word for this optimization to work" );

#pragma RETAIN
word secs_lcd_words[SECS_PER_MIN];

// Write a value from the array into this word to update the two seconds digits on the LCD display

// This address is hardcoded into the ASM so we don't need the reference here.
word *secs_lcdmem_word = LCDMEMW +  ( LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[  lcd_digit_segments[ SECS_ONES_DIGITPLACE ].SEG_A.lcd_pin ] ) );

// Builds a RAM-based table of words where each word is the value you would assign to a single LCD word address to display a given 2-digit number
// Note that this only works for cases where all 4 of the LPINs for a pair of digits are on consecutive LPINs that use only 2 LCDM addresses.
// In our case, seconds, minutes, and hours meet this constraint (not by accident!) so we can do the *vast* majority of all updates efficiently with just a single instruction word assignment.


void fill_lcd_words( word *words , const byte tens_digitplace , const byte ones_digitplace , const byte max_tens_digit , const byte max_ones_digit ) {

    const lcd_digit_segments_t ones_digitplace_segments = lcd_digit_segments[ones_digitplace];
    const lcd_digit_segments_t tens_digitplace_segments = lcd_digit_segments[tens_digitplace];


    byte n = 0 ;

    for( byte tens_digit = 0; tens_digit < max_tens_digit ; tens_digit ++ ) {

        // Start with all of bits for the segments for the tens digit turned on
        word tens_digitplace_bits = segments_bits( tens_digitplace_segments , digit_glyphs[ tens_digit ] );

        for( byte ones_digit = 0; ones_digit < max_ones_digit ; ones_digit ++ ) {

            word ones_digitplace_bits = segments_bits( ones_digitplace_segments , digit_glyphs[ ones_digit ] );

            // Combines all the segments that need to be lit to show this two digit number
            words[n] = tens_digitplace_bits | ones_digitplace_bits;
            n++;
        }

    }

};


// Cross check to make sure the current LCD layout and connections are compatible with this optimization
static_assert( test_all_digit_segments_contained_in_one_word( MINS_ONES_DIGITPLACE,  MINS_TENS_DIGITPLACE)   , "All of the segments in the minutes ones and tens digits must be in the same LCDMEM word for this optimization to work" );


#pragma RETAIN
word mins_lcd_words[SECS_PER_MIN];

// Write a value from the array into this word to update the two digits on the LCD display
word *mins_lcdmem_word = LCDMEMW +  ( LCDMEMW_OFFSET_FOR_LPIN( lcdpin_to_lpin[  lcd_digit_segments[ SECS_TENS_DIGITPLACE ].SEG_A.lcd_pin ] ) );


#define MINS_PER_HOUR 60
#pragma RETAIN
word hours_lcd_words[SECS_PER_MIN];


// Define a full LCD frame so we can put it into LCD memory in one shot.
// Note that a frame only includes words of LCD memory that have actual pins used. This is hard coded here and in the
// RTL_MODE_ISR. There are a total of only 8 words spread across 2 extents. This is driven by the PCB layout.
// It is faster to copy a sequence of words than try to only set the nibbles that have changed.



// Fills the arrays

void initLCDPrecomputedWordArrays() {

    // Note that we need different arrays for the minutes and seconds because , while both have all four LCD pins in the same LCDMEM word,
    // they had to be connected in different orders just due to PCB routing constraints. Of course it would have been great to get them ordered the same
    // way and save some RAM (or even also get all 4 of the hours pin in the same LCDMEM word) but I think we are just lucky that we could get things router so that
    // these two updates are optimized since they account for the VAST majority of all time spent in the CPU active mode.

    // Fill the seconds array
    fill_lcd_words( secs_lcd_words , SECS_TENS_DIGITPLACE , SECS_ONES_DIGITPLACE , 6 , 10 );
    // Fill the minutes array
    fill_lcd_words( mins_lcd_words , MINS_TENS_DIGITPLACE , MINS_ONES_DIGITPLACE , 6 , 10 );
    // Fill the array of frames for ready-to-launch-mode animation
}





// Show the digit x at position p
// where p=0 is the rightmost digit

template <uint8_t pos, uint8_t x>                    // Use template to force everything to compile down to an individualized, optimized function for each pos+x combination
inline void lcd_show() {

    constexpr uint8_t nibble_a_thru_d =  digit_glyphs[x].nibble_a_thru_d;         // Look up which segments on the low pin we need to turn on to draw this digit
    constexpr uint8_t nibble_e_thru_g =  digit_glyphs[x].nibble_e_thru_g;         // Look up which segments on the low pin we need to turn on to draw this digit

    constexpr uint8_t lpin_a_thru_d = digitplace_lpins_table[pos].lpin_a_thru_d;     // Look up the L-pin for the low segment bits of this digit
    constexpr uint8_t lpin_e_thru_g = digitplace_lpins_table[pos].lpin_e_thru_g;     // Look up the L-pin for the high bits of this digit

    constexpr uint8_t lcdmem_offset_a_thru_d = (lpin_t< lpin_a_thru_d >::lcdmem_offset()); // Look up the memory address for the low segment bits
    constexpr uint8_t lcdmem_offset_e_thru_g = lpin_t< lpin_e_thru_g >::lcdmem_offset(); // Look up the memory address for the high segment bits

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

    if ( lcdmem_offset_a_thru_d == lcdmem_offset_e_thru_g  ) {

        // If the two L-pins for this digit are in the same memory location, we can update them both with a single byte write
        // Note that the whole process that gets us here is static at compile time, so the whole lcd_show() call will compile down
        // to just a single immediate byte write, which is very efficient.

        const uint8_t lpin_a_thru_d_nibble = lpin_t< lpin_a_thru_d >::nibble();

        if ( lpin_a_thru_d_nibble == nibble_t::LOWER  ) {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (uint8_t) ( nibble_e_thru_g << 4 ) | (nibble_a_thru_d);     // Combine the nibbles, write them to the mem address

        } else {

            // the A-D segments go into the lower nibble
            // the E-G segments go into the upper nibble

            lcd_mem_reg[lcdmem_offset_a_thru_d] = (uint8_t) ( nibble_a_thru_d << 4 ) | (nibble_e_thru_g);     // Combine the nibbles, write them to the mem address

        }

    } else {

        // The A-D segments are located on a pin that has a different address than the E-G segments, so we can to manually splice the nibbles into those two addresses

        // Write the a_thru_d nibble to the memory address is lives in

        const nibble_t lpin_a_thru_d_nibble_index = lpin_t< lpin_a_thru_d >::nibble();

        set_nibble( &lcd_mem_reg[lcdmem_offset_a_thru_d] , lpin_a_thru_d_nibble_index , nibble_a_thru_d );


        // Write the e_thru_f nibble to the memory address is lives in

        const nibble_t lpin_e_thru_g_nibble_index = lpin_t< lpin_e_thru_g >::nibble();

        set_nibble( &lcd_mem_reg[lcdmem_offset_e_thru_g] , lpin_e_thru_g_nibble_index , nibble_e_thru_g );

    }

}


// Show the glyph x at position p
// where p=0 is the rightmost digit


void lcd_show_f( const uint8_t pos, const glyph_segment_t segs ) {


    byte * const lcd_mem_reg = (uint8_t *) LCDMEM;

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


}

inline void lcd_show_fast_secs( uint8_t secs ) {

    *secs_lcdmem_word = secs_lcd_words[  secs ];

}

void lcd_show_digit_f( const uint8_t pos, const byte d ) {

    lcd_show_f( pos , digit_glyphs[ d ] );
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
