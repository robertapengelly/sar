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
#include    "stdint.h"

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

static void aout_get_symbols (void *object, int64_t offset) {

    struct aout_exec *hdr = (struct aout_exec *) object;
    
    uint32_t sym_start = sizeof (*hdr) + array_to_integer (hdr->a_text, 4, 0) + array_to_integer (hdr->a_data, 4, 0) + array_to_integer (hdr->a_trsize, 4, 0) + array_to_integer (hdr->a_drsize, 4, 0);
    uint32_t strtab_start = sym_start + array_to_integer (hdr->a_syms, 4, 0);
    
    struct strtab *strtab;
    struct aout_nlist nlist;
    
    while (sym_start < strtab_start) {
    
        memcpy (&nlist, (char *) object + sym_start, sizeof (nlist));
        
        if (nlist.n_type == 5 || nlist.n_type == 7 || nlist.n_type == 9) {
        
            char *symname = (char *) object + strtab_start + array_to_integer (nlist.n_strx, 4, 0);
            
            strtab = xmalloc (sizeof (*strtab));
            strtab->length = strlen (symname);
            
            strtab->name = xstrdup (symname);
            strtab->offset = offset;
            
            add_strtab (&gstrtab, strtab);
        
        }
        
        sym_start += sizeof (nlist);
    
    }

}

static void elks_get_symbols (void *object, int64_t offset) {

    struct elks_exec *hdr = (struct elks_exec *) object;
    
    uint32_t sym_start = sizeof (*hdr) + array_to_integer (hdr->a_text, 4, 0) + array_to_integer (hdr->a_data, 4, 0) + array_to_integer (hdr->a_trsize, 4, 0) + array_to_integer (hdr->a_drsize, 4, 0);
    uint32_t strtab_start = sym_start + array_to_integer (hdr->a_syms, 4, 0);
    
    struct strtab *strtab;
    struct elks_nlist nlist;
    
    while (sym_start < strtab_start) {
    
        memcpy (&nlist, (char *) object + sym_start, sizeof (nlist));
        
        if (nlist.n_type == 5 || nlist.n_type == 7 || nlist.n_type == 9) {
        
            char *symname = (char *) object + strtab_start + array_to_integer (nlist.n_strx, 4, 0);
            
            strtab = xmalloc (sizeof (*strtab));
            strtab->length = strlen (symname);
            
            strtab->name = xstrdup (symname);
            strtab->offset = offset;
            
            add_strtab (&gstrtab, strtab);
        
        }
        
        sym_start += sizeof (nlist);
    
    }

}

