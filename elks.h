/******************************************************************************
 * @file            elks.h
 *****************************************************************************/
#ifndef     _ELKS_H
#define     _ELKS_H

struct elks_exec {

    unsigned char a_magic[2];
    unsigned char a_flags;
    unsigned char a_cpu;
    unsigned char a_hdrlen;
    unsigned char a_unused;
    unsigned char a_version[2];
    
    unsigned char a_text[4];
    unsigned char a_data[4];
    unsigned char a_bss[4];
    unsigned char a_entry[4];
    unsigned char a_total[4];
    unsigned char a_syms[4];
    
    unsigned char a_trsize[4];
    unsigned char a_drsize[4];
    unsigned char a_trbase[4];
    unsigned char a_drbase[4];

};

#define     ELKS_MAGIC                  0403

struct elks_nlist {

    unsigned char n_strx[4];
    unsigned char n_type;
    
    unsigned char n_other;
    unsigned char n_desc[2];
    
    unsigned char n_value[4];

};

#endif      /* _ELKS_H */
