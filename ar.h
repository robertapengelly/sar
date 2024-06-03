/******************************************************************************
 * @file            ar.h
 *****************************************************************************/
#ifndef     _AR_H
#define     _AR_H

struct ar_state {

    const char **files;
    long nb_files;
    
    const char *outfile;
    int append, del, display, extract, move, print, replace, ranlib;

};

extern struct ar_state *state;
extern const char *program_name;

struct ar_header {

    char name[16];
    char mtime[12];
    char owner[6];
    char group[6];
    char mode[8];
    char size[10];
    char endsig[2];

};

#include    <stdio.h>
unsigned long conv_dec (char *str, long max);

void append  (FILE *ofp, const char *fname);
void delete  (FILE *arfp, const char *fname);
void display (FILE *arfp);
void extract (FILE *arfp, const char *fname);
void ranlib  (FILE *arfp);
void replace (FILE *arfp, const char *fname);

#endif      /* _AR_H */