static void coff_get_symbols (void *object, int64_t offset) {

    struct coff_exec *hdr = (struct coff_exec *) object;
    
    uint32_t sym_start = array_to_integer (hdr->PointerToSymbolTable, 4, 0);
    uint32_t sym_cnt = array_to_integer (hdr->NumberOfSymbols, 4, 0);
    uint32_t string_table_start = sym_start + (sizeof (struct coff_symbol) * sym_cnt);
    
    struct coff_symbol sym;
    
    while (sym_cnt--) {
    
        memcpy (&sym, (char *) object + sym_start, sizeof (sym));
        
        if (sym.StorageClass[0] == 2 && array_to_integer (sym.SectionNumber, 2, 0) != 0) {
        
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
                
                long final_offset = ((uint32_t) offset1 | (((uint32_t) offset2) << 8) | (((uint32_t) offset3) << 16) | (((uint32_t) offset4) << 24));
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

static void elf32_get_symbols (void *object, int64_t offset, int endianess) {

    struct elf32_exec *hdr = (struct elf32_exec *) object;
    
    uint16_t e_shnum = array_to_integer (hdr->e_shnum, 2, endianess);
    uint32_t e_shoff = array_to_integer (hdr->e_shoff, 4, endianess);
    uint16_t e_shentsize = array_to_integer (hdr->e_shentsize, 2, endianess);
    
    uint32_t sh_link, sh_offset, sh_entsize, sh_size;
    uint32_t sym_strtab_size, i, j, st_name;
    
    struct elf32_shdr strtabhdr;
    struct elf32_shdr shdr;
    struct elf32_sym elf_symbol;
    
    struct strtab *strtab;
    char *sym_strtab;
    
    for (i = 1; i < e_shnum; i++) {
    
        memcpy (&shdr, (char *) object + e_shoff + i * e_shentsize, sizeof (shdr));
        
        if (array_to_integer (shdr.sh_type, 4, endianess) != 2) {
            continue;
        }
        
        sh_link = array_to_integer (shdr.sh_link, 4, endianess);
        sh_offset = array_to_integer (shdr.sh_offset, 4, endianess);
        
        if (sh_link == 0 || sh_link >= e_shnum) {
            continue;
        }
        
        memcpy (&strtabhdr, (char *) object + e_shoff + sh_link * e_shentsize, sizeof (strtabhdr));
        
        if (array_to_integer (strtabhdr.sh_type, 4, endianess) != 3) {
            continue;
        }
        
        sym_strtab_size = array_to_integer (strtabhdr.sh_size, 4, endianess);
        sym_strtab = (char *) object + array_to_integer (strtabhdr.sh_offset, 4, endianess);
        
        if ((sh_entsize = array_to_integer (shdr.sh_entsize, 4, endianess)) < sizeof (elf_symbol)) {
            continue;
        }
        
        sh_size = array_to_integer (shdr.sh_size, 4, endianess);
        
        for (j = 1; j < sh_size / sh_entsize; j++) {
        
            memcpy (&elf_symbol, (char *) object + sh_offset + j * sh_entsize, sizeof (elf_symbol));
            
            if ((st_name = array_to_integer (elf_symbol.st_name, 4, endianess)) >= sym_strtab_size) {
                continue;
            }
            
            if (array_to_integer (elf_symbol.st_shndx, 2, endianess) == 0 || (elf_symbol.st_info[0] >> 4) != 1) {
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

static void elf64_get_symbols (void *object, int64_t offset, int endianess) {

    struct elf64_exec *hdr = (struct elf64_exec *) object;
    
    uint16_t e_shnum = array_to_integer (hdr->e_shnum, 2, endianess);
    uint16_t e_shentsize = array_to_integer (hdr->e_shentsize, 2, endianess);
    
#if     defined (NO_LONG_LONG)
    uint64_t e_shoff = array_to_integer (hdr->e_shoff, 4, endianess);
#else
    uint64_t e_shoff = array_to_integer (hdr->e_shoff, 8, endianess);
#endif
    
    uint64_t sh_link, sh_offset, sh_entsize, sh_size;
    uint64_t sym_strtab_size, i, j, st_name;
    
    struct elf64_shdr strtabhdr;
    struct elf64_shdr shdr;
    struct elf64_sym elf_symbol;
    
    struct strtab *strtab;
    char *sym_strtab;
    
    for (i = 1; i < e_shnum; i++) {
    
        memcpy (&shdr, (char *) object + e_shoff + i * e_shentsize, sizeof (shdr));
        
        if (array_to_integer (shdr.sh_type, 4, endianess) != 2) {
            continue;
        }
        
        sh_link = array_to_integer (shdr.sh_link, 4, endianess);
        
#if     defined (NO_LONG_LONG)
        sh_offset = array_to_integer (shdr.sh_offset, 4, endianess);
#else
        sh_offset = array_to_integer (shdr.sh_offset, 8, endianess);
#endif
        
        if (sh_link == 0 || sh_link >= e_shnum) {
            continue;
        }
        
        memcpy (&strtabhdr, (char *) object + e_shoff + sh_link * e_shentsize, sizeof (strtabhdr));
        
        if (array_to_integer (strtabhdr.sh_type, 4, endianess) != 3) {
            continue;
        }
        
#if     defined (NO_LONG_LONG)
        sym_strtab_size = array_to_integer (strtabhdr.sh_size, 4, endianess);
        sym_strtab = (char *) object + array_to_integer (strtabhdr.sh_offset, 4, endianess);
        
        if ((sh_entsize = array_to_integer (shdr.sh_entsize, 4, endianess)) < sizeof (elf_symbol)) {
            continue;
        }
        
        sh_size = array_to_integer (shdr.sh_size, 4, endianess);
#else
        sym_strtab_size = array_to_integer (strtabhdr.sh_size, 8, endianess);
        sym_strtab = (char *) object + array_to_integer (strtabhdr.sh_offset, 8, endianess);
        
        if ((sh_entsize = array_to_integer (shdr.sh_entsize, 8, endianess)) < sizeof (elf_symbol)) {
            continue;
        }
        
        sh_size = array_to_integer (shdr.sh_size, 8, endianess);
#endif
        
        for (j = 1; j < sh_size / sh_entsize; j++) {
        
            memcpy (&elf_symbol, (char *) object + sh_offset + j * sh_entsize, sizeof (elf_symbol));
            
            if ((st_name = array_to_integer (elf_symbol.st_name, 4, endianess)) >= sym_strtab_size) {
                continue;
            }
            
            if (array_to_integer (elf_symbol.st_shndx, 2, endianess) == 0 || (elf_symbol.st_info[0] >> 4) != 1) {
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

static void hunk_get_symbols (void *object, uint64_t bytes, int64_t offset) {

    unsigned char *pos = (unsigned char *) object + 4;
    uint64_t name_size, size;
    
    struct strtab *strtab;
    
    name_size = array_to_integer (pos, 4, 1);
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
            
                size = array_to_integer (pos, 4, 1);
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
            
                name_size = array_to_integer (pos, 4, 1);
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
                    
                    num_ref = array_to_integer (pos, 4, 1);
                    num_ref *= 4;
                    
                    pos = pos + 4 + num_ref;
                
                }
                
                free (symname);
            
            }
            
            continue;
        
        }
        
        if (pos[2] == 0x03 && pos[3] == 0xEC) {
        
            pos += 4;
            
            while (1) {
            
                size = array_to_integer (pos, 4, 1) * 4;
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

static void omf_get_symbols (void *object, char *filename, uint64_t bytes, int64_t offset) {

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
        
        record_len = array_to_integer (pos + 1, 2, 0);
        
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

    FILE *tfp = get_temp_file ();
    
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
        
            int endianess = (object[5] == 2);

            if (object[4] == 2) {
                elf64_get_symbols (object, offset + 8, endianess);
            } else {
                elf32_get_symbols (object, offset + 8, endianess);
            }
        
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
        
        report_at (program_name, 0, REPORT_ERROR, "failed whilst writing ar header");
        return;
    
    }
    
    memset (temp, ' ', 16);
    temp[0] = '0';
    
    len = 1;
    memcpy (header.name, "/", len);
    
    while (len < 16) {
        header.name[len++] = ' ';
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
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst writing %s", state->outfile);
            exit (EXIT_FAILURE);
        
        }
    
    }
    
    free (contents);
    fclose (tfp);

}
