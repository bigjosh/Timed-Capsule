// *** LCD font

// These are independent of the LCD and how it is connected (assuming all LCDs use the same A-F pattern?)
// We use what seems to be the standard segment naming convention that starts with A at the top and then goes clockwise around to F, and then G in the center.

// These are really a set, but use bitmaps to make them constexpr. Luckily works because there are less segments (7) than bits (8), but we could use longer numbers to get more bits
// TODO: This would be better to have a struct with each segment as a boolean indicating if it is on or off.

#define SEG_A_BIT 0b00000001
#define SEG_B_BIT 0b00000010
#define SEG_C_BIT 0b00000100
#define SEG_D_BIT 0b00001000
#define SEG_E_BIT 0b00010000
#define SEG_F_BIT 0b00100000
#define SEG_G_BIT 0b01000000


typedef byte glyph_segment_t;    // Each digit of the LCD maps into a single byte in LCDMEM.

constexpr glyph_segment_t glyph_0      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT                      }; // "0"
constexpr glyph_segment_t glyph_1      = {                SEG_B_BIT     | SEG_C_BIT                                                                      }; // "1" (no high pin segments lit in the number 1)
constexpr glyph_segment_t glyph_2      = {SEG_A_BIT     | SEG_B_BIT     |                 SEG_D_BIT     | SEG_E_BIT     |                 SEG_G_BIT      }; // "2"
constexpr glyph_segment_t glyph_3      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     |                                 SEG_G_BIT      }; // "3"
constexpr glyph_segment_t glyph_4      = {                SEG_B_BIT     | SEG_C_BIT                     |                 SEG_F_BIT     | SEG_G_BIT      }; // "4"
constexpr glyph_segment_t glyph_5      = {SEG_A_BIT     |                 SEG_C_BIT     | SEG_D_BIT     |                 SEG_F_BIT     | SEG_G_BIT      }; // "5"
constexpr glyph_segment_t glyph_6      = {SEG_A_BIT     |                 SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "6"
constexpr glyph_segment_t glyph_7      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT                                                                      }; // "7"(no high pin segments lit in the number 7)
constexpr glyph_segment_t glyph_8      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "8"
constexpr glyph_segment_t glyph_9      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     |                 SEG_F_BIT     | SEG_G_BIT      }; // "9"
constexpr glyph_segment_t glyph_A      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT                     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "A"
constexpr glyph_segment_t glyph_b      = {                                SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "b"
constexpr glyph_segment_t glyph_C      = {SEG_A_BIT     |                                 SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT                      }; // "C"
constexpr glyph_segment_t glyph_c      = {                                                SEG_D_BIT     | SEG_E_BIT                     | SEG_G_BIT      }; // "c"
constexpr glyph_segment_t glyph_d      = {                SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT     |                 SEG_G_BIT      }; // "d"
constexpr glyph_segment_t glyph_E      = {SEG_A_BIT     |                                 SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "E"
constexpr glyph_segment_t glyph_F      = {SEG_A_BIT                                                     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "F"
constexpr glyph_segment_t glyph_g      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     |                 SEG_F_BIT     | SEG_G_BIT      }; // g
constexpr glyph_segment_t glyph_H      = {                SEG_B_BIT     | SEG_C_BIT                     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "H"
constexpr glyph_segment_t glyph_h      = {                                SEG_C_BIT                     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "h"
constexpr glyph_segment_t glyph_I      = {                SEG_B_BIT     | SEG_C_BIT                                                                      }; // I
constexpr glyph_segment_t glyph_J      = {                SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT                                      }; // "J"
constexpr glyph_segment_t glyph_K      = {                SEG_B_BIT     | SEG_C_BIT                     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "K"
constexpr glyph_segment_t glyph_i      = {                                SEG_C_BIT                                                                      }; // i
constexpr glyph_segment_t glyph_L      = {                                                SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT                      }; // L
constexpr glyph_segment_t glyph_m1     = {                                SEG_C_BIT                     | SEG_E_BIT     |                 SEG_G_BIT      }; // left half of "m"
constexpr glyph_segment_t glyph_m2     = {                                SEG_C_BIT                     |                                 SEG_G_BIT      }; // right half of "m"
constexpr glyph_segment_t glyph_n      = {                                SEG_C_BIT                     | SEG_E_BIT     |                 SEG_G_BIT      }; // n
constexpr glyph_segment_t glyph_O      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT                      }; // O
constexpr glyph_segment_t glyph_o      = {                                SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT     |                 SEG_G_BIT      }; // "o"
constexpr glyph_segment_t glyph_P      = {SEG_A_BIT     | SEG_B_BIT                                     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "P"
constexpr glyph_segment_t glyph_r      = {                                                                SEG_E_BIT     |                 SEG_G_BIT      }; // "r"
constexpr glyph_segment_t glyph_S      = {SEG_A_BIT     |                 SEG_C_BIT     | SEG_D_BIT     |                 SEG_F_BIT     | SEG_G_BIT      }; // S
constexpr glyph_segment_t glyph_t      = {                                                SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // t
constexpr glyph_segment_t glyph_X      = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT     | SEG_G_BIT      }; // "X"
constexpr glyph_segment_t glyph_y      = {                SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     |                 SEG_F_BIT     | SEG_G_BIT      }; // y

constexpr glyph_segment_t glyph_SPACE  = {                                                                                                               0};// " "

constexpr glyph_segment_t glyph_dash   = {  SEG_G_BIT     };                                                                                                // "-"
constexpr glyph_segment_t glyph_rbrac  = {SEG_A_BIT     | SEG_B_BIT     | SEG_C_BIT     | SEG_D_BIT     };                                                  // "]"
constexpr glyph_segment_t glyph_lbrac  = {SEG_A_BIT     |                                 SEG_D_BIT     | SEG_E_BIT     | SEG_F_BIT                      }; // "["
constexpr glyph_segment_t glyph_topbot = {SEG_A_BIT     |                                 SEG_D_BIT                                                      }; // top and bottom segs lit


// All the single digit numbers (up to 0x0f hex) in an array for easy access

constexpr glyph_segment_t digit_glyphs[0x10] = {

    glyph_0, // "0"
    glyph_1, // "1"
    glyph_2, // "2"
    glyph_3, // "3"
    glyph_4, // "4"
    glyph_5, // "5"
    glyph_6, // "6"
    glyph_7, // "7"
    glyph_8, // "8"
    glyph_9, // "9"
    glyph_A, // "A"
    glyph_b, // "b"
    glyph_C, // "C"
    glyph_d, // "d"
    glyph_E, // "E"
    glyph_F, // "F"

};


// Squiggle segments are used during the Ready To Launch mode animation before the pin is pulled

constexpr unsigned SQUIGGLE_SEGMENTS_SIZE = 8;          // There really should be a better way to do this.

constexpr glyph_segment_t squiggle_segments[SQUIGGLE_SEGMENTS_SIZE] = {
    SEG_A_BIT,
    SEG_B_BIT,
    SEG_G_BIT,
    SEG_E_BIT,
    SEG_D_BIT,
    SEG_C_BIT,
    SEG_G_BIT,
    SEG_F_BIT,
};


// These are little jaggy lines going one way and the other way
constexpr glyph_segment_t left_tick_segments = {  SEG_C_BIT     | SEG_F_BIT     | SEG_G_BIT     };
constexpr glyph_segment_t right_tick_segments = { SEG_B_BIT     | SEG_E_BIT     | SEG_G_BIT     };

