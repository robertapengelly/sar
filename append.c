/******************************************************************************
 * @file            append.c
 *****************************************************************************/
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "ar.h"
#include    "lib.h"
#include    "report.h"

void append (FILE *ofp, const char *fname) {

    FILE *tfp;
    
    struct ar_header header;
    char temp[17];
    
    const char *name = fname;
    char *p, *contents;
    
    int need_newline = 0;
    long bytes, len, read;
    
    if ((tfp = fopen (fname, "r+b")) == NULL) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to open %s", fname);
        return;
    
    }
    
    memset (temp, ' ', 16);
    temp[0] = '0';
    
    if ((p = strrchr (fname, '/'))) {
        name = (p + 1);
    }
    
    len = strlen (name);
    
    if (len > 16) {
        len = 16;
    }
    
    memcpy (header.name, name, len);
    
    while (len < 16) {
        header.name[len++] = ' ';
    }
    
    memcpy (header.mtime, temp, 12);
    memcpy (header.owner, temp, 6);
    memcpy (header.group, temp, 6);
    memcpy (header.mode, temp, 8);
    
    fseek (tfp, 0, SEEK_END);
    bytes = ftell (tfp);
    
    len = sprintf (temp, "%ld", bytes);
    temp[len] = ' ';
    
    memcpy (header.size, temp, 10);
    
    header.endsig[0] = '`';
    header.endsig[1] = '\n';
    
    need_newline = (bytes % 2);
    
    if (fwrite (&header, sizeof (header), 1, ofp) != 1) {
    
        fclose (tfp);
        
        report_at (program_name, 0, REPORT_ERROR, "failed whilst writing header");
        return;
    
    }
    
    contents = xmalloc (512);
    fseek (tfp, 0, SEEK_SET);
    
    for (;;) {
    
        if (bytes == 0 || feof (tfp)) {
            break;
        } else if (bytes >= 512) {
            read = 512;
        } else {
            read = bytes;
        }
        
        if (fread (contents, read, 1, tfp) != 1) {
            
            free (contents);
            fclose (tfp);
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst reading %s", fname);
            return;
        
        }
        
        bytes -= read;
        
        if (fwrite (contents, read, 1, ofp) != 1) {
        
            free (contents);
            fclose (tfp);
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst writing %s to archive", fname);
            return;
        
        }
    
    }
    
    free (contents);
    
    if (need_newline) {
    
        temp[0] = '\n';
        
        if (fwrite (temp, 1, 1, ofp) != 1) {
        
            fclose (tfp);
            exit (EXIT_FAILURE);
        
        }
    
    }
    
    fclose (tfp);

}
