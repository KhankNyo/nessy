#ifndef MC6502_H
#define MC6502_H


#include "Common.h"

typedef struct MC6502 MC6502;
struct MC6502 
{
    u8 A, X, Y;
    u8 SP;
    u8 Flags;
    u16 PC;
};


#endif /* MC6502_H */




#ifdef MC6502_IMPLEMENTATION



#  ifdef STANDALONE
int main(void)
{
    return 0;
}
#  endif /* STANDALONE */
#endif /* MC6502_IMPLEMENTATION */

