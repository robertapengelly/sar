/******************************************************************************
 * @file            ranlib.c
 *****************************************************************************/
#include    <limits.h>
#include    <stddef.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "aout.h"
#include    "ar.h"
#include    "coff.h"
#include    "elf.h"
#include    "elks.h"
#include    "lib.h"
#include    "report.h"

#define     GET_INT32(arr)              ((long) arr[0] | (((long) arr[1]) << 8) | (((long) arr[2]) << 16) | (((long) arr[3]) << 24))
#define     GET_UINT32(arr)             ((unsigned long) arr[0] | (((unsigned long) arr[1]) << 8) | (((unsigned long) arr[2]) << 16) | (((unsigned long) arr[3]) << 24))

#define     GET_UINT16(arr)             ((unsigned long) (arr)[0] | (((unsigned long) (arr)[1]) << 8))

struct strtab {

    const char *name;
    long length, offset;

};

struct gstrtab {

    long count, max;
    struct strtab *strtabs;

};

static struct gstrtab gstrtab = { 0, 64, NULL };

static int add_strtab (struct gstrtab *gstrtab, struct strtab *strtab) {

    if (gstrtab->strtabs == NULL) {
    
        if ((gstrtab->strtabs = malloc (gstrtab->max * sizeof (*strtab))) == NULL) {
            return 1;
        }
    
    }
    
    if (gstrtab->count >= gstrtab->max) {
    
        void *tmp;
        
        gstrtab->max *= 2;
        
        if ((tmp = realloc (gstrtab->strtabs, gstrtab->max * sizeof (*strtab))) == NULL) {
            return 1;
        }
        
        gstrtab->strtabs = tmp;
    
    }
    
    gstrtab->strtabs[gstrtab->count] = *strtab;
    gstrtab->count++;
    
    return 0;

}

static void aout_get_symbols (void *object, long offset) {

    struct aout_exec *hdr = (struct aout_exec *) object;
    
    long sym_start = sizeof (*hdr) + GET_UINT32 (hdr->a_text) + GET_UINT32 (hdr->a_data) + GET_UINT32 (hdr->a_trsize) + GET_UINT32 (hdr->a_drsize);
    long strtab_start = sym_start + GET_UINT32 (hdr->a_syms);
    
    struct strtab *strtab;
    struct aout_nlist nlist;
    
    while (sym_start < strtab_start) {
    
        memcpy (&nlist, (char *) object + sym_start, sizeof (nlist));
        
        if (nlist.n_type == 5 || nlist.n_type == 7 || nlist.n_type == 9) {
        
            char *symname = (char *) object + strtab_start + GET_INT32 (nlist.n_strx);
            
            strtab = xmalloc (sizeof (*strtab));
            strtab->length = strlen (symname);
            
            strtab->name = xstrdup (symname);
            strtab->offset = offset;
            
            add_strtab (&gstrtab, strtab);
        
        }
        
        sym_start += sizeof (nlist);
    
    }

}

static void elks_get_symbols (void *object, long offset) {

    struct elks_exec *hdr = (struct elks_exec *) object;
    
    long sym_start = sizeof (*hdr) + GET_UINT32 (hdr->a_text) + GET_UINT32 (hdr->a_data) + GET_UINT32 (hdr->a_trsize) + GET_UINT32 (hdr->a_drsize);
    long strtab_start = sym_start + GET_UINT32 (hdr->a_syms);
    
    struct strtab *strtab;
    struct elks_nlist nlist;
    
    while (sym_start < strtab_start) {
    
        memcpy (&nlist, (char *) object + sym_start, sizeof (nlist));
        
        if (nlist.n_type == 5 || nlist.n_type == 7 || nlist.n_type == 9) {
        
            char *symname = (char *) object + strtab_start + GET_INT32 (nlist.n_strx);
            
            strtab = xmalloc (sizeof (*strtab));
            strtab->length = strlen (symname);
            
            strtab->name = xstrdup (symname);
            strtab->offset = offset;
            
            add_strtab (&gstrtab, strtab);
        
        }
        
        sym_start += sizeof (nlist);
    
    }

}

