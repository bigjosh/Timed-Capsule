// *** Define the MSP430 LCD E device layout
// This information comes from the MSP430FR4 users manual SLAU445I page 445.

// Map LPIN number to the memory word you would write to for the lower byte of that LPIN.
// This just happens to be a simple mapping for this chip with two LPINs per LCDMEM
// This is a 4 mux LCD, so each of the lower 4 bits in the LCDMEM represents one COM segment on the even (lower) LPIN, and the higher 4 bits are on the odd (upper) LPIN.
// Add this offset to LCDMEM to get address to write to

constexpr word LCDMEM_OFFSET_FOR_LPIN( byte lpin) {
    return lpin>>1;
}

// This is a 16 bit processor so we can efficiently access each pair of LCDMEM locations as a word.
// We designed the PCB so that each word maps to a 2-digit number of hours, mins, or secs so each of these can be updated with a single write.
// (Round down the next even address)
// Add this offset to LCDMEMW to get address to write to

constexpr word LCDMEMW_OFFSET_FOR_LPIN( byte lpin) {
    return LCDMEM_OFFSET_FOR_LPIN(lpin) >> 1 ;
}

// In our 4-mux configuration, each LCDMEM address can hold two LPINs (4 bits in the top nibble, 4 bits in the bottom nibble)
// This function returns a value that you can OR into the LCDMEM location to set the COM bit for the requested lcd com.
// This mapping comes from page 445 of the MSP430FR4 family users guide

constexpr byte lcd_shifted_com_bits( byte lpin , byte lcd_com ) {

    byte com_bitmask = 1 << (lcd_com-1);            // The lcd_com values are 1-4. The MSP430 uses a bit in position 0-4 for even LPINs and a bit in position 5-7 for odd LPINs.

    if (lpin & 1 ) {

        return com_bitmask << 4;     // Odd lpins are in the upper nibble

    } else {

        return com_bitmask;

    }
}


constexpr word * LCDMEMW = (word *) LCDMEM;                // Give us a way to address LCD memory word-wise rather than byte wise.
constexpr byte LCDMEM_WORD_COUNT=16;             // Total number of words in LCD mem. Note that we do not actually use them all, but our display is spread across it.
