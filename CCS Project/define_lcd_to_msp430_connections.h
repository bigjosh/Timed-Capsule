// ** This header defines the connections between the LCD and the MSP430
// These values come from our schematic. Note that this table includes the COM pins, and we will use these to config the MSP430


// Now we define the connections from the LCD to the MSP430 LPINs
// This is really just a map, but we do it as an array so it can be constexpr

constexpr byte lcdpin_to_lpin[] = {

     0,                     // placeholder (there is no LCD pin 0)
    36,                     // LCD pin 1
    35,
    34,
    33,
    32,
    31,
    30,
    29,
    28,
    15,
    14,
    13,
    12,
    11,
    10,
     9,
     8,                     // LCD pin 17
};