static void coff_get_symbols (void *object, long offset) {

    struct coff_exec *hdr = (struct coff_exec *) object;
    
    unsigned long sym_start = GET_UINT32 (hdr->PointerToSymbolTable);
    unsigned long sym_cnt = GET_UINT32 (hdr->NumberOfSymbols);
    unsigned long string_table_start = sym_start + (sizeof (struct coff_symbol) * sym_cnt);
    
    struct coff_symbol sym;
    
    while (sym_cnt--) {
    
        memcpy (&sym, (char *) object + sym_start, sizeof (sym));
        
        if (sym.StorageClass[0] == 2 && GET_UINT16 (sym.SectionNumber) != 0) {
        
            struct strtab *strtab;
            
            if (sym.Name[0] != 0) {
            
                int i, len;
                
                for (i = 0, len = 0; i < 8; i++) {
                
                    if (sym.Name[i] == '\0') {
                        break;
                    }
                    
                    len++;
                
                }
                
                strtab = xmalloc (sizeof (*strtab));
                strtab->length = len;
                
                strtab->name = xstrdup (sym.Name);
                strtab->offset = offset;
                
                add_strtab (&gstrtab, strtab);
            
            } else {
            
                unsigned char offset1 = (unsigned char) sym.Name[4];
                unsigned char offset2 = (unsigned char) sym.Name[5];
                unsigned char offset3 = (unsigned char) sym.Name[6];
                unsigned char offset4 = (unsigned char) sym.Name[7];
                
                long final_offset = (((long) offset1) | (((long) offset2) << 8) | (((long) offset3) << 16) | (((long) offset4) << 24));
                final_offset += string_table_start;
                
                strtab = xmalloc (sizeof (*strtab));
                strtab->length = strlen ((char *) object + final_offset);
                
                strtab->name = xstrdup ((char *) object + final_offset);
                strtab->offset = offset;
                
                add_strtab (&gstrtab, strtab);
            
            }
        
        }
        
        sym_start += sizeof (sym);
    
    }

}

#define     GET_ELF_UINT16(arr)         (endianess ? (((unsigned long) arr[0] << 8) | (((unsigned long) arr[1]))) : ((unsigned long) arr[0] | (((unsigned long) arr[1]) << 8)))

#define     GET_ELF_UINT32(arr)         (endianess ? (((unsigned long) arr[0] << 24) | (((unsigned long) arr[1]) << 16) | (((unsigned long) arr[2]) << 8) | (((unsigned long) arr[3])))                 \
    : ((unsigned long) arr[0] | (((unsigned long) arr[1]) << 8) | (((unsigned long) arr[2]) << 16) | (((unsigned long) arr[3]) << 24)))

static void elf_get_symbols (void *object, long offset, int endianess) {

    struct elf_exec *hdr = (struct elf_exec *) object;
    
    unsigned long e_shnum = GET_ELF_UINT16 (hdr->e_shnum);
    unsigned long e_shoff = GET_ELF_UINT32 (hdr->e_shoff);
    unsigned long e_shentsize = GET_ELF_UINT16 (hdr->e_shentsize);
    
    unsigned long sh_link, sh_offset, sh_entsize, sh_size;
    unsigned long sym_strtab_size, i, j, st_name;
    
    struct elf_shdr strtabhdr;
    struct elf_shdr shdr;
    struct elf_sym elf_symbol;
    
    struct strtab *strtab;
    char *sym_strtab;
    
    for (i = 1; i < e_shnum; i++) {
    
        memcpy (&shdr, (char *) object + e_shoff + i * e_shentsize, sizeof (shdr));
        
        if (GET_ELF_UINT32 (shdr.sh_type) != 2) {
            continue;
        }
        
        sh_link = GET_ELF_UINT32 (shdr.sh_link);
        sh_offset = GET_ELF_UINT32 (shdr.sh_offset);
        
        if (sh_link == 0 || sh_link >= e_shnum) {
            continue;
        }
        
        memcpy (&strtabhdr, (char *) object + e_shoff + sh_link * e_shentsize, sizeof (strtabhdr));
        
        if (GET_ELF_UINT32 (strtabhdr.sh_type) != 3) {
            continue;
        }
        
        sym_strtab_size = GET_ELF_UINT32 (strtabhdr.sh_size);
        sym_strtab = (char *) object + GET_ELF_UINT32 (strtabhdr.sh_offset);
        
        if ((sh_entsize = GET_ELF_UINT32 (shdr.sh_entsize)) < sizeof (elf_symbol)) {
            continue;
        }
        
        sh_size = GET_ELF_UINT32 (shdr.sh_size);
        
        for (j = 1; j < sh_size / sh_entsize; j++) {
        
            memcpy (&elf_symbol, (char *) object + sh_offset + j * sh_entsize, sizeof (elf_symbol));
            
            if ((st_name = GET_ELF_UINT32 (elf_symbol.st_name)) >= sym_strtab_size) {
                continue;
            }
            
            if (GET_ELF_UINT16 (elf_symbol.st_shndx) == 0 || (elf_symbol.st_info[0] >> 4) != 1) {
                continue;
            }
            
            if (sym_strtab[st_name] != '\0') {
            
                strtab = xmalloc (sizeof (*strtab));
                strtab->offset = offset;
                
                strtab->name = xstrdup (sym_strtab + st_name);
                strtab->length = strlen (strtab->name);
                
                add_strtab (&gstrtab, strtab);
            
            }
        
        }
    
    }

}

