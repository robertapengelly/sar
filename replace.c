/******************************************************************************
 * @file            replace.c
 *****************************************************************************/
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "ar.h"
#include    "lib.h"
#include    "report.h"

void replace (FILE *arfp, const char *fname) {

    long bytes, len, read;
    
    const char *name, *p;
    char *contents, temp[17];
    
    FILE *tfp;
    
    if (!(tfp = sar_tmpfile ())) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to create temp file");
        return;
    
    }
    
    if (fwrite ("!<arch>\n", 8, 1, tfp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed whilst writing ar header");
        
        sar_close (tfp);
        return;
    
    }
    
    name = fname;
    
    if ((p = strrchr (fname, '/'))) {
        name = (p + 1);
    }
    
    len = strlen (name);
    
    if (len > 16) {
        len = 16;
    }
    
    memcpy (temp, name, len);
    temp[len] = '\0';
    
    state->append = 1;
    
    for (;;) {
    
        struct ar_header hdr;
        
        if (fread (&hdr, sizeof (hdr), 1, arfp) != 1) {
        
            if (feof (arfp)) {
                break;
            }
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst reading '%s'", state->outfile);
            return;
        
        }
        
        bytes = conv_dec (hdr.size, 10);
        
        if (bytes % 2) {
            bytes++;
        }
        
        if (memcmp (hdr.name, "/", 1) == 0) {
        
            fseek (arfp, bytes, SEEK_CUR);
            continue;
        
        }
        
        if (memcmp (hdr.name, temp, len) == 0) {
        
            state->append = 0;
            append (tfp, fname);
            
            fseek (arfp, bytes, SEEK_CUR);
            continue;
        
        }
        
        if (fwrite (&hdr, sizeof (hdr), 1, tfp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed whilst writing header");
            
            sar_close (tfp);
            exit (EXIT_FAILURE);
        
        }
        
        contents = xmalloc (512);
        
        for (;;) {
        
            if (bytes == 0 || feof (arfp)) {
                break;
            } else if (bytes >= 512) {
                read = 512;;
            } else {
                read = bytes;
            }
            
            if (fread (contents, read, 1, arfp) != 1) {
            
                report_at (program_name, 0, REPORT_ERROR, "failed to read %ld bytes from %s", bytes, state->outfile);
                
                free (contents);
                exit (EXIT_FAILURE);
            
            }
            
            bytes -= read;
            
            if (fwrite (contents, read, 1, tfp) != 1) {
                
                report_at (program_name, 0, REPORT_ERROR, "failed to write temp file");
                
                free (contents);
                exit (EXIT_FAILURE);
            
            }
        
        }
        
        free (contents);
    
    }
    
    if (state->append) {
    
        state->append = 0;
        append (tfp, fname);
    
    }
    
    fclose (arfp);
    remove (state->outfile);
    
    if ((arfp = fopen (state->outfile, "w+b")) == NULL) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to open %s", state->outfile);
        
        sar_close (tfp);
        exit (EXIT_FAILURE);
    
    }
    
    contents = xmalloc (512);
    bytes = ftell (tfp);
    
    fseek (arfp, 0, SEEK_SET);
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
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst reading temp file");
            
            sar_close (tfp);
            free (contents);
            
            return;
        
        }
        
        bytes -= read;
        
        if (fwrite (contents, read, 1, arfp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed whilst writing %s", state->outfile);
            
            sar_close (tfp);
            free (contents);
            
            exit (EXIT_FAILURE);
        
        }
    
    }
    
    sar_close (tfp);
    free (contents);

}
