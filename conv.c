/******************************************************************************
 * @file            conv.c
 *****************************************************************************/
unsigned long conv_dec (char *str, long max) {

    unsigned long value = 0;
    char ch;
    
    while ((ch = *str)) {
    
        if (ch == ' ' || max == 0) {
            break;
        }
        
        value = ((value * 10) + (ch - '0'));
        
        str++;
        max--;
    
    }
    
    return value;

}