unsigned long array_to_integer (unsigned char *arr, int size) {

    unsigned long value = 0;
    int i, j;
    
    for (i = size, j = 0; i > 0; i--, j++) {
        value |= arr[j] << (CHAR_BIT * (i - 1));
    }
    
    return value;

}

static void hunk_get_symbols (void *object, unsigned long bytes, long offset) {

    unsigned char *pos = (unsigned char *) object + 4;
    unsigned long name_size, size;
    
    struct strtab *strtab;
    
    name_size = array_to_integer (pos, 4);
    name_size *= 4;
    
    pos = pos + 4 + name_size;
    
    while (pos < (unsigned char *) object + bytes) {
    
        if (pos[2] == 0x03 && pos[3] == 0xE7){
            break;
        }
        
        if (pos[2] == 0x03 && pos[3] == 0xF2) {
        
            pos += 4;
            continue;
        
        }
        
        if (pos[2] == 0x03 && (pos[3] == 0xE8 || pos[3] == 0xE9 || pos[3] == 0xEA || pos[3] == 0xEB)) {
        
            pos += 4;
            
            if (pos[3] != 0xEB) {
            
                size = array_to_integer (pos, 4);
                size *= 4;
                
                pos = pos + 4 + size;
            
            }
            
            continue;
        
        }
        
        if (pos[2] == 0x03 && pos[3] == 0xEF) {
        
            unsigned char symbol_type;
            unsigned long num_ref;
            
            char *symname;
            pos += 4;
            
            while (1) {
            
                name_size = array_to_integer (pos, 4);
                pos += 4;
                
                if (name_size == 0) {
                    break;
                }
                
                symbol_type = (name_size >> 24) & 0xff;
                
                name_size &= 0xffffff;
                name_size *= 4;
                
                symname = xstrndup ((char *) pos, name_size);
                pos += name_size;
                
                if (symbol_type == 1 || symbol_type == 2) {
                
                    if (symbol_type == 1) {
                    
                        strtab = xmalloc (sizeof (*strtab));
                        strtab->length = strlen (symname);
                        
                        strtab->name = xstrdup (symname);
                        strtab->offset = offset;
                        
                        add_strtab (&gstrtab, strtab);
                    
                    }
                    
                    pos += 4;
                
                } else if (symbol_type == 129 || symbol_type == 130 || symbol_type == 136) {
                
                    if (symbol_type == 130) {
                        pos += 4;
                    }
                    
                    num_ref = array_to_integer (pos, 4);
                    num_ref *= 4;
                    
                    pos = pos + 4 + num_ref;
                
                }
            
            }
            
            continue;
        
        }
        
        if (pos[2] == 0x03 && pos[3] == 0xEC) {
        
            pos += 4;
            
            while (1) {
            
                size = array_to_integer (pos, 4) * 4;
                pos += 4;
                
                if (size == 0) {
                    break;
                }
                
                pos = pos + size + 4;
            
            }
            
            continue;
        
        }
    
    }

}

#define     RECORD_TYPE_EXTDEF          0x8C
#define     RECORD_TYPE_PUBDEF          0x90

