

// *** This header defines the pinout of the 17-pin 6 digit LCD we are using. 
// These values come from LCD datasheet pinout.


// This struct should probably be in its own header?
struct lcd_segment_location_t {
    byte lcd_pin;
    byte lcd_com;
};

struct lcd_digit_segments_t {
    lcd_segment_location_t SEG_A;
    lcd_segment_location_t SEG_B;
    lcd_segment_location_t SEG_C;
    lcd_segment_location_t SEG_D;
    lcd_segment_location_t SEG_E;
    lcd_segment_location_t SEG_F;
    lcd_segment_location_t SEG_G;
};

// Location of each segment of digit on the LCD
// Note that on the LCD pinout, COM index starts at 1

constexpr lcd_digit_segments_t lcd_digit_segments[] = {
    {{ 3, 1}, { 3, 2}, { 3, 3}, { 2, 4}, { 2, 3}, { 2, 1}, { 2, 2}},                          // Digitplace 0 (leftmost digit)
    {{ 5, 1}, { 5, 2}, { 5, 3}, { 4, 4}, { 4, 3}, { 4, 1}, { 4, 2}},
    {{ 7, 1}, { 7, 2}, { 7, 3}, { 6, 4}, { 6, 3}, { 6, 1}, { 6, 2}},
    {{ 9, 1}, { 9, 2}, { 9, 3}, { 8, 4}, { 8, 3}, { 8, 1}, { 8, 2}},
    {{11, 1}, {11, 2}, {11, 3}, {10, 4}, {10, 3}, {10, 1}, {10, 2}},
    {{13, 1}, {13, 2}, {13, 3}, {12, 4}, {12, 3}, {12, 1}, {12, 2}},                          // Digitplace 5 (rightmost digit)

};

// These are some special non-numeric segments on this LCD

constexpr lcd_segment_location_t lcd_segment_dot1 = { 9 , 4 };       // decimal point between mins and secs
constexpr lcd_segment_location_t lcd_segment_col1 = { 5 , 4 };       // Colon between hours and mins

constexpr lcd_segment_location_t lcd_segment_batt_outline =  { 1 , 1 };

constexpr lcd_segment_location_t lcd_segment_batt_level[] = {                 // The battery outline, followed
    { 1 , 4 },                                              // Bottom
    { 1 , 3 },                                              // Middle
    { 1 , 2 },                                              // Top
};

// Logical location of each digit as an index into lcd_digit_segments

#define SECS_ONES_DIGITPLACE  ( 5)
#define SECS_TENS_DIGITPLACE  ( 4)
#define MINS_ONES_DIGITPLACE  ( 3)
#define MINS_TENS_DIGITPLACE  ( 2)
#define HOURS_ONES_DIGITPLACE ( 1)
#define HOURS_TENS_DIGITPLACE ( 0)

constexpr uint8_t DIGITPLACE_COUNT=6;

// Normalize the positions so index 0 is rightmost
constexpr byte digit_positions_rj( byte pos ) {

    return 5-pos;
}

// What LCD pin each LCD COM is on. These indexes match to the com values in the table above.
// This is really a map but we make it into an array so it can be constexpr.

constexpr byte lcd_com_to_lcd_pin[] {
     0,                             // placeholder (The LCD COM index starts at 1, so there is no LCD COM0)
    17,                             // COM1
    16,                             // COM2
    15,                             // COM3
    14,                             // COM4
};

