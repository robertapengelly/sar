/******************************************************************************
 * @file            lib.c
 *****************************************************************************/
#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)
# ifndef    _POSIX_C_SOURCE
#  define       _POSIX_C_SOURCE         200809L
# endif
#endif

#include    <limits.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "ar.h"
#include    "lib.h"
#include    "report.h"
#include    "stdint.h"

static void print_usage (void) {

    if (program_name) {
    
        fprintf (stderr, "Usage: %s command archive-file file...\n\n", program_name);
        fprintf (stderr, "Commands:\n\n");
        
        fprintf (stderr, "    d             - delete file(s) from the archive\n");
        fprintf (stderr, "    q             - quick append file(s) to the archive\n");
        fprintf (stderr, "    r             - replace existing or insert new file(s) into the archive\n");
        fprintf (stderr, "    s             - act as ranlib\n");
        fprintf (stderr, "    t             - display contents of the archive\n");
        fprintf (stderr, "    x             - extract file(s) from the archive\n");
        
        fprintf (stderr, "\n");
    
    }

}

static void dynarray_add (void *ptab, long *nb_ptr, void *data) {

    long nb, nb_alloc;
    void **pp;
    
    nb = *nb_ptr;
    pp = *(void ***) ptab;
    
    if ((nb & (nb - 1)) == 0) {
    
        if (!nb) {
            nb_alloc = 1;
        } else {
            nb_alloc = nb * 2;
        }
        
        pp = xrealloc (pp, nb_alloc * sizeof (void *));
        *(void ***) ptab = pp;
    
    }
    
    pp[nb++] = data;
    *nb_ptr = nb;

}

char *xstrdup (const char *__p) {

    char *p = xmalloc (strlen (__p) + 1);
    
    strcpy (p, __p);
    return p;

}

char *xstrndup (const char *__p, unsigned long __size) {

    char *p = xmalloc (__size + 1);
    
    memcpy (p, __p, __size);
    return p;

}

uint64_t array_to_integer (unsigned char *arr, int size, int bigendian) {

    uint64_t value = 0;
    int i;
    
    if (bigendian) {
    
        int j;
        
        for (i = size, j = 0; i > 0; i--, j++) {
            value |= (uint64_t) arr[j] << (CHAR_BIT * (i - 1));
        }
    
    } else {
    
        for (i = 0; i < size; i++) {
            value |= arr[i] << (CHAR_BIT * i);
        }
    
    }
    
    return value;

}

void parse_args (int argc, char **argv, int optind) {

    const char *r;
    int opt;
    
    if (argc <= optind) {
    
        print_usage ();
        exit (EXIT_SUCCESS);
    
    }
    
    r = argv[optind++];
    
check_options:
    
    if (r[0] == '-') {
        ++r;
    }
    
    while (*r != '\0') {
    
        char ch = *r++;
        
        if (ch == 'd') {
        
            state->del++;
            continue;
        
        }
        
        if (ch == 'm') {
        
            state->move++;
            continue;
        
        }
        
        if (ch == 'p') {
        
            state->print++;
            continue;
        
        }
        
        if (ch == 'q') {
        
            state->append++;
            continue;
        
        }
        
        if (ch == 'r') {
        
            state->replace++;
            continue;
        
        }
        
        if (ch == 's') {
        
            state->ranlib++;
            continue;
        
        }
        
        if (ch == 't') {
        
            state->display++;
            continue;
        
        }
        
        if (ch == 'x') {
        
            state->extract++;
            continue;
        
        }
        
        report_at (program_name, 0, REPORT_ERROR, "invalid option -- '%c'", ch);
        
        print_usage ();
        exit (EXIT_SUCCESS);
    
    }
    
    if ((r = argv[optind++])) {
    
        if (*r == '-') {
            goto check_options;
        }
    
    }
    
    if ((opt = state->append + state->del + state->display + state->extract + state->move + state->print + state->ranlib + state->replace) != 1) {
    
        if (opt > 1) {
            report_at (program_name, 0, REPORT_ERROR, "more than one option provided");
        }
        
        print_usage ();
        exit (EXIT_SUCCESS);
    
    }
    
    if (!r) {
    
        print_usage ();
        exit (EXIT_SUCCESS);
    
    }
    
    state->outfile = xstrdup (r);
    
    if (state->display || state->ranlib) {
        return;
    }
    
    if (optind >= argc) {
    
        print_usage ();
        exit (EXIT_SUCCESS);
    
    }
    
    while (optind < argc) {
        dynarray_add (&state->files, &state->nb_files, xstrdup (argv[optind++]));
    }

}

void *xmalloc (unsigned long __size) {

    void *ptr = malloc (__size);
    
    if (!ptr && __size) {
    
        report_at (program_name, 0, REPORT_ERROR, "memory full (malloc)");
        exit (EXIT_FAILURE);
    
    }
    
    memset (ptr, 0, __size);
    return ptr;

}

void *xrealloc (void *__ptr, unsigned long __size) {

    void *ptr = realloc (__ptr, __size);
    
    if (!ptr && __size) {
    
        report_at (program_name, 0, REPORT_ERROR, "memory full (realloc)");
        exit (EXIT_FAILURE);
    
    }
    
    return ptr;

}


#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)
FILE *get_temp_file (void) {

    char *temp_path = "/tmp", *template;
    int fd;
    
    template = xmalloc (strlen (temp_path) + 1 + 10 + 1);
    sprintf (template, "%s/sarXXXXXX", temp_path);
    
    if (!(fd = mkstemp (template))) {
        return 0;
    }
    
    return fdopen (fd, "w+b");

}
#elif   defined (_WIN32)
# include   <windows.h>

# ifndef    MAX_PATH
#  define       MAX_PATH                206
# endif

FILE *get_temp_file (void) {

    char temp_path[MAX_PATH + 1] = { 0 };
    char full_path[MAX_PATH + 1] = { 0 };
    
    if (!GetTempPath (MAX_PATH + 1, temp_path)) {
        return 0;
    }
    
    if (!GetTempFileName (temp_path, "sar", 0, full_path)) {
        return 0;
    }
    
    return fopen (full_path, "w+b");

}
#else
FILE *get_temp_file (void) {
    return tmpfile ();
}
#endif