static void omf_get_symbols (void *object, char *filename, unsigned long bytes, long offset) {

    unsigned char *pos = (unsigned char *) object;
    unsigned char base_segment_index;
    
    unsigned char record_type;
    unsigned short record_len;
    
    unsigned char *pubdef_name, *pubdef_name_end;
    unsigned char pubdef_name_len;
    
    unsigned char *extdef_name, *extdef_name_end;
    unsigned char extdef_name_len;
    
    struct strtab *strtab;
    int big_fields;
    
    while (pos < (unsigned char *) object + bytes) {
    
        record_type = pos[0];
        
        big_fields = record_type & 1;
        record_type &= ~1;
        
        record_len = GET_UINT16 (pos + 1);
        
        {
        
            unsigned char checksum = 0;
            unsigned long i;
            
            for (i = 0; i < (unsigned long) record_len + 3; i++) {
                checksum += pos[i];
            }
            
            if (checksum != 0) {
                report_at (program_name, 0, REPORT_WARNING, "%s: invalid checksum", filename);
            }
        
        }
        
        pos += 3;
        
        if (record_type == RECORD_TYPE_EXTDEF) {
        
            extdef_name_end = (extdef_name = pos) + record_len - 1;
            
            while (extdef_name != extdef_name_end) {
            
                extdef_name_len = extdef_name[0];
                
                if (extdef_name + 1 + extdef_name_len + 1 > extdef_name_end) {
                
                    report_at (program_name, 0, REPORT_FATAL_ERROR, "%s: incorrect string length", filename);
                    exit (EXIT_FAILURE);
                
                }
                
                strtab = xmalloc (sizeof (*strtab));
                strtab->length = extdef_name_len;
                
                strtab->name = xstrndup ((char *) extdef_name + 1, extdef_name_len);
                strtab->offset = offset;
                
                add_strtab (&gstrtab, strtab);
                extdef_name = extdef_name + 1 + extdef_name_len + 1;
            
            }
        
        } else if (record_type == RECORD_TYPE_PUBDEF) {
        
            unsigned long prefix = 1;
            unsigned long suffix = 2 + 1;
            
            pubdef_name_end = pos + record_len - 1;
            
            if (big_fields) {
                suffix += 2;
            }
            
            base_segment_index = pos[1];
            
            if (!base_segment_index) {
            
                report_at (program_name, 0, REPORT_WARNING, "%s: PUBDEF Base Frame is not supported", filename);
                prefix += 2;
            
            }
            
            pubdef_name = pos + 2;
            
            while (pubdef_name != pubdef_name_end) {
            
                pubdef_name_len = pubdef_name[prefix - 1];
                
                strtab = xmalloc (sizeof (*strtab));
                strtab->length = pubdef_name_len;
                
                strtab->name = xstrndup ((char *) pubdef_name + prefix, pubdef_name_len);
                strtab->offset = offset;
                
                add_strtab (&gstrtab, strtab);
                pubdef_name = pubdef_name + prefix + pubdef_name_len + suffix;
            
            }
        
        }
        
        pos += record_len;
    
    }

}

