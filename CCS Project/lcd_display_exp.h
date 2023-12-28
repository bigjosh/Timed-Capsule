/*
 * lcd_display_exp.h
 *
 * exports from lcd_display.cpp to be used by tsl_asm.asm
 */

#ifndef LCD_DISPLAY_EXP_H_
#define LCD_DISPLAY_EXP_H_

// These are each an array of 100 words. Each word is what you write to the matching XXX_lcdmemw address below to display corresponding two digit number on the LCD.
extern const unsigned int * const secs_lcd_words;
extern const unsigned int * const mins_lcd_words;
extern const unsigned int * const hours_lcd_words;

// Write a value from that array into this word to update the two digits on the LCD display
extern unsigned int *secs_lcdmemw;
extern unsigned int *mins_lcdmemw;
extern unsigned int *hours_lcdmemw;

#endif /* LCD_DISPLAY_EXP_H_ */
