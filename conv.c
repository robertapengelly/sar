/******************************************************************************
 * @file            conv.c
 *****************************************************************************/
#include    "stdint.h"

unsigned long conv_dec (char *str, long max) {

    unsigned long value = 0;
    
    while (*str != ' ' && max-- > 0) {
    
        value *= 10;
        value += *str++ - '0';
    
    }
    
    return value;

}