void ranlib (FILE *arfp) {

    FILE *tfp = tmpfile ();
    
    struct ar_header header;
    long i, j, len, read, val;
    
    unsigned char *object;
    void *contents;
    
    char temp[16];
    long offset = 0;
    
#if     UINT_MAX == 65535
    long bytes;
#else
    int bytes;
#endif
    
    for (;;) {
    
        struct ar_header hdr;
        long bytes, actual_bytes;
        
        if (fread (&hdr, sizeof (hdr), 1, arfp) != 1) {
        
            if (feof (arfp)) {
                break;
            }
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst reading '%s'", state->outfile);
            return;
        
        }
        
        actual_bytes = bytes = conv_dec (hdr.size, 10);
        
        if (bytes % 2) {
            bytes++;
        }
        
        if (memcmp (hdr.name, "/", 1) == 0) {
        
            fseek (arfp, bytes, SEEK_CUR);
            continue;
        
        }
        
        object = xmalloc (bytes);
        
        if (fread (object, bytes, 1, arfp) != 1) {
        
            free (object);
            
            report_at (program_name, 0, REPORT_ERROR, "failed to read %ld bytes from %s", bytes, state->outfile);
            exit (EXIT_FAILURE);
            
        
        }
        
        if (object[2] == 0x03 && object[3] == 0xE7) {
            hunk_get_symbols (object, actual_bytes, offset + 8);
        } else if (object[0] == 0x01 && object[1] == 0x03) {
            elks_get_symbols (object, offset + 8);
        } else if (object[0] == 0x07 && object[1] == 0x01) {
            aout_get_symbols (object, offset + 8);
        } else if ((object[0] == 0x4C && object[1] == 0x01) || (object[0] == 0x64 && object[1] == 0x86)) {
            coff_get_symbols (object, offset + 8);
        } else if (object[0] == 0x7F && memcmp (object + 1, "ELF", 3) == 0) {
            elf_get_symbols (object, offset + 8, object[5] == 2);
        } else if (object[0] == 0x80) {
        
            char filename[17] = { 0 };
            memcpy (filename, hdr.name, 16);
            
            for (i = 16; i >= 0; --i) {
            
                if (filename[i] == ' ') {
                    filename[i] = '\0';
                }
            
            }
            
            omf_get_symbols (object, filename, actual_bytes, offset + 8);
        
        }
        
        free (object);
        
        offset += sizeof (hdr);
        offset += bytes;
    
    }
    
    fseek (arfp, 8, SEEK_SET);
    bytes = 0;
    
    for (i = 0; i < gstrtab.count; ++i) {
        bytes += gstrtab.strtabs[i].length + 5;
    }
    
    for (i = 0; i < gstrtab.count; ++i) {
    
        gstrtab.strtabs[i].offset += bytes;
        
        if (bytes % 2) {
            gstrtab.strtabs[i].offset++;
        }
    
    }
    
    if (fwrite ("!<arch>\n", 8, 1, tfp) != 1) {
    
        fclose (tfp);
        
        report_at (program_name, 0, REPORT_ERROR, "failed whist writing ar header");
        return;
    
    }
    
    memset (temp, 0x20, 16);
    temp[0] = '0';
    
    len = 1;
    memcpy (header.name, "/", len);
    
    while (len < 16) {
        header.name[len++] = 0x20;
    }
    
    memcpy (header.mtime, temp, 12);
    memcpy (header.owner, temp, 6);
    memcpy (header.group, temp, 6);
    memcpy (header.mode, temp, 8);
    
#if     UINT_MAX == 65535
    len = sprintf (temp, "%ld", bytes + 4);
#else
    len = sprintf (temp, "%d", bytes + 4);
#endif
    
    temp[len] = ' ';
    memcpy (header.size, temp, 10);
    
    header.endsig[0] = '`';
    header.endsig[1] = '\n';
    
    if (fwrite (&header, sizeof (header), 1, tfp) != 1) {
    
        fclose (tfp);
        
        report_at (program_name, 0, REPORT_ERROR, "failed whilst writing header");
        return;
    
    }
    
    val = gstrtab.count;
    
    for (i = 0; i < 4; ++i) {
        temp[4 - 1 - i] = (val >> (CHAR_BIT * i)) & UCHAR_MAX;
    }
    
    if (fwrite (temp, 4, 1, tfp) != 1) {
        exit (EXIT_FAILURE);
    }
    
    for (i = 0; i < gstrtab.count; ++i) {
    
        val = gstrtab.strtabs[i].offset + 4 + sizeof (header);
        
        for (j = 0; j < 4; ++j) {
            temp[4 - 1 - j] = (val >> (CHAR_BIT * j)) & UCHAR_MAX;
        }
        
        if (fwrite (temp, 4, 1, tfp) != 1) {
            exit (EXIT_FAILURE);
        }
    
    }
    
    for (i = 0; i < gstrtab.count; ++i) {
    
        const char *name = gstrtab.strtabs[i].name;
        long length = gstrtab.strtabs[i].length;
        
        if (fwrite (name, length, 1, tfp) != 1) {
            exit (EXIT_FAILURE);
        }
        
        temp[0] = '\0';
        
        if (fwrite (temp, 1, 1, tfp) != 1) {
            exit (EXIT_FAILURE);
        }
    
    }
    
    if (bytes % 2) {
    
        temp[0] = '\0';
        
        if (fwrite (temp, 1, 1, tfp) != 1) {
            exit (EXIT_FAILURE);
        }
    
    }
    
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
        
        if (fwrite (&hdr, sizeof (hdr), 1, tfp) != 1) {
        
            fclose (tfp);
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst writing header");
            exit (EXIT_FAILURE);
        
        }
        
        contents = xmalloc (512);
        
        for (;;) {
        
            if (bytes == 0 || feof (arfp)) {
                break;
            } else if (bytes >= 512) {
                read = 512;
            } else {
                read = bytes;
            }
            
            if (fread (contents, read, 1, arfp) != 1) {
            
                free (contents);
                fclose (tfp);
                
                report_at (program_name, 0, REPORT_ERROR, "failed to read %ld bytes from %s", bytes, state->outfile);
                exit (EXIT_FAILURE);
            
            }
            
            bytes -= read;
            
            if (fwrite (contents, read, 1, tfp) != 1) {
            
                free (contents);
                fclose (tfp);
                
                report_at (program_name, 0, REPORT_ERROR, "failed to write temp file");
                exit (EXIT_FAILURE);
            
            }
        
        }
        
        free (contents);
    
    }
    
    fclose (arfp);
    remove (state->outfile);
    
    if ((arfp = fopen (state->outfile, "w+b")) == NULL) {
    
        fclose (tfp);
        
        report_at (program_name, 0, REPORT_ERROR, "failed to open %s for writing", state->outfile);
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
            
            free (contents);
            fclose (tfp);
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst reading temp file");
            return;
        
        }
        
        bytes -= read;
        
        if (fwrite (contents, read, 1, arfp) != 1) {
        
            free (contents);
            fclose (tfp);
            
            report_at (program_name, 0, REPORT_ERROR, "failed whist writing %s", state->outfile);
            exit (EXIT_FAILURE);
        
        }
    
    }
    
    free (contents);
    fclose (tfp);

}
