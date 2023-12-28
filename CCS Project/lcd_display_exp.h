/*
 * lcd_display_exp.h
 *
 * exports from lcd_display.cpp to be used by tsl_asm.asm
 */

#ifndef LCD_DISPLAY_EXP_H_
#define LCD_DISPLAY_EXP_H_

// This is an array of 100 words corresponding to the 100 two digit numbers.
extern const unsigned int * const secs_lcd_words;
// Write a value from that array into this word to update the two digits on the LCD display
extern unsigned int *secs_lcdmemw;


#endif /* LCD_DISPLAY_EXP_H_ */
