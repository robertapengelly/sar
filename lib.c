/******************************************************************************
 * @file            lib.c
 *****************************************************************************/
#if     defined (unix) || defined (__unix) || defined (__unix__) || defined (__APPLE__)
# ifndef    _POSIX_C_SOURCE
#  define       _POSIX_C_SOURCE         200809L
# endif
#endif

#include    <limits.h>
#include    <stdint.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "ar.h"
#include    "lib.h"
#include    "report.h"

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
# include       <unistd.h>
# include       "hashtab.h"

struct temp_file {

    char *name, *path;
    FILE *fp;

};

static struct hashtab hashtab_tmpfiles = { 0 };

FILE *sar_tmpfile (void) {

    char *temp_path, *template, *name, *p;
    
    struct hashtab_name *key;
    struct temp_file *tmp;
    
    FILE *fp;
    int fd;
    
    if (!(temp_path = getenv ("TMPDIR"))) {
        temp_path = "/tmp";
    }
    
    template = xmalloc (strlen (temp_path) + 1 + 10 + 1);
    
again:
    
    sprintf (template, "%s/sarXXXXXX", temp_path);
    
    if ((fd = mkstemp (template)) < 0) {
        return 0;
    }
    
    if ((p = strrchr (template, '/'))) {
        name = xstrdup (p + 1);
    } else {
        name = xstrdup (template);
    }
    
    if ((key = hashtab_get_key (&hashtab_tmpfiles, name))) {
    
        close (fd);
        
        remove (template);
        free (name);
        
        goto again;
    
    }
    
    if (!((key = hashtab_alloc_name (name)))) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory for key");
        exit (EXIT_FAILURE);
    
    }
    
    if ((fp = fdopen (fd, "w+b"))) {
    
        tmp = xmalloc (sizeof (*tmp));
        tmp->fp = fp;
        
        tmp->path = xstrdup (template);
        tmp->name = name;
        
        hashtab_put (&hashtab_tmpfiles, key, tmp);
        
        free (template);
        return fp;
    
    }
    
    free (name);
    free (key);
    
    close (fd);
    
    remove (template);
    free (template);
    
    return 0;

}

int sar_close (FILE *fp) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if (tmp->fp == fp) {
        
            if ((ret = fclose (fp))) {
                return ret;
            }
            
            hashtab_remove (&hashtab_tmpfiles, entry->key);
            remove (tmp->path);
            
            free (tmp->path);
            free (tmp->name);
            
            free (tmp);
            return 0;
        
        }
    
    }
    
    return 0;

}

void cleanup_tempfiles (void) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if ((ret = fclose (tmp->fp))) {
            continue;
        }
        
        remove (tmp->path);
    
    }

}
#elif       defined (_WIN32)
# include       <windows.h>

# ifndef    MAX_PATH
#  define       MAX_PATH                206
# endif

# include       "hashtab.h"

struct temp_file {

    char *name, *path;
    FILE *fp;

};

static struct hashtab hashtab_tmpfiles = { 0 };

FILE *sar_tmpfile (void) {

    char temp_path[MAX_PATH + 1] = { 0 };
    char full_path[MAX_PATH + 1] = { 0 };
    
    struct hashtab_name *key;
    struct temp_file *tmp;
    
    char *name, *p;
    FILE *fp;
    
    if (!GetTempPath (MAX_PATH + 1, temp_path)) {
        return 0;
    }
    
again:
    
    if (!GetTempFileName (temp_path, "sar", 0, full_path)) {
        return 0;
    }
    
    if ((p = strrchr (full_path, '\\'))) {
        name = xstrdup (p + 1);
    } else {
        name = xstrdup (full_path);
    }
    
    if ((key = hashtab_get_key (&hashtab_tmpfiles, name))) {
    
        free (name);
        goto again;
    
    }
    
    if (!((key = hashtab_alloc_name (name)))) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory for key");
        exit (EXIT_FAILURE);
    
    }
    
    if ((fp = fopen (full_path, "w+b"))) {
    
        tmp = xmalloc (sizeof (*tmp));
        tmp->fp = fp;
        
        tmp->path = xstrdup (full_path);
        tmp->name = name;
        
        hashtab_put (&hashtab_tmpfiles, key, tmp);
        return fp;
    
    }
    
    free (name);
    free (key);
    
    return 0;

}

int sar_close (FILE *fp) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if (tmp->fp == fp) {
        
            if ((ret = fclose (fp))) {
                return ret;
            }
            
            hashtab_remove (&hashtab_tmpfiles, entry->key);
            remove (tmp->path);
            
            free (tmp->path);
            free (tmp->name);
            
            free (tmp);
            return 0;
        
        }
    
    }
    
    return 0;

}

void cleanup_tempfiles (void) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if ((ret = fclose (tmp->fp))) {
            continue;
        }
        
        remove (tmp->path);
    
    }

}
#else
# include       "hashtab.h"

struct temp_file {

    char *name;
    FILE *fp;

};

FILE *sar_tmpfile (void) {

    static char name[64] = { 0 };
    
    char *temp;
    unsigned long index, i, j;
    
    struct hashtab_name *key;
    struct temp_file *tmp;
    
    FILE *fp;
    
    for (i = 0; i < 456976; i++) {
    
        sprintf (name, "sar_AAAA.tmp");
        
        index = 7;
        j = i;
        
        do {
        
            name[index] = (j % 26) + 'A';
            index--;
        
        } while ((j /= 26) > 0);
        
        temp = xstrdup (name);
        
        if ((key = hashtab_get_key (&hashtab_tmpfiles, temp))) {
        
            free (temp);
            continue;
        
        }
        
        if ((fp = fopen (temp, "rb"))) {
        
            fclose (fp);
            
            free (temp);
            continue;
        
        }
        
        if (!((key = hashtab_alloc_name (temp)))) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory for key");
            exit (EXIT_FAILURE);
        
        }
        
        if ((fp = fopen (temp, "w+b"))) {
            
            tmp = xmalloc (sizeof (*tmp));
            
            tmp->name = temp;
            tmp->fp = fp;
        
            hashtab_put (&hashtab_tmpfiles, key, tmp);
            return fp;
        
        }
        
        free (temp);
        free (key);
    
    }
    
    return 0;

}

int sar_close (FILE *fp) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if (tmp->fp == fp) {
        
            if ((ret = fclose (fp))) {
                return ret;
            }
            
            hashtab_remove (&hashtab_tmpfiles, entry->key);
            remove (tmp->name);
            
            free (tmp->name);
            free (tmp);
            
            return 0;
        
        }
    
    }
    
    return 0;

}

void cleanup_tempfiles (void) {

    struct hashtab_entry *entry;
    struct temp_file *tmp;
    
    int ret, i;
    
    for (i = 0; i < hashtab_tmpfiles.capacity; i++) {
    
        if (!(entry = &hashtab_tmpfiles.entries[i])) {
            continue;
        }
        
        if (!entry->key || !entry->value || entry->value == HASHTAB_TOMBSTONE) {
            continue;
        }
        
        tmp = (struct temp_file *) entry->value;
        
        if ((ret = fclose (tmp->fp))) {
            continue;
        }
        
        remove (tmp->name);
    
    }

}
#endif
