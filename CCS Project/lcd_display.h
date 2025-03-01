/*
 * lcd_display.h
 *
 *  Created on: Jan 20, 2023
 *      Author: passp
 */

#ifndef LCD_DISPLAY_H_
#define LCD_DISPLAY_H_

#include "util.h"


// *** LCD layout



// these arrays hold the pre-computed words that we will write to word in LCD memory that
// controls the seconds and mins digits on the LCD. We keep these in RAM intentionally for power and latency savings.
// use fill_lcd_words() to fill these arrays.

#define SECS_PER_MIN 60

#define MINS_PER_HOUR 60


const byte READY_TO_LAUNCH_LCD_FRAME_COUNT=8;                             // How many frames in the ready-to-launch mode animation


void lcd_segment_set( char * lcdmem_base , lcd_segment_location_t seg  );

void lcd_segment_set_to_lcdmem( lcd_segment_location_t seg  );

void lcd_segment_clear_to_lcdmem( lcd_segment_location_t seg  );

// General function to write a glyph onto the LCD. No constraints on how pins are connected, but inefficient.
// Remember that means that different segments in the same digit could be at different LCDMEM locations!

void lcd_write_glyph_to_lcdmem( byte digitplace, glyph_segment_t glyph );

// Write a glyph to the blinking LCD buffer
void lcd_write_glyph_to_lcdbm( byte digitplace , glyph_segment_t glyph );


void lcd_write_blank_to_lcdmem( byte digitplace );

void lcd_write_blank_to_lcdbm( byte digitplace );


// Show the digit x at position p
// where p=0 is the rightmost digit

template <uint8_t pos, uint8_t x>                    // Use template to force everything to compile down to an individualized, optimized function for each pos+x combination
inline void lcd_show();
// Show the digit x at position p
// where p=0 is the rightmost digit


void lcd_show_fast_secs( uint8_t secs );

// p=0 is the rightmost digit

void lcd_show_f( char *lcdmem_base ,  const uint8_t pos, const glyph_segment_t segs );

void lcd_show_digit_f( const uint8_t pos, const byte d );
void lcd_show_digit_f( char *lcd_base , const uint8_t pos, const byte d );

void lcd_show_testing_only_message();

// For now, show all 9's.
// TODO: Figure out something better here

void lcd_show_long_now();


// Squigle segments are used during the Ready To Launch mode animation before the pin is pulled
constexpr unsigned SQUIGGLE_ANIMATION_FRAME_COUNT = 8;          // There really should be a better way to do this.

// Show a frame n the ready-to-lanuch squiggle animation
// 0 <= step < SQUIGGLE_ANIMATION_FRAME_COUNT

void lcd_show_squiggle_frame( byte step );


// Fill the screen with horizontal dashes
void lcd_show_dashes();

// Fill the screen with X's
void lcd_show_XXX();

void lcd_show_load_pin_message();

constexpr unsigned LOAD_PIN_ANIMATION_FRAME_COUNT = 5;      // Dash sliding to the right to point ot the trigger pin
void lcd_show_load_pin_animation(unsigned int step);

// The lance is 2 digitplaces wide so we need extra frames to show it coming onto and off of the screen
constexpr unsigned LANCE_ANIMATION_FRAME_COUNT = DIGITPLACE_COUNT+3;          // There really should be a better way to do this.

// Show "First Start"
void lcd_show_start_message();

// Show the message "Error cOde X" on the lcd.
void lcd_show_errorcode( byte code  );


// Every 100 days
void lcd_show_centesimus_dies_message();

// Fill the screen with 0's
void lcd_show_zeros();

// Do a hardware clear of the LCD memory
// Note this does block until the hardware indicates that the clear is complete.
// TODO: Check how long this takes.

void lcd_cls_LCDMEM();

void lcd_cls_LCDMEM_nowait();       // Do it blindly, start the process but do not wait for it to complete.



// This bit supports flashing LCD applications by turning off all segment lines, while leaving the LCD timing generator and R33 enabled.

void lcd_on();

void lcd_off();



// Init the "d" in the days display in the secondary LCD buffer
void lcd_show_day_label_lcdbmem();


// Print the current days value into the secondary LCD buffer
// Currently leading spaces, but could be leading 0s

void lcd_show_days_lcdbmem( const unsigned days );


// Init the LCD. Clears memory.
void initLCD();


// Blank all LCD segments in hardware
void lcd_off();
// Unblank all LCD segments in hardware
void lcd_on();


// Show the main mem bank on LCD
void lcd_show_LCDMEM_bank();

// Show the secondary (LCDBMEM) bank on the lcd
void lcd_show_LCDBMEM_bank();


// Put the LCD into no blinking mode
void lcd_blinking_mode_none();

// Put the LCD into blinking mode where any segment in LCDBMEM blinks
void lcd_blinking_mode_segments();

// Put the LCD into blinking mode where all segements blink
void lcd_blinking_mode_all();

// Put the LCD into double page buffer mode where you can switch beteween LCDMEM and LCDBMEM
void lcd_blinking_mode_doublebuffer();

// Show " OPEn"
void lcd_show_open_message();

// Show "Hold  "
void lcd_show_hold_message();



#endif /* LCD_DISPLAY_H_ */
